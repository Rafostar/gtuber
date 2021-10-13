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

#include <json-glib/json-glib.h>

#include "gtuber-youtube.h"
#include "utils/common/gtuber-utils-common.h"
#include "utils/json/gtuber-utils-json.h"

struct _GtuberYoutube
{
  GtuberWebsite parent;

  gchar *video_id;
  gchar *hls_uri;

  gchar *visitor_data;
  gchar *client_version;
  gchar *locale;

  guint try_count;
};

struct _GtuberYoutubeClass
{
  GtuberWebsiteClass parent_class;
};

#define parent_class gtuber_youtube_parent_class
G_DEFINE_TYPE (GtuberYoutube, gtuber_youtube, GTUBER_TYPE_WEBSITE)

static void gtuber_youtube_finalize (GObject *object);

static GtuberFlow gtuber_youtube_create_request (GtuberWebsite *website,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error);
static GtuberFlow gtuber_youtube_parse_input_stream (GtuberWebsite *website,
    GInputStream *stream, GtuberMediaInfo *info, GError **error);

static void
gtuber_youtube_init (GtuberYoutube *self)
{
  const gchar * const *langs;

  self->try_count = 0;
  self->hls_uri = NULL;

  /* FIXME: get from cache */
  self->visitor_data = g_strdup ("");
  self->client_version = g_strdup ("16.37.36");

  langs = g_get_language_names ();
  while (*langs) {
    if (strlen (*langs) == 5 && g_str_has_prefix (*langs + 2, "_")) {
      self->locale = g_strdup (*langs);
      break;
    }
    langs++;
  }
  if (!self->locale)
    self->locale = g_strdup ("en_US");
}

static void
gtuber_youtube_class_init (GtuberYoutubeClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtuberWebsiteClass *website_class = (GtuberWebsiteClass *) klass;

  gobject_class->finalize = gtuber_youtube_finalize;

  website_class->handles_input_stream = TRUE;
  website_class->create_request = gtuber_youtube_create_request;
  website_class->parse_input_stream = gtuber_youtube_parse_input_stream;
}

static void
gtuber_youtube_finalize (GObject *object)
{
  GtuberYoutube *self = GTUBER_YOUTUBE (object);

  g_debug ("Youtube finalize");

  g_free (self->video_id);
  g_free (self->hls_uri);

  g_free (self->visitor_data);
  g_free (self->client_version);
  g_free (self->locale);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
_read_stream_info (JsonReader *reader, GtuberStream *stream)
{
  const gchar *uri_str, *yt_mime;
  gchar *mod_uri;

  uri_str = gtuber_utils_json_get_string (reader, "url", NULL);

  /* No point continuing without URI */
  if (!uri_str)
    return;

  mod_uri = gtuber_utils_common_obtain_uri_with_query_as_path (uri_str);
  gtuber_stream_set_uri (stream, mod_uri);
  g_free (mod_uri);

  gtuber_stream_set_itag (stream, gtuber_utils_json_get_int (reader, "itag", NULL));
  gtuber_stream_set_bitrate (stream, gtuber_utils_json_get_int (reader, "bitrate", NULL));
  gtuber_stream_set_width (stream, gtuber_utils_json_get_int (reader, "width", NULL));
  gtuber_stream_set_height (stream, gtuber_utils_json_get_int (reader, "height", NULL));
  gtuber_stream_set_fps (stream, gtuber_utils_json_get_int (reader, "fps", NULL));

  /* Parse mime type and codecs */
  if ((yt_mime = gtuber_utils_json_get_string (reader, "mimeType", NULL))) {
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
        GtuberStreamMimeType mime_type;

        mime_type = gtuber_utils_common_get_mime_type_from_string (strv[0]);
        gtuber_stream_set_mime_type (stream, mime_type);

        g_strstrip (g_strdelimit (codecs, "\"", ' '));

        switch (mime_type) {
          case GTUBER_STREAM_MIME_TYPE_AUDIO_MP4:
          case GTUBER_STREAM_MIME_TYPE_AUDIO_WEBM:
            /* codecs contain only one (audio) codec */
            gtuber_stream_set_audio_codec (stream, codecs);
            break;
          default:{
            gchar **cstrv;

            cstrv = g_strsplit (codecs, ",", 2);
            if (g_strv_length (cstrv) > 1)
              g_strstrip (cstrv[1]);

            gtuber_stream_set_codecs (stream, cstrv[0], cstrv[1]);

            g_strfreev (cstrv);
            break;
          }
        }
        g_free (codecs);
      }
    }
    g_strfreev (strv);
  }
}

static void
_read_stream_cb (JsonReader *reader, GtuberMediaInfo *info, gpointer user_data)
{
  GtuberStream *stream;
  guint itag;

  itag = gtuber_utils_json_get_int (reader, "itag", NULL);

  switch (itag) {
    case 0:  // unknown
    case 17: // deprecated 3GP
      return;
    default:
      break;
  }

  stream = gtuber_stream_new ();
  _read_stream_info (reader, stream);

  gtuber_media_info_add_stream (info, stream);
}

static void
_read_adaptive_stream_cb (JsonReader *reader, GtuberMediaInfo *info, gpointer user_data)
{
  GtuberAdaptiveStream *astream;

  astream = gtuber_adaptive_stream_new ();
  _read_stream_info (reader, GTUBER_STREAM (astream));

  if (gtuber_utils_json_go_to (reader, "initRange", NULL)) {
    const gchar *start, *end;

    start = gtuber_utils_json_get_string (reader, "start", NULL);
    end = gtuber_utils_json_get_string (reader, "end", NULL);

    if (start && end) {
      gtuber_adaptive_stream_set_init_range (astream,
          g_ascii_strtoull (start, NULL, 10),
          g_ascii_strtoull (end, NULL, 10));
    }

    gtuber_utils_json_go_back (reader, 1);
  }
  if (gtuber_utils_json_go_to (reader, "indexRange", NULL)) {
    const gchar *start, *end;

    start = gtuber_utils_json_get_string (reader, "start", NULL);
    end = gtuber_utils_json_get_string (reader, "end", NULL);

    if (start && end) {
      gtuber_adaptive_stream_set_index_range (astream,
          g_ascii_strtoull (start, NULL, 10),
          g_ascii_strtoull (end, NULL, 10));
    }

    gtuber_utils_json_go_back (reader, 1);
  }

  gtuber_adaptive_stream_set_manifest_type (astream, GTUBER_ADAPTIVE_STREAM_MANIFEST_DASH);
  gtuber_media_info_add_adaptive_stream (info, astream);
}

static void
_update_hls_stream_cb (GtuberAdaptiveStream *astream, gpointer user_data)
{
  GtuberStream *stream;
  GUri *guri;
  const gchar *uri_str;

  stream = GTUBER_STREAM (astream);
  uri_str = gtuber_stream_get_uri (stream);

  if (!uri_str)
    return;

  guri = g_uri_parse (uri_str, G_URI_FLAGS_ENCODED, NULL);
  if (guri) {
    gchar **path_parts;
    guint i = 0;

    path_parts = g_strsplit (g_uri_get_path (guri), "/", 0);

    while (path_parts[i]) {
      if (!strcmp (path_parts[i], "itag") && path_parts[i + 1]) {
        guint itag;

        itag = g_ascii_strtoull (path_parts[i + 1], NULL, 10);
        gtuber_stream_set_itag (stream, itag);
      }
      i++;
    }

    g_strfreev (path_parts);
    g_uri_unref (guri);
  }
}

static void
update_info_hls (GtuberMediaInfo *info)
{
  const GPtrArray *astreams;

  astreams = gtuber_media_info_get_adaptive_streams (info);
  g_ptr_array_foreach ((GPtrArray *) astreams, (GFunc) _update_hls_stream_cb, NULL);
}

static GtuberFlow
parse_response_data (GtuberYoutube *self, JsonParser *parser,
    GtuberMediaInfo *info, GError **error)
{
  JsonReader *reader = json_reader_new (json_parser_get_root (parser));
  const gchar *status, *visitor_data;
  GtuberFlow flow = GTUBER_FLOW_OK;

  /* Check if video is playable */
  status = gtuber_utils_json_get_string (reader, "playabilityStatus", "status", NULL);
  if (g_strcmp0 (status, "OK")) {
    if (self->try_count < 2) {
      flow = GTUBER_FLOW_RESTART;
      g_debug ("Video is not playable, trying again...");
    } else {
      g_set_error (error, GTUBER_WEBSITE_ERROR,
          GTUBER_WEBSITE_ERROR_OTHER,
          "Video is not playable");
    }
    goto finish;
  }

  if (gtuber_utils_json_go_to (reader, "videoDetails", NULL)) {
    const gchar *id, *title, *desc, *duration;

    id = gtuber_utils_json_get_string (reader, "videoId", NULL);
    gtuber_media_info_set_id (info, id);

    title = gtuber_utils_json_get_string (reader, "title", NULL);
    gtuber_media_info_set_title (info, title);

    desc = gtuber_utils_json_get_string (reader, "shortDescription", NULL);
    gtuber_media_info_set_description (info, desc);

    duration = gtuber_utils_json_get_string (reader, "lengthSeconds", NULL);
    if (duration)
      gtuber_media_info_set_duration (info, g_ascii_strtoull (duration, NULL, 10));

    gtuber_utils_json_go_back (reader, 1);
  }

  if (gtuber_utils_json_go_to (reader, "streamingData", NULL)) {
    self->hls_uri = g_strdup (gtuber_utils_json_get_string (reader, "hlsManifestUrl", NULL));

    if (!self->hls_uri) {
      if (gtuber_utils_json_go_to (reader, "formats", NULL)) {
        gtuber_utils_json_array_foreach (reader, info,
            (GtuberFunc) _read_stream_cb, NULL);
        gtuber_utils_json_go_back (reader, 1);
      }
      if (gtuber_utils_json_go_to (reader, "adaptiveFormats", NULL)) {
        gtuber_utils_json_array_foreach (reader, info,
            (GtuberFunc) _read_adaptive_stream_cb, NULL);
        gtuber_utils_json_go_back (reader, 1);
      }
    }
    gtuber_utils_json_go_back (reader, 1);
  }

  visitor_data = gtuber_utils_json_get_string (reader, "responseContext", "visitorData", NULL);
  if (visitor_data && strcmp (self->visitor_data, visitor_data)) {
    g_free (self->visitor_data);
    self->visitor_data = g_strdup (visitor_data);
    g_debug ("Updated visitor_data: %s", self->visitor_data);

    /* TODO: Update cache */
  }

finish:
  if (*error)
    flow = GTUBER_FLOW_ERROR;

  g_object_unref (reader);

  return flow;
}

static gchar *
obtain_player_req_body (GtuberYoutube *self, GtuberMediaInfo *info)
{
  gchar *req_body, **parts;
  const gchar *cliScreen;

  /* Get EMBED video info on first try.
   * If it fails, try default one on next try */
  cliScreen = (self->try_count == 1)
      ? "\"EMBED\""
      : "null";

  parts = g_strsplit (self->locale, "_", 0);
  req_body = g_strdup_printf ("{\n"
  "  \"context\": {\n"
  "    \"client\": {\n"
  "      \"clientName\": \"ANDROID\",\n"
  "      \"clientVersion\": \"%s\",\n"
  "      \"clientScreen\": %s,\n"
  "      \"hl\": \"%s\",\n"
  "      \"gl\": \"%s\",\n"
  "      \"visitorData\": \"%s\"\n"
  "    },\n"
  "    \"thirdParty\": {\n"
  "      \"embedUrl\": \"https://www.youtube.com/watch?v=%s\"\n"
  "    },\n"
  "    \"user\": {\n"
  "      \"lockedSafetyMode\": false\n"
  "    }\n"
  "  },\n"
  "  \"video_id\": \"%s\"\n"
  "}",
      self->client_version, cliScreen, parts[0], parts[1],
      self->visitor_data, self->video_id, self->video_id);

  g_strfreev (parts);

  return req_body;
}

static GtuberFlow
gtuber_youtube_create_request (GtuberWebsite *website,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error)
{
  GtuberYoutube *self = GTUBER_YOUTUBE (website);
  SoupMessageHeaders *headers;
  gchar *req_body, *ua;

  if (!self->hls_uri) {
    self->try_count++;
    g_debug ("Try number: %i", self->try_count);

    *msg = soup_message_new ("POST",
        "https://www.youtube.com/youtubei/v1/player?"
        "key=AIzaSyA8eiZmM1FaDVjRy-df2KTyQ_vz_yYM39w");
    req_body = obtain_player_req_body (self, info);
    g_debug ("Request body: %s", req_body);

    gtuber_utils_common_msg_take_request (*msg, "application/json", req_body);
  } else {
    *msg = soup_message_new ("GET", self->hls_uri);
  }
  headers = soup_message_get_request_headers (*msg);

  ua = g_strdup_printf (
      "com.google.android.youtube/%s(Linux; U; Android 11; en_US) gzip",
      self->client_version);
  soup_message_headers_replace (headers, "User-Agent", ua);
  soup_message_headers_append (headers, "X-Goog-Api-Format-Version", "2");
  soup_message_headers_append (headers, "X-Goog-Visitor-Id", self->visitor_data);
  g_free (ua);

  return GTUBER_FLOW_OK;
}

static GtuberFlow
gtuber_youtube_parse_input_stream (GtuberWebsite *website,
    GInputStream *stream, GtuberMediaInfo *info, GError **error)
{
  GtuberYoutube *self = GTUBER_YOUTUBE (website);
  GtuberFlow flow = GTUBER_FLOW_OK;
  JsonParser *parser;

  if (self->hls_uri) {
    if (gtuber_utils_common_parse_hls_input_stream (stream, info, error)) {
      update_info_hls (info);
      return GTUBER_FLOW_OK;
    }
    return GTUBER_FLOW_ERROR;
  }

  parser = json_parser_new ();
  json_parser_load_from_stream (parser, stream, NULL, error);
  if (*error)
    goto finish;

  gtuber_utils_json_parser_debug (parser);
  flow = parse_response_data (self, parser, info, error);

finish:
  g_object_unref (parser);

  if (*error)
    return GTUBER_FLOW_ERROR;
  if (self->hls_uri)
    return GTUBER_FLOW_RESTART;

  return flow;
}

GtuberWebsite *
query_plugin (GUri *uri)
{
  guint uri_match;
  gchar *id;

  if (!gtuber_utils_common_uri_matches_hosts (uri, &uri_match,
      "youtube.com",
      "m.youtube.com",
      "youtu.be",
      NULL))
    return NULL;

  id = (uri_match == 3)
      ? gtuber_utils_common_obtain_uri_id_from_paths (uri, NULL, "/", NULL)
      : gtuber_utils_common_obtain_uri_query_value (uri, "v");

  if (!id)
    id = gtuber_utils_common_obtain_uri_id_from_paths (uri, NULL, "/v/", NULL);

  if (id) {
    GtuberYoutube *youtube;

    youtube = g_object_new (GTUBER_TYPE_YOUTUBE, NULL);
    youtube->video_id = id;

    g_debug ("Requested video: %s", youtube->video_id);

    return GTUBER_WEBSITE (youtube);
  }

  return NULL;
}
