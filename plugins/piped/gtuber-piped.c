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

#include "gtuber-piped.h"
#include "utils/common/gtuber-utils-common.h"
#include "utils/json/gtuber-utils-json.h"

typedef enum
{
  PIPED_MEDIA_NONE,
  PIPED_MEDIA_VIDEO_STREAM,
  PIPED_MEDIA_AUDIO_STREAM,
} PipedMediaType;

struct _GtuberPiped
{
  GtuberWebsite parent;

  gchar *video_id;

  PipedMediaType media_type;
};

struct _GtuberPipedClass
{
  GtuberWebsiteClass parent_class;
};

#define parent_class gtuber_piped_parent_class
G_DEFINE_TYPE (GtuberPiped, gtuber_piped, GTUBER_TYPE_WEBSITE)

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

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
_read_stream_cb (JsonReader *reader, GtuberMediaInfo *info, GtuberPiped *self)
{
  GtuberAdaptiveStream *astream = NULL;
  GtuberStream *stream;
  GtuberStreamMimeType mime_type;
  GUri *guri;
  const gchar *uri;
  gboolean video_only;

  if (self->media_type == PIPED_MEDIA_VIDEO_STREAM) {
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
  if (self->media_type == PIPED_MEDIA_AUDIO_STREAM || video_only) {
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

  switch (self->media_type) {
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
parse_response_data (GtuberPiped *self, JsonParser *parser,
    GtuberMediaInfo *info, GError **error)
{
  JsonReader *reader = json_reader_new (json_parser_get_root (parser));

  gtuber_media_info_set_id (info, self->video_id);
  gtuber_media_info_set_title (info, gtuber_utils_json_get_string (reader, "title", NULL));
  gtuber_media_info_set_description (info, gtuber_utils_json_get_string (reader, "description", NULL));
  gtuber_media_info_set_duration (info, gtuber_utils_json_get_int (reader, "duration", NULL));

  if (gtuber_utils_json_go_to (reader, "videoStreams", NULL)) {
    self->media_type = PIPED_MEDIA_VIDEO_STREAM;
    gtuber_utils_json_array_foreach (reader, info,
        (GtuberFunc) _read_stream_cb, self);
    gtuber_utils_json_go_back (reader, 1);
  }
  if (gtuber_utils_json_go_to (reader, "audioStreams", NULL)) {
    self->media_type = PIPED_MEDIA_AUDIO_STREAM;
    gtuber_utils_json_array_foreach (reader, info,
        (GtuberFunc) _read_stream_cb, self);
    gtuber_utils_json_go_back (reader, 1);
  }

  g_object_unref (reader);
}

static GtuberFlow
gtuber_piped_create_request (GtuberWebsite *website,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error)
{
  GtuberPiped *self = GTUBER_PIPED (website);
  gchar *uri;

  uri = g_strdup_printf ("https://pipedapi.kavin.rocks/streams/%s", self->video_id);
  *msg = soup_message_new ("GET", uri);
  g_free (uri);

  return GTUBER_FLOW_OK;
}

static GtuberFlow
gtuber_piped_parse_input_stream (GtuberWebsite *website,
    GInputStream *stream, GtuberMediaInfo *info, GError **error)
{
  GtuberPiped *self = GTUBER_PIPED (website);
  JsonParser *parser;

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

  return GTUBER_FLOW_OK;
}

GtuberWebsite *
query_plugin (GUri *uri)
{
  guint uri_match;
  gchar *id;

  if (!gtuber_utils_common_uri_matches_hosts (uri, &uri_match,
      "piped.kavin.rocks",
      NULL))
    return NULL;

  id = gtuber_utils_common_obtain_uri_query_value (uri, "v");
  if (!id)
    id = gtuber_utils_common_obtain_uri_id_from_paths (uri, NULL, "/v/", NULL);

  if (id) {
    GtuberPiped *piped;

    piped = g_object_new (GTUBER_TYPE_PIPED, NULL);
    piped->video_id = id;

    g_debug ("Requested video: %s", piped->video_id);

    return GTUBER_WEBSITE (piped);
  }

  return NULL;
}
