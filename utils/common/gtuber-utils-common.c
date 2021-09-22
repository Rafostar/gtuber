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

#include "gtuber-utils-common.h"
#include "gtuber/gtuber-plugin-devel.h"

/**
 * gtuber_utils_common_get_mime_type_from_string:
 * @string: a null-terminated string
 *
 * Returns: a #GtuberStreamMimeType with
 *   detected MIME type from the string.
 **/
GtuberStreamMimeType
gtuber_utils_common_get_mime_type_from_string (const gchar *string)
{
  gboolean is_mp4, is_webm;

  is_mp4 = g_str_has_suffix (string, "mp4");
  is_webm = is_mp4 ? FALSE : g_str_has_suffix (string, "webm");

  if (!is_mp4 && !is_webm)
    goto unknown;

  if (g_str_has_prefix (string, "video")) {
    if (is_mp4)
      return GTUBER_STREAM_MIME_TYPE_VIDEO_MP4;
    if (is_webm)
      return GTUBER_STREAM_MIME_TYPE_VIDEO_WEBM;
  } else if (g_str_has_prefix (string, "audio")) {
    if (is_mp4)
      return GTUBER_STREAM_MIME_TYPE_AUDIO_MP4;
    if (is_webm)
      return GTUBER_STREAM_MIME_TYPE_AUDIO_WEBM;
  }

unknown:
  return GTUBER_STREAM_MIME_TYPE_UNKNOWN;
}

typedef enum
{
  HLS_PARAM_NONE,
  HLS_PARAM_UNSUPPORTED,
  HLS_PARAM_BANDWIDTH,
  HLS_PARAM_RESOLUTION,
  HLS_PARAM_FRAME_RATE,
  HLS_PARAM_CODECS,
} HlsParamType;

static HlsParamType
get_hls_param_type (const gchar *param)
{
  if (!strcmp (param, "BANDWIDTH"))
    return HLS_PARAM_BANDWIDTH;
  if (!strcmp (param, "RESOLUTION"))
    return HLS_PARAM_RESOLUTION;
  if (!strcmp (param, "FRAME-RATE"))
    return HLS_PARAM_FRAME_RATE;
  if (!strcmp (param, "CODECS"))
    return HLS_PARAM_CODECS;

  return (g_ascii_isupper (param[0]))
      ? HLS_PARAM_UNSUPPORTED
      : HLS_PARAM_NONE;
}

static gboolean
get_is_audio_codec (const gchar *codec)
{
  if (g_str_has_prefix (codec, "mp4a"))
    return TRUE;

  return FALSE;
}

/**
 * gtuber_utils_common_parse_hls_input_stream:
 * @stream: a #GInputStream
 * @info: a #GtuberMediaInfo
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * A convenience function that reads #GInputStream pointing to HLS manifest
 *   and fills #GtuberMediaInfo with #GtuberAdaptiveStream(s) from it.
 *
 * Returns: %TRUE if info was successfully updated, %FALSE otherwise.
 */
gboolean
gtuber_utils_common_parse_hls_input_stream (GInputStream *stream,
    GtuberMediaInfo *info, GError **error)
{
  GDataInputStream *dstream = g_data_input_stream_new (stream);
  GtuberAdaptiveStream *astream = NULL;
  gchar *line;
  guint itag = 1;
  gboolean success = FALSE;

  g_debug ("Parsing HLS...");

  while ((line = g_data_input_stream_read_line (dstream, NULL, NULL, error))) {
    if (g_str_has_prefix (line, "#EXT-X-STREAM-INF:")) {
      gchar **params = g_strsplit_set (line + 18, ",=\"", 0);
      const gchar *str;
      gint i = 0;
      HlsParamType last = HLS_PARAM_NONE;

      astream = gtuber_adaptive_stream_new ();
      gtuber_stream_set_itag (GTUBER_STREAM_CAST (astream), itag);

      g_debug ("Created new adaptive stream, itag: %i", itag);

      while ((str = params[i])) {
        HlsParamType current;

        if (!strlen (str)) {
          i++;
          continue;
        }

        current = get_hls_param_type (str);
        if (current != HLS_PARAM_NONE) {
          last = current;
          i++;
          continue;
        }

        switch (last) {
          case HLS_PARAM_BANDWIDTH:
            g_debug ("HLS stream bitrate: %s", str);
            gtuber_stream_set_bitrate (GTUBER_STREAM_CAST (astream), atoi (str));
            break;
          case HLS_PARAM_RESOLUTION:{
            gchar **resolution = g_strsplit (str, "x", 3);
            if (resolution[0] && resolution[1]) {
              g_debug ("HLS stream width: %s, height: %s", resolution[0], resolution[1]);
              gtuber_stream_set_width (GTUBER_STREAM_CAST (astream), atoi (resolution[0]));
              gtuber_stream_set_height (GTUBER_STREAM_CAST (astream), atoi (resolution[1]));
            }
            g_strfreev (resolution);
            break;
          }
          case HLS_PARAM_FRAME_RATE:
            g_debug ("HLS stream fps: %s", str);
            gtuber_stream_set_fps (GTUBER_STREAM_CAST (astream), atoi (str));
            break;
          case HLS_PARAM_CODECS:
            if (!get_is_audio_codec (str)) {
              g_debug ("HLS stream video codec: %s", str);
              gtuber_stream_set_video_codec (GTUBER_STREAM_CAST (astream), str);
            } else {
              g_debug ("HLS stream audio codec: %s", str);
              gtuber_stream_set_audio_codec (GTUBER_STREAM_CAST (astream), str);
            }
            break;
          default:
            break;
        }
        i++;
      }
      g_strfreev (params);
    } else if (astream && !g_str_has_prefix (line, "#")) {
      gtuber_stream_set_uri (GTUBER_STREAM_CAST (astream), line);
      g_debug ("HLS stream URI: %s", line);

      gtuber_media_info_add_adaptive_stream (info, astream);
      astream = NULL;
      success = TRUE;

      itag++;
    }
    g_free (line);
  }
  g_debug ("HLS parsing %ssuccessful", success ? "" : "un");

  /* When stream not added */
  if (astream)
    g_object_unref (astream);

  g_object_unref (dstream);

  if (!success && !*error) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Could not extract adaptive streams from HLS");
  }

  return success;
}
