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
#include "utils/youtube/gtuber-utils-youtube.h"

/* Host must expose API at /api/v1/ path */
GTUBER_WEBSITE_PLUGIN_EXPORT_HOSTS (
  "invidious.snopyta.org",
  "vid.puffyan.us",
  "inv.riverside.rocks",
  "invidio.xamh.de",
  "vid.mint.lgbt",
  "invidious.hub.ne.kr",
  NULL
)
GTUBER_WEBSITE_PLUGIN_DECLARE (Invidious, invidious, INVIDIOUS)

struct _GtuberInvidious
{
  GtuberWebsite parent;

  gchar *video_id;
  gchar *source;
  gchar *hls_uri;
};

#define parent_class gtuber_invidious_parent_class
GTUBER_WEBSITE_PLUGIN_DEFINE (Invidious, invidious)

static void gtuber_invidious_finalize (GObject *object);

static GtuberFlow gtuber_invidious_create_request (GtuberWebsite *website,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error);
static GtuberFlow gtuber_invidious_parse_input_stream (GtuberWebsite *website,
    GInputStream *stream, GtuberMediaInfo *info, GError **error);

static void
gtuber_invidious_init (GtuberInvidious *self)
{
  self->hls_uri = NULL;
}

static void
gtuber_invidious_class_init (GtuberInvidiousClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtuberWebsiteClass *website_class = (GtuberWebsiteClass *) klass;

  gobject_class->finalize = gtuber_invidious_finalize;

  website_class->handles_input_stream = TRUE;
  website_class->create_request = gtuber_invidious_create_request;
  website_class->parse_input_stream = gtuber_invidious_parse_input_stream;
}

static void
gtuber_invidious_finalize (GObject *object)
{
  GtuberInvidious *self = GTUBER_INVIDIOUS (object);

  g_debug ("Invidious finalize");

  g_free (self->video_id);
  g_free (self->source);
  g_free (self->hls_uri);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GtuberStream *
get_filled_stream (GtuberInvidious *self, JsonReader *reader,
    GtuberMediaInfo *info, gboolean is_adaptive)
{
  GtuberStream *stream = NULL;
  const gchar *org_uri, *itag_str;
  gchar *mod_uri;
  guint itag = 0;

  org_uri = gtuber_utils_json_get_string (reader, "url", NULL);
  itag_str = gtuber_utils_json_get_string (reader, "itag", NULL);

  if (!org_uri || !itag_str)
    return NULL;

  itag = g_ascii_strtoull (itag_str, NULL, 10);

  switch (itag) {
    case 17: // 3GP
      return NULL;
    default:
      break;
  }

  mod_uri = gtuber_utils_common_replace_uri_source (org_uri, self->source);

  if (mod_uri && itag > 0) {
    const gchar *bitrate, *res, *yt_mime;
    guint width = 0;
    guint height = 0;

    /* Parse size, otherwise use resolution as fallback to get height */
    res = gtuber_utils_json_get_string (reader, "size", NULL);
    if (res) {
      gchar **parts = g_strsplit (res, "x", 3);
      if (parts[0] && parts[1] && !parts[2]) {
        width = g_ascii_strtoull (parts[0], NULL, 10);
        height = g_ascii_strtoull (parts[1], NULL, 10);
      }
      g_strfreev (parts);
    } else {
      res = gtuber_utils_json_get_string (reader, "resolution", NULL);
      if (res)
        height = g_ascii_strtoull (res, NULL, 10);
    }

    stream = (is_adaptive)
        ? GTUBER_STREAM (gtuber_adaptive_stream_new ())
        : gtuber_stream_new ();

    gtuber_stream_set_uri (stream, mod_uri);
    gtuber_stream_set_itag (stream, itag);

    bitrate = gtuber_utils_json_get_string (reader, "bitrate", NULL);
    if (bitrate)
      gtuber_stream_set_bitrate (stream, g_ascii_strtoull (bitrate, NULL, 10));

    if (width || height) {
      gtuber_stream_set_width (stream, width);
      gtuber_stream_set_height (stream, height);
      gtuber_stream_set_fps (stream,
          gtuber_utils_json_get_int (reader, "fps", NULL));
    }

    if ((yt_mime = gtuber_utils_json_get_string (reader, "type", NULL))) {
      GtuberStreamMimeType mime_type = GTUBER_STREAM_MIME_TYPE_UNKNOWN;
      gchar *vcodec = NULL;
      gchar *acodec = NULL;

      gtuber_utils_youtube_parse_mime_type_string (yt_mime, &mime_type, &vcodec, &acodec);
      gtuber_stream_set_mime_type (stream, mime_type);
      gtuber_stream_set_codecs (stream, vcodec, acodec);

      g_free (vcodec);
      g_free (acodec);
    }
  }

  g_free (mod_uri);

  return stream;
}

static void
_add_stream_cb (JsonReader *reader, GtuberMediaInfo *info,
    GtuberInvidious *self)
{
  GtuberStream *stream;

  stream = get_filled_stream (self, reader, info, FALSE);

  if (stream)
    gtuber_media_info_add_stream (info, stream);
}

static void
_add_adaptive_stream_cb (JsonReader *reader, GtuberMediaInfo *info,
    GtuberInvidious *self)
{
  GtuberStream *stream;

  stream = get_filled_stream (self, reader, info, TRUE);
  if (stream) {
    GtuberAdaptiveStream *astream;
    const gchar *init_range, *index_range;

    astream = GTUBER_ADAPTIVE_STREAM (stream);

    if ((init_range = gtuber_utils_json_get_string (reader, "init", NULL))) {
      gchar **parts = g_strsplit (init_range, "-", 3);
      if (parts[0] && parts[1] && !parts[2]) {
        gtuber_adaptive_stream_set_init_range (astream,
            g_ascii_strtod (parts[0], NULL), g_ascii_strtod (parts[1], NULL));
      }
      g_strfreev (parts);
    }
    if ((index_range = gtuber_utils_json_get_string (reader, "index", NULL))) {
      gchar **parts = g_strsplit (index_range, "-", 3);
      if (parts[0] && parts[1] && !parts[2]) {
        gtuber_adaptive_stream_set_index_range (astream,
            g_ascii_strtod (parts[0], NULL), g_ascii_strtod (parts[1], NULL));
      }
      g_strfreev (parts);
    }

    gtuber_adaptive_stream_set_manifest_type (astream,
        GTUBER_ADAPTIVE_STREAM_MANIFEST_DASH);

    gtuber_media_info_add_adaptive_stream (info, astream);
  }
}

static void
parse_response_data (GtuberInvidious *self, JsonParser *parser,
    GtuberMediaInfo *info, GError **error)
{
  JsonReader *reader = json_reader_new (json_parser_get_root (parser));
  const gchar *desc;

  gtuber_media_info_set_id (info,
      gtuber_utils_json_get_string (reader, "videoId", NULL));
  gtuber_media_info_set_title (info,
      gtuber_utils_json_get_string (reader, "title", NULL));
  gtuber_media_info_set_duration (info,
      gtuber_utils_json_get_int (reader, "lengthSeconds", NULL));

  desc = gtuber_utils_json_get_string (reader, "description", NULL);
  gtuber_media_info_set_description (info, desc);

  if (gtuber_utils_json_get_boolean (reader, "liveNow", NULL))
    self->hls_uri = g_strdup (gtuber_utils_json_get_string (reader, "hlsUrl", NULL));

  if (!self->hls_uri) {
    if (gtuber_utils_json_go_to (reader, "formatStreams", NULL)) {
      gtuber_utils_json_array_foreach (reader, info,
          (GtuberFunc) _add_stream_cb, self);
      gtuber_utils_json_go_back (reader, 1);
    }
    if (gtuber_utils_json_go_to (reader, "adaptiveFormats", NULL)) {
      gtuber_utils_json_array_foreach (reader, info,
          (GtuberFunc) _add_adaptive_stream_cb, self);
      gtuber_utils_json_go_back (reader, 1);
    }
    gtuber_utils_youtube_insert_chapters_from_description (info, desc);
  }

  g_object_unref (reader);
}

static GtuberFlow
gtuber_invidious_create_request (GtuberWebsite *website,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error)
{
  GtuberInvidious *self = GTUBER_INVIDIOUS (website);

  if (!self->hls_uri) {
    gchar *api_uri, *msg_uri;

    api_uri = g_uri_resolve_relative (
        gtuber_website_get_uri (website),
        "/api/v1/videos",
        G_URI_FLAGS_ENCODED,
        error);

    if (*error)
      return GTUBER_FLOW_ERROR;

    msg_uri = g_strjoin ("/", api_uri, self->video_id, NULL);
    *msg = soup_message_new ("GET", msg_uri);

    g_free (api_uri);
    g_free (msg_uri);
  } else {
    *msg = soup_message_new ("GET", self->hls_uri);
  }

  return GTUBER_FLOW_OK;
}

static GtuberFlow
gtuber_invidious_parse_input_stream (GtuberWebsite *website,
    GInputStream *stream, GtuberMediaInfo *info, GError **error)
{
  GtuberInvidious *self = GTUBER_INVIDIOUS (website);
  JsonParser *parser;

  if (self->hls_uri) {
    if (gtuber_utils_common_parse_hls_input_stream (stream, info, error)) {
      /* FIXME: Share update HLS code with youtube plugin */
      return GTUBER_FLOW_OK;
    }
    return GTUBER_FLOW_ERROR;
  }

  parser = json_parser_new ();
  json_parser_load_from_stream (parser, stream, NULL, error);
  if (*error)
    goto finish;

  gtuber_utils_json_parser_debug (parser);
  parse_response_data (self, parser, info, error);

finish:
  g_object_unref (parser);

  if (*error)
    return GTUBER_FLOW_ERROR;
  if (self->hls_uri)
    return GTUBER_FLOW_RESTART;

  return GTUBER_FLOW_OK;
}

GtuberWebsite *
plugin_query (GUri *uri)
{
  gchar *id;

  id = gtuber_utils_common_obtain_uri_query_value (uri, "v");
  if (!id)
    id = gtuber_utils_common_obtain_uri_id_from_paths (uri, NULL, "/v/", NULL);

  if (id) {
    GtuberInvidious *invidious;

    invidious = gtuber_invidious_new ();
    invidious->video_id = id;
    invidious->source = gtuber_utils_common_obtain_uri_source (uri);

    g_debug ("Requested source: %s, video: %s",
        invidious->source, invidious->video_id);

    return GTUBER_WEBSITE (invidious);
  }

  return NULL;
}
