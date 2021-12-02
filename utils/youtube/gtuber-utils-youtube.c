/*
 * Copyright (C) 2021 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <gtuber/gtuber-plugin-devel.h>

#include "gtuber-utils-youtube.h"
#include "utils/common/gtuber-utils-common.h"

/*
 * gtuber_utils_youtube_parse_mime_type_string:
 *
 * Parse YT mime_type string into #GtuberStreamMimeType and
 * seperate video/audio codecs from it.
 */
void
gtuber_utils_youtube_parse_mime_type_string (const gchar *yt_mime,
    GtuberStreamMimeType *mime_type, gchar **vcodec, gchar **acodec)
{
  gchar **strv;

  strv = g_strsplit (yt_mime, ";", 2);
  if (strv[1]) {
    GHashTable *params;
    gchar *codecs = NULL;

    g_strstrip (strv[1]);
    params = g_uri_parse_params (strv[1], -1, ";", G_URI_PARAMS_WWW_FORM, NULL);

    if (params) {
      codecs = g_strdup (g_hash_table_lookup (params, "codecs"));
      g_hash_table_unref (params);
    }

    if (codecs) {
      *mime_type = gtuber_utils_common_get_mime_type_from_string (strv[0]);

      g_strstrip (g_strdelimit (codecs, "\"", ' '));

      switch (*mime_type) {
        case GTUBER_STREAM_MIME_TYPE_AUDIO_MP4:
        case GTUBER_STREAM_MIME_TYPE_AUDIO_WEBM:
          /* codecs contain only one (audio) codec */
          *acodec = codecs;
          break;
        default:{
          gchar **cstrv;

          cstrv = g_strsplit (codecs, ",", 2);
          if (g_strv_length (cstrv) > 1)
            g_strstrip (cstrv[1]);

          *vcodec = g_strdup (cstrv[0]);
          *acodec = g_strdup (cstrv[1]);

          g_strfreev (cstrv);
          g_free (codecs);
          break;
        }
      }
    }
  }
  g_strfreev (strv);
}

static gboolean
decs_line_is_chapter (const gchar *line, gboolean *has_hours)
{
  if (g_ascii_isdigit (line[0]) && strlen (line) > 6) {
    guint i;

    /* Possible time formats:
     * 0:00, 00:00, 0:00:00, 00:00:00 */
    for (i = 1; i <= 2; i++) {
      if (line[i] == ':'
          && g_ascii_isdigit (line[i + 1])
          && g_ascii_isdigit (line[i + 2])) {
        if (line[i + 3] == ' ') {
          *has_hours = FALSE;
          return TRUE;
        } else if (line[i + 3] == ':'
            && g_ascii_isdigit (line[i + 4])
            && g_ascii_isdigit (line[i + 5])
            && line[i + 6] == ' ') {
          *has_hours = TRUE;
          return TRUE;
        }
      } else if (!g_ascii_isdigit (line[i])) {
        break;
      }
    }
  }

  return FALSE;
}

/*
 * gtuber_utils_youtube_insert_chapters_from_description:
 *
 * Parse YT video description string and add all found
 * video chapters into #GtuberMediaInfo from it.
 */
void
gtuber_utils_youtube_insert_chapters_from_description (GtuberMediaInfo *info,
    const gchar *description)
{
  gchar **strv;
  gboolean inserted = FALSE;
  gboolean has_hours;
  guint index = 0;

  g_return_if_fail (description != NULL);
  g_return_if_fail (GTUBER_IS_MEDIA_INFO (info));

  g_debug ("Inserting YT chapters");
  strv = g_strsplit (description, "\n", 0);

  while (strv[index]) {
    gchar **chapter_strv;

    if (!decs_line_is_chapter (strv[index], &has_hours)) {
      if (inserted) {
        g_debug ("No more chapters");
        break;
      }

      index++;
      continue;
    }

    chapter_strv = g_strsplit (strv[index], " ", 2);

    if (chapter_strv[0] && chapter_strv[1]) {
      guint pos = 0, hours, minutes, seconds;
      guint64 total = 0;
      gint offset = 0;

      if (has_hours) {
        hours = g_ascii_strtoull (chapter_strv[0], NULL, 10);
        pos += 2;
        if ((chapter_strv[0])[pos] == ':')
          pos++;
        total += hours * 60 * 60 * 1000;
      }
      minutes = g_ascii_strtoull (chapter_strv[0] + pos, NULL, 10);
      pos += 2;
      if ((chapter_strv[0])[pos] == ':')
        pos++;
      seconds = g_ascii_strtoull (chapter_strv[0] + pos, NULL, 10);

      total += minutes * 60 * 1000;
      total += seconds * 1000;

      if (G_UNLIKELY (
          (chapter_strv[1])[0] == '-'
          && (chapter_strv[1])[1] == ' '))
        offset = 2;

      g_debug ("Inserting YT chapter, %lu: %s", total, chapter_strv[1] + offset);
      gtuber_media_info_insert_chapter (info, total, chapter_strv[1] + offset);

      /* Inserted something, break on next non-chapter string */
      inserted = TRUE;
    }

    g_strfreev (chapter_strv);
    index++;
  }

  g_strfreev (strv);

  if (inserted)
    g_debug ("Finished inserting YT chapters");
  else
    g_debug ("No YT chapters found");
}
