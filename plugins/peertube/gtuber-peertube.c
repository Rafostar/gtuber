/*
 * Copyright (C) 2022 Rafał Dzięgiel <rafostar.github@gmail.com>
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

GTUBER_WEBSITE_PLUGIN_EXPORT_HOSTS_FROM_FILE (peertube)
GTUBER_WEBSITE_PLUGIN_EXPORT_SCHEMES (
  "http",
  "https",
  "peertube",
  NULL
)

GTUBER_WEBSITE_PLUGIN_DECLARE (Peertube, peertube, PEERTUBE)

struct _GtuberPeertube
{
  GtuberWebsite parent;

  gchar *video_id;
  gchar *hls_uri;
};

#define parent_class gtuber_peertube_parent_class
GTUBER_WEBSITE_PLUGIN_DEFINE (Peertube, peertube)

static void gtuber_peertube_finalize (GObject *object);

static GtuberFlow gtuber_peertube_create_request (GtuberWebsite *website,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error);
static GtuberFlow gtuber_peertube_parse_input_stream (GtuberWebsite *website,
    GInputStream *stream, GtuberMediaInfo *info, GError **error);

static void
gtuber_peertube_init (GtuberPeertube *self)
{
}

static void
gtuber_peertube_class_init (GtuberPeertubeClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtuberWebsiteClass *website_class = (GtuberWebsiteClass *) klass;

  gobject_class->finalize = gtuber_peertube_finalize;

  website_class->handles_input_stream = TRUE;
  website_class->create_request = gtuber_peertube_create_request;
  website_class->parse_input_stream = gtuber_peertube_parse_input_stream;
}

static void
gtuber_peertube_finalize (GObject *object)
{
  GtuberPeertube *self = GTUBER_PEERTUBE (object);

  g_debug ("Peertube finalize");

  g_free (self->video_id);
  g_free (self->hls_uri);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
_read_streaming_playlist_cb (JsonReader *reader, GtuberMediaInfo *info, GtuberPeertube *self)
{
  if (!self->hls_uri)
    self->hls_uri = g_strdup (gtuber_utils_json_get_string (reader, "playlistUrl", NULL));
}

static void
_read_stream_cb (JsonReader *reader, GtuberMediaInfo *info, GtuberPeertube *self)
{
  GtuberStream *stream;
  const gchar *uri;
  gint64 id, size;
  guint duration;

  /* No point continuing without URI */
  if (!(uri = gtuber_utils_json_get_string (reader, "fileUrl", NULL)))
    return;

  stream = gtuber_stream_new ();
  gtuber_stream_set_uri (stream, uri);

  /* Peertube uses video height as stream itag */
  id = gtuber_utils_json_get_int (reader, "resolution", "id", NULL);
  gtuber_stream_set_itag (stream, id);
  gtuber_stream_set_height (stream, id);

  gtuber_stream_set_fps (stream, gtuber_utils_json_get_int (reader, "fps", NULL));
  gtuber_stream_set_bitrate (stream, gtuber_utils_json_get_int (reader, "bitrate", NULL));

  /* We do not have bitrate from API, but can calcucate something approximately */
  size = gtuber_utils_json_get_int (reader, "size", NULL);
  duration = gtuber_media_info_get_duration (info);
  if (size > 0 && duration > 0)
    gtuber_stream_set_bitrate (stream, (size * 8) / duration);

  /* XXX: Peertube does only "avc1" + "mp4a" AFAIK */
  gtuber_stream_set_codecs (stream, "avc1", "mp4a");
  gtuber_stream_set_mime_type (stream, GTUBER_STREAM_MIME_TYPE_VIDEO_MP4);

  gtuber_media_info_add_stream (info, stream);
}

static void
parse_response_data (GtuberPeertube *self, JsonParser *parser,
    GtuberMediaInfo *info, GError **error)
{
  JsonReader *reader = json_reader_new (json_parser_get_root (parser));
  gchar *id;

  id = g_strdup_printf ("%li", gtuber_utils_json_get_int (reader, "id", NULL));
  gtuber_media_info_set_id (info, id);
  g_free (id);

  gtuber_media_info_set_title (info, gtuber_utils_json_get_string (reader, "name", NULL));
  gtuber_media_info_set_description (info, gtuber_utils_json_get_string (reader, "description", NULL));
  gtuber_media_info_set_duration (info, gtuber_utils_json_get_int (reader, "duration", NULL));

  if (gtuber_utils_json_go_to (reader, "streamingPlaylists", NULL)) {
    gtuber_utils_json_array_foreach (reader, info,
        (GtuberFunc) _read_streaming_playlist_cb, self);
    gtuber_utils_json_go_back (reader, 1);
  }
  if (gtuber_utils_json_go_to (reader, "files", NULL)) {
    gtuber_utils_json_array_foreach (reader, info,
        (GtuberFunc) _read_stream_cb, self);
    gtuber_utils_json_go_back (reader, 1);
  }

  g_object_unref (reader);
}

static GtuberFlow
gtuber_peertube_create_request (GtuberWebsite *website,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error)
{
  GtuberPeertube *self = GTUBER_PEERTUBE (website);

  if (!self->hls_uri) {
    GUri *guri, *website_uri;
    gchar *path;
    gboolean use_http;

    website_uri = gtuber_website_get_uri (website);
    use_http = gtuber_website_get_use_http (website);

    g_debug ("Using secure HTTP: %s", use_http ? "no" : "yes");

    path = g_strdup_printf ("/api/v1/videos/%s", self->video_id);
    guri = g_uri_build (G_URI_FLAGS_ENCODED,
        use_http ? "http" : "https", NULL,
        g_uri_get_host (website_uri),
        g_uri_get_port (website_uri),
        path, NULL, NULL);
    g_free (path);

    *msg = soup_message_new_from_uri ("GET", guri);
    g_uri_unref (guri);
  } else {
    *msg = soup_message_new ("GET", self->hls_uri);
  }

  return GTUBER_FLOW_OK;
}

static GtuberFlow
gtuber_peertube_parse_input_stream (GtuberWebsite *website,
    GInputStream *stream, GtuberMediaInfo *info, GError **error)
{
  GtuberPeertube *self = GTUBER_PEERTUBE (website);
  JsonParser *parser;

  if (self->hls_uri) {
    return (gtuber_utils_common_parse_hls_input_stream_with_base_uri (stream,
        info, self->hls_uri, error))
      ? GTUBER_FLOW_OK
      : GTUBER_FLOW_ERROR;
  }

  parser = json_parser_new ();
  if (json_parser_load_from_stream (parser, stream, NULL, error)) {
    gtuber_utils_json_parser_debug (parser);
    parse_response_data (self, parser, info, error);
  }
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

  id = gtuber_utils_common_obtain_uri_id_from_paths (uri, NULL,
      "/videos/watch/", "/w/", NULL);

  if (id) {
    GtuberPeertube *peertube;

    peertube = gtuber_peertube_new ();
    peertube->video_id = id;

    g_debug ("Requested host: %s, video: %s",
        g_uri_get_host (uri), peertube->video_id);

    return GTUBER_WEBSITE (peertube);
  }

  return NULL;
}
