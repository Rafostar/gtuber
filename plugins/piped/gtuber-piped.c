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

typedef enum
{
  PIPED_MEDIA_NONE,
  PIPED_MEDIA_VIDEO_STREAM,
  PIPED_MEDIA_AUDIO_STREAM,
} PipedMediaType;

GTUBER_WEBSITE_PLUGIN_EXPORT_HOSTS (
  "piped.kavin.rocks",
  "piped.silkky.cloud",
  NULL
)
static const gchar *piped_apis[] = {
  "pipedapi.kavin.rocks",
  "api.piped.silkky.cloud",
  NULL
};

GTUBER_WEBSITE_PLUGIN_DECLARE (Piped, piped, PIPED)

struct _GtuberPiped
{
  GtuberWebsite parent;

  gchar *video_id;
  gchar *hls_uri;

  gint api_id;
};

#define parent_class gtuber_piped_parent_class
GTUBER_WEBSITE_PLUGIN_DEFINE (Piped, piped)

static void gtuber_piped_finalize (GObject *object);

static GtuberFlow gtuber_piped_create_request (GtuberWebsite *website,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error);
static GtuberFlow gtuber_piped_parse_input_stream (GtuberWebsite *website,
    GInputStream *stream, GtuberMediaInfo *info, GError **error);

static void
gtuber_piped_init (GtuberPiped *self)
{
}

static void
gtuber_piped_class_init (GtuberPipedClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtuberWebsiteClass *website_class = (GtuberWebsiteClass *) klass;

  gobject_class->finalize = gtuber_piped_finalize;

  website_class->handles_input_stream = TRUE;
  website_class->create_request = gtuber_piped_create_request;
  website_class->parse_input_stream = gtuber_piped_parse_input_stream;
}

static void
gtuber_piped_finalize (GObject *object)
{
  GtuberPiped *self = GTUBER_PIPED (object);

  g_debug ("Piped finalize");

  g_free (self->video_id);
  g_free (self->hls_uri);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
piped_read_stream (GtuberPiped *self, JsonReader *reader,
    GtuberMediaInfo *info, PipedMediaType media_type)
{
  GtuberAdaptiveStream *astream = NULL;
  GtuberStream *stream;
  GtuberStreamMimeType mime_type;
  GUri *guri;
  const gchar *uri;
  gboolean video_only;

  if (media_type == PIPED_MEDIA_VIDEO_STREAM) {
    const gchar *format;

    format = gtuber_utils_json_get_string (reader, "format", NULL);

    /* We treat 3GPP as deprecated */
    if (!g_strcmp0 (format, "v3GPP"))
      return;
  }

  video_only = gtuber_utils_json_get_boolean (reader, "videoOnly", NULL);
  uri = gtuber_utils_json_get_string (reader, "url", NULL);

  /* No point continuing without URI */
  if (!uri)
    return;

  /* Piped API does not say if stream is adaptive or not,
   * assume that "all audio" and "video only" are adaptive */
  if (media_type == PIPED_MEDIA_AUDIO_STREAM || video_only) {
    astream = gtuber_adaptive_stream_new ();

    gtuber_adaptive_stream_set_init_range (astream,
        gtuber_utils_json_get_int (reader, "initStart", NULL),
        gtuber_utils_json_get_int (reader, "initEnd", NULL));

    gtuber_adaptive_stream_set_index_range (astream,
        gtuber_utils_json_get_int (reader, "indexStart", NULL),
        gtuber_utils_json_get_int (reader, "indexEnd", NULL));

    gtuber_adaptive_stream_set_manifest_type (astream, GTUBER_ADAPTIVE_STREAM_MANIFEST_DASH);
  }

  stream = (astream != NULL)
      ? GTUBER_STREAM (astream)
      : gtuber_stream_new ();

  /* FIXME: It would be easier if Piped provided YT itags */
  guri = g_uri_parse (uri, G_URI_FLAGS_NONE, NULL);
  if (guri) {
    gchar *itag;

    itag = gtuber_utils_common_obtain_uri_query_value (guri, "itag");
    if (itag) {
      gtuber_stream_set_itag (stream, g_ascii_strtoull (itag, NULL, 10));
      g_free (itag);
    }
    g_uri_unref (guri);
  }

  gtuber_stream_set_uri (stream, uri);
  gtuber_stream_set_bitrate (stream, gtuber_utils_json_get_int (reader, "bitrate", NULL));

  switch (media_type) {
    case PIPED_MEDIA_VIDEO_STREAM:
      gtuber_stream_set_width (stream, gtuber_utils_json_get_int (reader, "width", NULL));
      gtuber_stream_set_height (stream, gtuber_utils_json_get_int (reader, "height", NULL));
      gtuber_stream_set_fps (stream, gtuber_utils_json_get_int (reader, "fps", NULL));
      gtuber_stream_set_video_codec (stream, gtuber_utils_json_get_string (reader, "codec", NULL));
      break;
    case PIPED_MEDIA_AUDIO_STREAM:
      gtuber_stream_set_audio_codec (stream, gtuber_utils_json_get_string (reader, "codec", NULL));
      break;
    default:
      g_assert_not_reached ();
  }

  mime_type = gtuber_utils_common_get_mime_type_from_string (
      gtuber_utils_json_get_string (reader, "mimeType", NULL));
  gtuber_stream_set_mime_type (stream, mime_type);

  if (astream)
    gtuber_media_info_add_adaptive_stream (info, astream);
  else
    gtuber_media_info_add_stream (info, stream);
}

static void
_read_video_stream_cb (JsonReader *reader, GtuberMediaInfo *info, GtuberPiped *self)
{
  piped_read_stream (self, reader, info, PIPED_MEDIA_VIDEO_STREAM);
}

static void
_read_audio_stream_cb (JsonReader *reader, GtuberMediaInfo *info, GtuberPiped *self)
{
  piped_read_stream (self, reader, info, PIPED_MEDIA_AUDIO_STREAM);
}

static void
_read_chapter_cb (JsonReader *reader, GtuberMediaInfo *info, GtuberPiped *self)
{
  const gchar *title;
  guint64 start;

  title = gtuber_utils_json_get_string (reader, "title", NULL);
  start = gtuber_utils_json_get_int (reader, "start", NULL);

  gtuber_media_info_insert_chapter (info, start * 1000, title);
}

static void
parse_response_data (GtuberPiped *self, JsonParser *parser,
    GtuberMediaInfo *info, GError **error)
{
  JsonReader *reader = json_reader_new (json_parser_get_root (parser));

  gtuber_media_info_set_id (info, self->video_id);
  gtuber_media_info_set_title (info, gtuber_utils_json_get_string (reader, "title", NULL));
  gtuber_media_info_set_description (info, gtuber_utils_json_get_string (reader, "description", NULL));
  gtuber_media_info_set_duration (info, gtuber_utils_json_get_int (reader, "duration", NULL));

  if (gtuber_utils_json_get_boolean (reader, "livestream", NULL))
    self->hls_uri = g_strdup (gtuber_utils_json_get_string (reader, "hls", NULL));

  if (!self->hls_uri) {
    if (gtuber_utils_json_go_to (reader, "videoStreams", NULL)) {
      gtuber_utils_json_array_foreach (reader, info,
          (GtuberFunc) _read_video_stream_cb, self);
      gtuber_utils_json_go_back (reader, 1);
    }
    if (gtuber_utils_json_go_to (reader, "audioStreams", NULL)) {
      gtuber_utils_json_array_foreach (reader, info,
          (GtuberFunc) _read_audio_stream_cb, self);
      gtuber_utils_json_go_back (reader, 1);
    }
    if (gtuber_utils_json_go_to (reader, "chapters", NULL)) {
      gtuber_utils_json_array_foreach (reader, info,
          (GtuberFunc) _read_chapter_cb, self);
      gtuber_utils_json_go_back (reader, 1);
    }
  }

  g_object_unref (reader);
}

static GtuberFlow
gtuber_piped_create_request (GtuberWebsite *website,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error)
{
  GtuberPiped *self = GTUBER_PIPED (website);

  if (!self->hls_uri) {
    gchar *uri;

    uri = g_strdup_printf ("https://%s/streams/%s",
        piped_apis[self->api_id], self->video_id);
    *msg = soup_message_new ("GET", uri);

    g_free (uri);
  } else {
    *msg = soup_message_new ("GET", self->hls_uri);
  }

  return GTUBER_FLOW_OK;
}

static GtuberFlow
gtuber_piped_parse_input_stream (GtuberWebsite *website,
    GInputStream *stream, GtuberMediaInfo *info, GError **error)
{
  GtuberPiped *self = GTUBER_PIPED (website);
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
    GtuberPiped *piped;

    piped = gtuber_piped_new ();
    piped->video_id = id;

    if (G_UNLIKELY (!gtuber_utils_common_uri_matches_hosts_array (uri,
        &piped->api_id, _hosts_compat)))
      g_critical ("Plugin query with unmatched host, this should not happen");

    g_debug ("Requested video: %s, API instance: %s",
        piped->video_id, piped_apis[piped->api_id]);

    return GTUBER_WEBSITE (piped);
  }

  return NULL;
}
