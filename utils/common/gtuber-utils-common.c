/*
 * Copyright (C) 2021 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#include <math.h>
#include <gtuber/gtuber-plugin-devel.h>

#include "gtuber-utils-common.h"

static const gchar *
get_parsed_host (const gchar *host)
{
  guint offset = 0;

  /* Skip common prefixes */
  if (g_str_has_prefix (host, "www."))
    offset = 4;
  else if (g_str_has_prefix (host, "m."))
    offset = 2;

  return host + offset;
}

/**
 * gtuber_utils_common_uri_matches_hosts:
 * @uri: a #GUri
 * @match: (out) (optional): index of host that matched
 *   or -1 when match was not found
 * @search_host: possible supported host name
 * @...: arguments, as per @search_host
 *
 * A convenience function that checks if host is matched
 * with any of the provided possibilities.
 *
 * Returns: %TRUE if host was matched, %FALSE otherwise.
 */
gboolean
gtuber_utils_common_uri_matches_hosts (GUri *uri, gint *match, const gchar *search_host, ...)
{
  va_list args;
  guint index = 0;
  gboolean found = FALSE;
  const gchar *host;

  host = g_uri_get_host (uri);
  if (host) {
    const gchar *parsed_host;

    parsed_host = get_parsed_host (host);

    va_start (args, search_host);
    while (search_host) {
      if ((found = strcmp (parsed_host, search_host) == 0))
        break;

      search_host = va_arg (args, const gchar *);
      index++;
    }
    va_end (args);
  }

  if (match)
    *match = found ? index : -1;

  return found;
}

gboolean
gtuber_utils_common_uri_matches_hosts_array (GUri *uri, gint *match, const gchar *const *hosts)
{
  guint index = 0;
  gboolean found = FALSE;
  const gchar *host;

  host = g_uri_get_host (uri);
  if (host) {
    const gchar *parsed_host;

    parsed_host = get_parsed_host (host);

    for (index = 0; hosts[index]; index++) {
      if ((found = strcmp (parsed_host, hosts[index]) == 0))
        break;
    }
  }

  if (match)
    *match = found ? index : -1;

  return found;
}

/**
 * gtuber_utils_common_obtain_uri_id_from_paths:
 * @uri: a #GUri
 * @match: (out) (optional): number of path that matched
 *   or zero when match not found
 * @search_path: expected path before ID
 * @...: arguments, as per @search_path
 *
 * A convenience function that obtains the video ID hidden inside
 * URI path. When providing multiple possibilities that start with
 * the same path, sort them from longest to shortest path.
 * If part of the path is irrevelant use an asterisk (*) wildcard.
 *
 * Every provided path must end with "/" character.
 *
 * Returns: (transfer full): the extracted ID or %NULL.
 */
gchar *
gtuber_utils_common_obtain_uri_id_from_paths (GUri *uri, gint *match, const gchar *search_path, ...)
{
  va_list args;
  guint index = -1;
  gchar *video_id = NULL;
  gchar **path_parts;
  const gchar *path = g_uri_get_path (uri);

  g_debug ("Identifying ID from path: %s", path);
  path_parts = g_strsplit (path, "/", 0);

  va_start (args, search_path);
  while (search_path && !video_id) {
    gchar **search_parts;
    guint i = 0;

    index++;
    search_parts = g_strsplit (search_path, "/", 0);

    while (path_parts[i] && search_parts[i]) {
      if (strcmp (search_parts[i], "*")
          && strcmp (path_parts[i], search_parts[i])) {
        if (!search_parts[i + 1])
          video_id = g_strdup (path_parts[i]);

        break;
      }
      i++;
    }
    g_strfreev (search_parts);
    search_path = va_arg (args, const gchar *);
  }
  va_end (args);
  g_strfreev (path_parts);

  g_debug ("Identified ID: %s", video_id);

  if (match)
    *match = video_id ? index : -1;

  return video_id;
}

gchar *
gtuber_utils_common_obtain_uri_query_value (GUri *uri, const gchar *key)
{
  const gchar *query;
  gchar *value;
  GHashTable *params;

  query = g_uri_get_query (uri);
  if (!query)
    return NULL;

  params = g_uri_parse_params (query, -1,
      "&", G_URI_PARAMS_NONE, NULL);

  value = g_strdup (g_hash_table_lookup (params, key));
  g_hash_table_unref (params);

  return value;
}

gchar *
gtuber_utils_common_obtain_uri_with_query_as_path (const gchar *uri_str)
{
  GUri *uri;
  GUriParamsIter iter;
  gchar *attr, *value, *query_path, *mod_path, *mod_uri;
  GString *string;

  string = g_string_new ("");
  uri = g_uri_parse (uri_str, G_URI_FLAGS_ENCODED, NULL);

  g_uri_params_iter_init (&iter, g_uri_get_query (uri),
      -1, "&", G_URI_PARAMS_NONE);

  while (g_uri_params_iter_next (&iter, &attr, &value, NULL)) {
    gchar *esc_attr, *esc_value;

    esc_attr = g_uri_escape_string (attr, NULL, TRUE);
    g_free (attr);

    esc_value = g_uri_escape_string (value, NULL, TRUE);
    g_free (value);

    g_string_append_printf (string, "/%s/%s", esc_attr, esc_value);
    g_free (esc_attr);
    g_free (esc_value);
  }

  query_path = g_string_free (string, FALSE);
  mod_path = g_build_path ("/", g_uri_get_path (uri), query_path, NULL);

  g_uri_unref (uri);
  g_free (query_path);

  mod_uri = g_uri_resolve_relative (uri_str, mod_path,
      G_URI_FLAGS_ENCODED, NULL);
  g_free (mod_path);

  return mod_uri;
}

gchar *
gtuber_utils_common_obtain_uri_source (GUri *uri)
{
  gchar *uri_str, *source;

  uri_str = g_uri_to_string_partial (uri, G_URI_HIDE_QUERY | G_URI_HIDE_FRAGMENT);
  source = g_uri_resolve_relative (uri_str, "/", G_URI_FLAGS_ENCODED, NULL);

  g_free (uri_str);

  return source;
}

gchar *
gtuber_utils_common_replace_uri_source (const gchar *uri_str, const gchar *src_uri_str)
{
  GUri *uri, *src_uri, *mod_uri;
  gchar *mod_uri_str;

  uri = g_uri_parse (uri_str, G_URI_FLAGS_ENCODED, NULL);
  if (!uri)
    return NULL;

  src_uri = g_uri_parse (src_uri_str, G_URI_FLAGS_ENCODED, NULL);
  if (!src_uri) {
    g_uri_unref (uri);
    return NULL;
  }

  mod_uri = g_uri_build (G_URI_FLAGS_ENCODED,
      g_uri_get_scheme (src_uri),
      g_uri_get_userinfo (src_uri),
      g_uri_get_host (src_uri),
      g_uri_get_port (src_uri),
      g_uri_get_path (uri),
      g_uri_get_query (uri),
      g_uri_get_fragment (uri));

  mod_uri_str = g_uri_to_string (mod_uri);

  g_uri_unref (uri);
  g_uri_unref (src_uri);
  g_uri_unref (mod_uri);

  return mod_uri_str;
}

gchar *
gtuber_utils_common_input_stream_to_data (GInputStream *stream, GError **error)
{
  GOutputStream *ostream;
  gchar *data = NULL;

  ostream = g_memory_output_stream_new_resizable ();
  if (g_output_stream_splice (ostream, stream,
      G_OUTPUT_STREAM_SPLICE_CLOSE_SOURCE | G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
      NULL, error) != -1) {
    data = g_memory_output_stream_steal_data (G_MEMORY_OUTPUT_STREAM (ostream));
  }
  g_object_unref (ostream);

  if (!data && *error == NULL) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Could not convert input stream to data");
  }

  return data;
}

/**
 * gtuber_utils_common_msg_take_request:
 * @msg: a #SoupMessage
 * @content_type: MIME Content-Type of the body, or NULL if unknown
 * @req_body: (transfer full): request body
 *
 * Set the request body of a SoupMessage from string.
 *
 * Takes ownership of passed string and frees it automatically
 * when done with it.
 */
void
gtuber_utils_common_msg_take_request (SoupMessage *msg,
    const gchar *content_type, gchar *req_body)
{
  GInputStream *stream;

  stream = g_memory_input_stream_new_from_data (req_body,
      strlen (req_body), (GDestroyNotify) g_free);
  soup_message_set_request_body (msg, content_type,
      stream, strlen (req_body));

  g_object_unref (stream);
}

/**
 * gtuber_utils_common_get_mime_type_from_string:
 * @string: a null-terminated string
 *
 * Returns: a #GtuberStreamMimeType with
 *   detected MIME type from the string.
 */
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
  HLS_PARAM_URI,
  HLS_PARAM_GROUP_ID,
  HLS_PARAM_AUDIO
} HlsParamType;

static HlsParamType
get_hls_param_type (const gchar *param)
{
  if (!strcmp (param, "BANDWIDTH") || !strcmp (param, "AVERAGE-BANDWIDTH"))
    return HLS_PARAM_BANDWIDTH;
  if (!strcmp (param, "RESOLUTION"))
    return HLS_PARAM_RESOLUTION;
  if (!strcmp (param, "FRAME-RATE"))
    return HLS_PARAM_FRAME_RATE;
  if (!strcmp (param, "CODECS"))
    return HLS_PARAM_CODECS;
  if (!strcmp (param, "URI"))
    return HLS_PARAM_URI;
  if (!strcmp (param, "GROUP-ID"))
    return HLS_PARAM_GROUP_ID;
  if (!strcmp (param, "AUDIO"))
    return HLS_PARAM_AUDIO;

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
  return gtuber_utils_common_parse_hls_input_stream_with_base_uri (stream, info, NULL, error);
}

static void
_unquote_str (gchar *string)
{
  gsize index = strlen (string) - 1;

  if (string[index] == '"')
    string[index] = '\0';

  if (string[0] == '"')
    memmove (string, string + 1, strlen (string));
}

gboolean
gtuber_utils_common_parse_hls_input_stream_with_base_uri (GInputStream *stream,
    GtuberMediaInfo *info, const gchar *base_uri, GError **error)
{
  GDataInputStream *dstream = g_data_input_stream_new (stream);
  GtuberAdaptiveStream *astream = NULL;
  gchar *line;
  guint itag = 1;
  gboolean success = FALSE;

  g_debug ("Parsing HLS...");

  while ((line = g_data_input_stream_read_line (dstream, NULL, NULL, error))) {
    guint line_offset = 0;

    if (g_str_has_prefix (line, "#EXT-X-MEDIA:"))
      line_offset = 13;
    else if (g_str_has_prefix (line, "#EXT-X-STREAM-INF:"))
      line_offset = 18;

    if (line_offset > 0) {
      gchar **params = g_strsplit_set (line + line_offset, ",=", 0);
      gchar *str;
      gint i = 0;
      guint group_itag = 0;
      HlsParamType last = HLS_PARAM_NONE;
      GtuberStream *bstream;

      if (!astream) {
        astream = gtuber_adaptive_stream_new ();

        gtuber_adaptive_stream_set_manifest_type (astream,
            GTUBER_ADAPTIVE_STREAM_MANIFEST_HLS);
        gtuber_stream_set_itag ((GtuberStream *) astream, itag);

        g_debug ("Created new adaptive stream, itag: %u", itag);
      }

      bstream = GTUBER_STREAM (astream);

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

        _unquote_str (str);

        switch (last) {
          case HLS_PARAM_BANDWIDTH:{
            guint old_bitrate, bitrate;

            old_bitrate = gtuber_stream_get_bitrate (bstream);
            bitrate = g_ascii_strtoull (str, NULL, 10);

            /* Use average bitrate if available */
            if (old_bitrate == 0 || old_bitrate > bitrate) {
              g_debug ("HLS stream bitrate: %s", str);
              gtuber_stream_set_bitrate (bstream, bitrate);
            }
            break;
          }
          case HLS_PARAM_RESOLUTION:{
            gchar **resolution = g_strsplit (str, "x", 3);
            if (resolution[0] && resolution[1]) {
              g_debug ("HLS stream width: %s, height: %s", resolution[0], resolution[1]);
              gtuber_stream_set_width (bstream, g_ascii_strtoull (resolution[0], NULL, 10));
              gtuber_stream_set_height (bstream, g_ascii_strtoull (resolution[1], NULL, 10));
            }
            g_strfreev (resolution);
            break;
          }
          case HLS_PARAM_FRAME_RATE:{
            guint fps = round (g_ascii_strtod (str, NULL));
            g_debug ("HLS stream fps: %i", fps);
            gtuber_stream_set_fps (bstream, fps);
            break;
          }
          case HLS_PARAM_CODECS:
            if (!get_is_audio_codec (str)) {
              g_debug ("HLS stream video codec: %s", str);
              gtuber_stream_set_video_codec (bstream, str);
            } else {
              g_debug ("HLS stream audio codec: %s", str);
              gtuber_stream_set_audio_codec (bstream, str);
            }
            break;
          case HLS_PARAM_URI:
            g_free (line);
            line = g_strdup (str);
            break;
          case HLS_PARAM_GROUP_ID:
          case HLS_PARAM_AUDIO:{
            if (group_itag == 0)
              group_itag = g_str_hash (str);
            if (last == HLS_PARAM_GROUP_ID) {
              g_debug ("Replaced itag from GROUP-ID: %u", group_itag);
              gtuber_stream_set_itag (bstream, group_itag);
            }
            break;
          }
          default:
            break;
        }
        i++;
      }
      g_strfreev (params);

      /* Move audio codec from video-only to audio-only stream */
      if (group_itag > 0
          && gtuber_stream_get_video_codec (bstream) != NULL
          && gtuber_stream_get_audio_codec (bstream) != NULL) {
        GPtrArray *astreams;
        guint j;

        astreams = gtuber_media_info_get_adaptive_streams (info);

        for (j = 0; j < astreams->len; j++) {
          GtuberAdaptiveStream *tmp_astream;
          GtuberAdaptiveStreamManifest manifest_type;

          tmp_astream = g_ptr_array_index (astreams, j);
          manifest_type = gtuber_adaptive_stream_get_manifest_type (tmp_astream);

          /* We might already have some non-HLS adaptive streams
           * added by website plugin, make sure to update HLS only */
          if (manifest_type != GTUBER_ADAPTIVE_STREAM_MANIFEST_HLS
              || gtuber_stream_get_itag ((GtuberStream *) tmp_astream) != group_itag)
            continue;

          gtuber_stream_set_audio_codec ((GtuberStream *) tmp_astream,
              gtuber_stream_get_audio_codec (bstream));
          gtuber_stream_set_audio_codec (bstream, NULL);
        }
      }
    }
    if (astream && !g_str_has_prefix (line, "#")) {
      gboolean duplicate = FALSE;

      if (base_uri) {
        gchar *full_uri;

        if (!g_uri_is_valid (line, G_URI_FLAGS_ENCODED, NULL)) {
          full_uri = g_uri_resolve_relative (base_uri, line,
              G_URI_FLAGS_ENCODED, NULL);
        } else {
          full_uri = gtuber_utils_common_replace_uri_source (line, base_uri);
        }
        g_debug ("Resolved URI: %s", full_uri);

        if (full_uri) {
          g_free (line);
          line = full_uri;
        }
      }

      /* HLS streams with seperate audio might have URI duplicates,
       * due to possible multiple video+audio combinations.
       * Lets simply drop them, so they will not appear twice */
      if (!gtuber_stream_get_audio_codec ((GtuberStream *) astream)) {
        GPtrArray *astreams;
        guint j;

        g_debug ("Checking for duplicated URIs...");
        astreams = gtuber_media_info_get_adaptive_streams (info);

        for (j = 0; j < astreams->len; j++) {
          GtuberStream *tmp_stream;
          const gchar *present_uri;

          tmp_stream = (GtuberStream *) g_ptr_array_index (astreams, j);
          present_uri = gtuber_stream_get_uri (tmp_stream);

          if ((duplicate = strcmp (present_uri, line) == 0))
            break;
        }
        g_debug ("Duplicated URIs found: %s", duplicate ? "yes" : "no");
      }

      g_debug ("%s adaptive stream, itag: %u",
          duplicate ? "Dropped duplicated" : "Added",
          gtuber_stream_get_itag ((GtuberStream *) astream));

      if (!duplicate) {
        gtuber_stream_set_uri ((GtuberStream *) astream, line);
        g_debug ("HLS stream URI: %s", line);

        gtuber_media_info_add_adaptive_stream (info, astream);
      } else {
        g_object_unref (astream);
      }

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

  if (!success && *error == NULL) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Could not extract adaptive streams from HLS");
  }

  return success;
}
