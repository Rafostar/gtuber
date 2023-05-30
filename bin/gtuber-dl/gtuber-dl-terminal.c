/*
 * Copyright (C) 2023 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <gst/gst.h>
#include <stdio.h>

#include "gtuber-dl-terminal.h"

#define TERM_CODE(c) "\033[" c "m"
#define TERM_RESET   "\x1b[0m"

#define TERM_BOLD    TERM_CODE("1")
#define TERM_RED     TERM_CODE("91")
#define TERM_GREEN   TERM_CODE("92")
#define TERM_YELLOW  TERM_CODE("93")
#define TERM_BLUE    TERM_CODE("94")
#define TERM_MAGENTA TERM_CODE("95")
#define TERM_CYAN    TERM_CODE("96")

static const gchar *table_columns[] = {
  "Itag",
  "Type",
  "Resolution",
  "FPS",
  "Bitrate",
  "Video Codec",
  "Audio Codec",
  NULL
};

static inline void
_append_top_down_border (GString *string, guint table_width)
{
  guint i, count, rem;

  count = (table_width - 2) / 10;
  rem = (table_width - 2) % 10;

  /* XXX: Appending larger chunks at once should be faster */
  for (i = 0; i < count; ++i)
    g_string_append (string, "──────────");
  for (i = 0; i < rem; ++i)
    g_string_append (string, "─");
}

static guint
calculate_table_width (void)
{
  guint i, table_width = 2;

  for (i = 0; table_columns[i]; ++i)
    table_width += (strlen (table_columns[i]) + 4);

  return table_width;
}

static void
append_top (GString *string, guint table_width)
{
  g_string_append (string, "┌");
  _append_top_down_border (string, table_width);
  g_string_append (string, "┐\n");
}

static void
append_bottom (GString *string, guint table_width)
{
  g_string_append (string, "└");
  _append_top_down_border (string, table_width);
  g_string_append (string, "┘");
}

static void
append_title (GString *string, GtuberMediaInfo *info, guint table_width)
{
  GString *title_string = g_string_new (NULL);
  const gchar *title = NULL;
  gchar *custom_title;
  gint i, free_space = table_width - 4; // Subtract border and space on each side

  if (!(title = gtuber_media_info_get_title (info)))
    title = gtuber_media_info_get_id (info);

  g_string_append (string, "│" TERM_BOLD TERM_MAGENTA " ");

  if (G_LIKELY (g_utf8_validate (title, -1, NULL))) {
    const gchar *walk = title;

    while (*walk) {
      gunichar wc = g_utf8_get_char (walk);
      guint wc_width = (g_unichar_iswide (wc)) ? 2 : 1;

      if (free_space <= wc_width) {
        g_string_append (title_string, "…");
        free_space--;
        break;
      }

      g_string_append_unichar (title_string, wc);
      free_space -= wc_width;

      walk = g_utf8_next_char (walk);
    }
  }

  if (free_space > 0) {
    gint spacer = free_space / 2;
    gint rem = free_space % 2;

    for (i = 0; i < spacer; ++i)
      g_string_prepend (title_string, " ");
    for (i = 0; i < spacer; ++i)
      g_string_append (title_string, " ");
    if (rem == 1)
      g_string_append (title_string, " ");
  }

  custom_title = g_string_free (title_string, FALSE);
  g_string_append (string, custom_title);
  g_free (custom_title);

  g_string_append (string, " " TERM_RESET "│\n");
}

static void
append_columns (GString *string)
{
  guint i;

  g_string_append (string, "├─");
  for (i = 0; table_columns[i]; ++i) {
    g_string_append_printf (string, TERM_YELLOW " %-*s " TERM_RESET,
        (guint) strlen (table_columns[i]), table_columns[i]);

    if (table_columns[i + 1])
      g_string_append (string, "──");
  }
  g_string_append (string, "─┤\n");
}

static void
append_stream (GString *string, GtuberStream *stream)
{
  guint itag, width, height, fps, bitrate;
  const gchar *vcodec, *acodec;
  gboolean has_video, has_audio;
  guint column;

  itag = gtuber_stream_get_itag (stream);
  width = gtuber_stream_get_width (stream);
  height = gtuber_stream_get_height (stream);
  fps = gtuber_stream_get_fps (stream);
  bitrate = gtuber_stream_get_bitrate (stream);
  vcodec = gtuber_stream_get_video_codec (stream);
  acodec = gtuber_stream_get_audio_codec (stream);

  has_video = (width > 0 || height > 0 || fps > 0 || vcodec != NULL);
  has_audio = (acodec != NULL);

  g_string_append (string, "│");

  for (column = 0; table_columns[column]; ++column) {
    guint row_width = strlen (table_columns[column]) + 1;

    if (column > 0)
      row_width += 1;

    switch (column) {
      case 0:{
        const gchar *itag_color = (!has_video)
            ? TERM_CYAN
            : (!has_audio)
            ? TERM_GREEN
            : TERM_RED;
        g_string_append_printf (string, "%s %*u " TERM_RESET, itag_color, row_width, itag);
        break;
      }
      case 1:{
        const gchar *type = "HTTPS";
        if (GTUBER_IS_ADAPTIVE_STREAM (stream)) {
          GtuberAdaptiveStreamManifest manifest =
              gtuber_adaptive_stream_get_manifest_type (GTUBER_ADAPTIVE_STREAM (stream));

          switch (manifest) {
            case GTUBER_ADAPTIVE_STREAM_MANIFEST_DASH:
              type = "DASH";
              break;
            case GTUBER_ADAPTIVE_STREAM_MANIFEST_HLS:
              type = "HLS";
              break;
            default:
              type = "?";
              break;
          }
        }
        g_string_append_printf (string, " %*s ", row_width, type);
        break;
      }
      case 2:{
        gchar *resolution = (height > 0 && width > 0)
            ? g_strdup_printf ("%ux%u", width, height)
            : (height > 0)
            ? g_strdup_printf ("%u", height)
            : NULL;
        g_string_append_printf (string, " %*s ", row_width, (resolution) ? resolution : "-");
        g_free (resolution);
        break;
      }
      case 3:{
        if (fps > 0)
          g_string_append_printf (string, " %*u ", row_width, fps);
        else
          g_string_append_printf (string, " %*s ", row_width, "-");
        break;
      }
      case 4:
        g_string_append_printf (string, " %*u ", row_width, bitrate);
        break;
      case 5:
        g_string_append_printf (string, " %*.*s ", row_width, row_width, (vcodec) ? vcodec : "-");
        break;
      case 6:
        g_string_append_printf (string, " %*.*s ", row_width, row_width, (acodec) ? acodec : "-");
        break;
      default:
        /* Fill any unimplemented column with question marks */
        g_string_append_printf (string, " %*s ", row_width, "?");
        break;
    }
  }

  g_string_append (string, " " TERM_RESET "│\n");
}

void
gtuber_dl_terminal_print_formats (GtuberMediaInfo *info)
{
  GPtrArray *streams, *astreams;
  GString *string;
  gchar *table;
  guint table_width, i;

  table_width = calculate_table_width ();

  streams = gtuber_media_info_get_streams (info);
  astreams = gtuber_media_info_get_adaptive_streams (info);
  string = g_string_new (NULL);

  append_top (string, table_width);
  append_title (string, info, table_width);
  append_columns (string);

  for (i = 0; i < streams->len; ++i)
    append_stream (string, GTUBER_STREAM (g_ptr_array_index (streams, i)));

  for (i = 0; i < astreams->len; ++i)
    append_stream (string, GTUBER_STREAM (g_ptr_array_index (astreams, i)));

  append_bottom (string, table_width);

  table = g_string_free (string, FALSE);
  gst_println ("%s", table);
  g_free (table);
}

gchar *
gtuber_dl_terminal_read_itags (void)
{
  gchar *itags = NULL;
  gsize len = 0;

  gst_print ("Selected Itags: ");
  if ((getline (&itags, &len, stdin) == -1) || len == 0)
    g_clear_pointer (&itags, g_free);

  return itags;
}
