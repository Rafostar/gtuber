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
#include <json-glib/json-glib.h>

#include "utils/common/gtuber-utils-common.h"
#include "utils/json/gtuber-utils-json.h"

#include "gtuber/gtuber-soup-compat.h"

typedef enum
{
  LBRY_STREAM_UNKNOWN,
  LBRY_STREAM_HLS,
  LBRY_STREAM_DIRECT,
} LbryStreamType;

GTUBER_WEBSITE_PLUGIN_EXPORT_HOSTS (
  "odysee.com",
  NULL
)
GTUBER_WEBSITE_PLUGIN_EXPORT_SCHEMES (
  "http",
  "https",
  "lbry",
  NULL
)
GTUBER_WEBSITE_PLUGIN_DECLARE (Lbry, lbry, LBRY)

struct _GtuberLbry
{
  GtuberWebsite parent;

  gchar *video_id;
  gchar *streaming_url;

  LbryStreamType stream_type;
  gboolean resolve;
};

#define parent_class gtuber_lbry_parent_class
GTUBER_WEBSITE_PLUGIN_DEFINE (Lbry, lbry)

static void gtuber_lbry_finalize (GObject *object);

static GtuberFlow gtuber_lbry_create_request (GtuberWebsite *website,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error);
static GtuberFlow gtuber_lbry_read_response (GtuberWebsite *website,
    SoupMessage *msg, GError **error);
static GtuberFlow gtuber_lbry_parse_input_stream (GtuberWebsite *website,
    GInputStream *stream, GtuberMediaInfo *info, GError **error);

static void
gtuber_lbry_init (GtuberLbry *self)
{
  self->stream_type = LBRY_STREAM_UNKNOWN;
}

static void
gtuber_lbry_class_init (GtuberLbryClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtuberWebsiteClass *website_class = (GtuberWebsiteClass *) klass;

  gobject_class->finalize = gtuber_lbry_finalize;

  website_class->handles_input_stream = TRUE;
  website_class->create_request = gtuber_lbry_create_request;
  website_class->read_response = gtuber_lbry_read_response;
  website_class->parse_input_stream = gtuber_lbry_parse_input_stream;
}

static void
gtuber_lbry_finalize (GObject *object)
{
  GtuberLbry *self = GTUBER_LBRY (object);

  g_debug ("Lbry finalize");

  g_free (self->video_id);
  g_free (self->streaming_url);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GtuberFlow
acquire_streaming_url (GtuberLbry *self, JsonReader *reader, GError **error)
{
  const gchar *url;

  g_debug ("Searching for stream URI...");

  url = gtuber_utils_json_get_string (reader, "result", "streaming_url", NULL);
  if (!url) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Streaming URL is missing");
    return GTUBER_FLOW_ERROR;
  }

  self->streaming_url = g_strdup (url);
  g_debug ("Got stream URI: %s", self->streaming_url);

  return GTUBER_FLOW_RESTART;
}

static GtuberFlow
parse_hls (GtuberLbry *self, GInputStream *stream,
    GtuberMediaInfo *info, GError **error)
{
  g_debug ("Parsing LBRY HLS...");
  self->resolve = gtuber_utils_common_parse_hls_input_stream (stream, info, error);
  g_debug ("HLS parsed, success: %s", self->resolve ? "YES" : "NO");

  return (self->resolve)
      ? GTUBER_FLOW_RESTART
      : GTUBER_FLOW_ERROR;
}

static gboolean
fill_streams_info (GtuberLbry *self, GPtrArray *array, GtuberStreamMimeType mime_type)
{
  gboolean success = TRUE;
  guint i;

  for (i = 0; i < array->len; i++) {
    GtuberStream *stream;

    stream = (GtuberStream *) g_ptr_array_index (array, i);
    gtuber_stream_set_mime_type (stream, mime_type);

    if (!gtuber_stream_get_video_codec (stream)
        && !gtuber_stream_get_audio_codec (stream)) {
      const gchar *vcodec = NULL, *acodec = NULL;

      switch (mime_type) {
        case GTUBER_STREAM_MIME_TYPE_VIDEO_MP4:
          vcodec = "avc1";
        case GTUBER_STREAM_MIME_TYPE_AUDIO_MP4:
          acodec = "mp4a";
          break;
        default:
          break;
      }

      gtuber_stream_set_codecs (stream, vcodec, acodec);
    }

    if (self->stream_type == LBRY_STREAM_HLS) {
      gchar *full_uri;

      full_uri = g_uri_resolve_relative (self->streaming_url,
          gtuber_stream_get_uri (stream), G_URI_FLAGS_ENCODED, NULL);

      if (!(success = full_uri != NULL))
        break;

      gtuber_stream_set_uri (stream, full_uri);
      g_free (full_uri);
    }
  }

  return success;
}

static GtuberFlow
fill_media_info (GtuberLbry *self, JsonReader *reader,
    GtuberMediaInfo *info, GError **error)
{
  const gchar *mime_str;
  gboolean success = FALSE;

  g_debug ("Filling media info...");

  if (!gtuber_utils_json_go_to (reader, "result", self->video_id, "value", NULL)) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Invalid website API response");
    return GTUBER_FLOW_ERROR;
  }

  gtuber_media_info_set_id (info, self->video_id);
  gtuber_media_info_set_title (info,
      gtuber_utils_json_get_string (reader, "title", NULL));
  gtuber_media_info_set_description (info,
      gtuber_utils_json_get_string (reader, "description", NULL));

  if (gtuber_utils_json_go_to (reader, "video", NULL)) {
    gtuber_media_info_set_duration (info,
        gtuber_utils_json_get_int (reader, "duration", NULL));

    if (self->stream_type == LBRY_STREAM_DIRECT) {
      GtuberStream *stream;

      stream = gtuber_stream_new ();
      gtuber_stream_set_itag (stream, 1);
      gtuber_stream_set_uri (stream, self->streaming_url);
      gtuber_stream_set_width (stream,
          gtuber_utils_json_get_int (reader, "width", NULL));
      gtuber_stream_set_height (stream,
          gtuber_utils_json_get_int (reader, "height", NULL));

      gtuber_media_info_add_stream (info, stream);
    }

    gtuber_utils_json_go_back (reader, 1);
  }

  if ((mime_str = gtuber_utils_json_get_string (reader,
      "source", "media_type", NULL))) {
    GPtrArray *streams, *astreams;
    GtuberStreamMimeType mime_type;

    streams = gtuber_media_info_get_streams (info);
    astreams = gtuber_media_info_get_adaptive_streams (info);
    mime_type = gtuber_utils_common_get_mime_type_from_string (mime_str);

    if (!(success = (fill_streams_info (self, streams, mime_type)
        && fill_streams_info (self, astreams, mime_type)))) {
      g_set_error (error, GTUBER_WEBSITE_ERROR,
          GTUBER_WEBSITE_ERROR_PARSE_FAILED,
          "Could not fill all streams info");
    }
  }

  /* Return from "result.video_id.value" */
  gtuber_utils_json_go_back (reader, 3);

  if (!success)
    return GTUBER_FLOW_ERROR;

  g_debug ("Media info filled");
  return GTUBER_FLOW_OK;
}

static void
lbry_update_streaming_url (GtuberLbry *self, SoupMessage *msg)
{
  GUri *guri;
  gchar *uri_str;

  guri = gtuber_soup_message_obtain_uri (msg);
  uri_str = g_uri_to_string (guri);
  g_uri_unref (guri);

  if (uri_str) {
    g_free (self->streaming_url);
    self->streaming_url = uri_str;

    g_debug ("Updated stream URI: %s", self->streaming_url);
  }
}

static GtuberFlow
gtuber_lbry_create_request (GtuberWebsite *website,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error)
{
  GtuberLbry *self = GTUBER_LBRY (website);
  gchar *req_body;

  if (self->streaming_url && !self->resolve) {
    *msg = (self->stream_type == LBRY_STREAM_UNKNOWN)
        ? soup_message_new ("HEAD", self->streaming_url)
        : soup_message_new ("GET", self->streaming_url);

    return GTUBER_FLOW_OK;
  }

  req_body = g_strdup_printf (
      "{"
      "  \"method\": \"%s\","
      "  \"params\": {"
      "    \"%s\": \"%s\""
      "  }"
      "}",
      self->resolve ? "resolve" : "get",
      self->resolve ? "urls" : "uri",
      self->video_id);

  *msg = soup_message_new ("POST", "https://api.na-backend.odysee.com/api/v1/proxy");
  gtuber_utils_common_msg_take_request (*msg, "application/json-rpc", req_body);

  return GTUBER_FLOW_OK;
}

static GtuberFlow
gtuber_lbry_read_response (GtuberWebsite *website,
    SoupMessage *msg, GError **error)
{
  GtuberLbry *self = GTUBER_LBRY (website);
  SoupStatus status;

  status = soup_message_get_status (msg);

  if (status >= 400) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "HTTP response code: %i", status);
    return GTUBER_FLOW_ERROR;
  }

  if (self->streaming_url
      && self->stream_type == LBRY_STREAM_UNKNOWN) {
    SoupMessageHeaders *resp_headers;
    const gchar *content_type;

    resp_headers = soup_message_get_response_headers (msg);

    content_type = soup_message_headers_get_content_type (resp_headers, NULL);
    self->stream_type = (!strcmp (content_type, "application/x-mpegurl"))
        ? LBRY_STREAM_HLS
        : LBRY_STREAM_DIRECT;
    self->resolve = (self->stream_type == LBRY_STREAM_DIRECT);

    g_debug ("URI leads to HLS: %s",
        self->stream_type == LBRY_STREAM_HLS ? "YES" : "NO");

    /* Update URI from redirect */
    lbry_update_streaming_url (self, msg);

    /* Only HEAD here, nothing more to parse */
    return GTUBER_FLOW_RESTART;
  }

  return GTUBER_FLOW_OK;
}

static GtuberFlow
gtuber_lbry_parse_input_stream (GtuberWebsite *website,
    GInputStream *stream, GtuberMediaInfo *info, GError **error)
{
  GtuberLbry *self = GTUBER_LBRY (website);
  JsonParser *parser;
  JsonReader *reader;
  GtuberFlow flow = GTUBER_FLOW_ERROR;

  /* Last request was HLS manifest download */
  if (self->streaming_url && !self->resolve
      && self->stream_type == LBRY_STREAM_HLS)
    return parse_hls (self, stream, info, error);

  parser = json_parser_new ();
  json_parser_load_from_stream (parser, stream, NULL, error);
  if (*error)
    goto finish;

  gtuber_utils_json_parser_debug (parser);
  reader = json_reader_new (json_parser_get_root (parser));

  flow = (!self->streaming_url)
      ? acquire_streaming_url (self, reader, error)
      : fill_media_info (self, reader, info, error);

  g_object_unref (reader);

finish:
  g_object_unref (parser);

  if (*error)
    return GTUBER_FLOW_ERROR;

  return flow;
}

GtuberWebsite *
plugin_query (GUri *uri)
{
  GtuberLbry *lbry = NULL;
  gchar *path;

  if (!strcmp (g_uri_get_scheme (uri), "lbry")) {
    path = g_strdup_printf ("/@%s%s",
        g_uri_get_host (uri),
        g_uri_get_path (uri));
  } else {
    path = g_strdup (g_uri_get_path (uri));
  }

  /* Path must start from "/@" */
  if (path && g_str_has_prefix (path, "/@")) {
    gchar *video_path;

    video_path = g_uri_join (G_URI_FLAGS_ENCODED, NULL,
        NULL, NULL, -1, path, NULL, g_uri_get_fragment (uri));

    lbry = gtuber_lbry_new ();
    lbry->video_id = g_strdup (video_path + 1);

    g_debug ("Requested video: %s", lbry->video_id);
    g_free (video_path);
  }

  g_free (path);

  return GTUBER_WEBSITE (lbry);
}
