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
  GQL_REQ_NONE,
  GQL_REQ_ACCESS_TOKEN,
  GQL_REQ_ACCESS_TOKEN_CLIP,
  GQL_REQ_METADATA_CHANNEL,
  GQL_REQ_METADATA_VIDEO,
  GQL_REQ_METADATA_CLIP,
} GqlReqType;

typedef enum
{
  TWITCH_MEDIA_NONE,
  TWITCH_MEDIA_CHANNEL,
  TWITCH_MEDIA_VIDEO,
  TWITCH_MEDIA_CLIP,
} TwitchMediaType;

GTUBER_WEBSITE_PLUGIN_EXPORT_HOSTS (
  "twitch.tv",
  NULL
)
GTUBER_WEBSITE_PLUGIN_DECLARE (Twitch, twitch, TWITCH)

struct _GtuberTwitch
{
  GtuberWebsite parent;

  gchar *video_id;

  gchar *access_token;
  gchar *signature;

  TwitchMediaType media_type;
  GqlReqType last_req;

  gboolean download_hls;
};

#define parent_class gtuber_twitch_parent_class
GTUBER_WEBSITE_PLUGIN_DEFINE (Twitch, twitch)

static void gtuber_twitch_finalize (GObject *object);

static GtuberFlow gtuber_twitch_create_request (GtuberWebsite *website,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error);
static GtuberFlow gtuber_twitch_parse_input_stream (GtuberWebsite *website,
    GInputStream *stream, GtuberMediaInfo *info, GError **error);
static GtuberFlow gtuber_twitch_set_user_req_headers (GtuberWebsite *website,
    SoupMessageHeaders *req_headers, GHashTable *user_headers, GError **error);

static void
gtuber_twitch_init (GtuberTwitch *self)
{
  self->media_type = TWITCH_MEDIA_NONE;
  self->last_req = GQL_REQ_NONE;
}

static void
gtuber_twitch_class_init (GtuberTwitchClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtuberWebsiteClass *website_class = (GtuberWebsiteClass *) klass;

  gobject_class->finalize = gtuber_twitch_finalize;

  website_class->handles_input_stream = TRUE;
  website_class->create_request = gtuber_twitch_create_request;
  website_class->parse_input_stream = gtuber_twitch_parse_input_stream;
  website_class->set_user_req_headers = gtuber_twitch_set_user_req_headers;
}

static void
gtuber_twitch_finalize (GObject *object)
{
  GtuberTwitch *self = GTUBER_TWITCH (object);

  g_debug ("Twitch finalize");

  g_free (self->video_id);

  g_free (self->access_token);
  g_free (self->signature);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
_find_error_cb (JsonReader *reader, GtuberMediaInfo *info, const gchar **err_msg)
{
  if (*err_msg)
    return;

  *err_msg = gtuber_utils_json_get_string (reader, "message", NULL);
  if (!*err_msg)
    *err_msg = gtuber_utils_json_get_string (reader, "error", NULL);
}

static void
_read_clip_stream_cb (JsonReader *reader, GtuberMediaInfo *info, GtuberTwitch *self)
{
  const gchar *org_uri;

  org_uri = gtuber_utils_json_get_string (reader, "sourceURL", NULL);
  if (org_uri) {
    GtuberStream *stream = gtuber_stream_new ();
    GPtrArray *streams = gtuber_media_info_get_streams (info);
    const gchar *quality;
    gchar *mod_uri, *query;
    guint itag, fps, height = 0;

    itag = streams->len + 1;
    gtuber_stream_set_itag (stream, itag);
    g_debug ("Created stream, itag %i", itag);

    query = soup_form_encode (
        "sig", self->signature,
        "token", self->access_token,
        NULL);

    mod_uri = g_strjoin ("?", org_uri, query, NULL);
    g_free (query);

    gtuber_stream_set_uri (stream, mod_uri);
    g_debug ("Clip URI: %s", mod_uri);
    g_free (mod_uri);

    fps = gtuber_utils_json_get_int (reader, "frameRate", NULL);
    gtuber_stream_set_fps (stream, fps);
    g_debug ("Clip fps: %i", fps);

    quality = gtuber_utils_json_get_string (reader, "quality", NULL);
    if (quality && strlen (quality)) {
      height = g_ascii_strtod (quality, NULL);
      gtuber_stream_set_height (stream, height);
      g_debug ("Clip height: %i", height);
    }

    gtuber_media_info_add_stream (info, stream);
  }
}

static void
_read_clip_streams (GtuberTwitch *self, JsonReader *reader,
    GtuberMediaInfo *info, GError **error)
{
  gboolean found_streams = FALSE;

  if (gtuber_utils_json_go_to (reader, "data", "clip", "videoQualities", NULL)) {
    found_streams = gtuber_utils_json_array_foreach (reader, info,
        (GtuberFunc) _read_clip_stream_cb, self);
    gtuber_utils_json_go_back (reader, 3);
  }

  if (!found_streams) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Could not find clip streams");
  }
}

static void
_parse_access_token_data (GtuberTwitch *self, JsonParser *parser,
    GtuberMediaInfo *info, GError **error)
{
  JsonReader *reader = json_reader_new (json_parser_get_root (parser));
  const gchar *data_type = NULL;

  switch (self->media_type) {
    case TWITCH_MEDIA_CHANNEL:
      data_type = "streamPlaybackAccessToken";
      break;
    case TWITCH_MEDIA_VIDEO:
      data_type = "videoPlaybackAccessToken";
      break;
    case TWITCH_MEDIA_CLIP:
      data_type = "playbackAccessToken";
      break;
    default:
      g_assert_not_reached ();
  }

  g_free (self->access_token);
  self->access_token = (self->media_type == TWITCH_MEDIA_CLIP)
      ? g_strdup (gtuber_utils_json_get_string (reader, "data", "clip", data_type, "value", NULL))
      : g_strdup (gtuber_utils_json_get_string (reader, "data", data_type, "value", NULL));
  g_debug ("Downloaded access token: %s", self->access_token);

  g_free (self->signature);
  self->signature = (self->media_type == TWITCH_MEDIA_CLIP)
      ? g_strdup (gtuber_utils_json_get_string (reader, "data", "clip", data_type, "signature", NULL))
      : g_strdup (gtuber_utils_json_get_string (reader, "data", data_type, "signature", NULL));
  g_debug ("Downloaded signature: %s", self->signature);

  /* We cannot do anything without access token */
  if (!self->access_token || !self->signature) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Could not parse access token data");
    goto finish;
  }

  /* Clips access token data also contains streams */
  if (self->media_type == TWITCH_MEDIA_CLIP)
    _read_clip_streams (self, reader, info, error);

  /* TODO: Set show_ads to false in access_token */

finish:
  g_object_unref (reader);
}

static void
_read_channel_metadata (JsonReader *reader, GtuberMediaInfo *info, GError **error)
{
  const gchar *id, *title;

  id = gtuber_utils_json_get_string (reader, "data", "user", "lastBroadcast", "id", NULL);
  gtuber_media_info_set_id (info, id);
  g_debug ("Broadcast ID: %s", id);

  title = gtuber_utils_json_get_string (reader, "data", "user", "lastBroadcast", "title", NULL);
  gtuber_media_info_set_title (info, title);
  g_debug ("Broadcast title: %s", title);

  /* Check if channel is still streaming */
  if (!gtuber_utils_json_get_string (reader, "data", "user", "stream", "id", NULL)) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_OTHER,
        "Requested channel is not streaming anymore");
  }
}

static void
_read_video_metadata (JsonReader *reader, GtuberMediaInfo *info, GError **error)
{
  const gchar *id, *title, *description;
  guint duration;

  id = gtuber_utils_json_get_string (reader, "data", "video", "id", NULL);
  gtuber_media_info_set_id (info, id);
  g_debug ("Video ID: %s", id);

  title = gtuber_utils_json_get_string (reader, "data", "video", "title", NULL);
  gtuber_media_info_set_title (info, title);
  g_debug ("Video title: %s", title);

  description = gtuber_utils_json_get_string (reader, "data", "video", "description", NULL);
  gtuber_media_info_set_description (info, description);
  g_debug ("Video description: %s", description);

  duration = gtuber_utils_json_get_int (reader, "data", "video", "lengthSeconds", NULL);
  gtuber_media_info_set_duration (info, duration);
  g_debug ("Video duration: %u", duration);
}

static void
_read_clip_metadata (JsonReader *reader, GtuberMediaInfo *info, GError **error)
{
  const gchar *id, *title;

  id = gtuber_utils_json_get_string (reader, "data", "clip", "id", NULL);
  gtuber_media_info_set_id (info, id);
  g_debug ("Clip ID: %s", id);

  title = gtuber_utils_json_get_string (reader, "data", "clip", "title", NULL);
  gtuber_media_info_set_title (info, title);
  g_debug ("Clip title: %s", title);
}

static void
_parse_metadata (GtuberTwitch *self, JsonParser *parser,
    GtuberMediaInfo *info, GError **error)
{
  JsonReader *reader = json_reader_new (json_parser_get_root (parser));

  if (gtuber_utils_json_go_to (reader, "errors", NULL)) {
    const gchar *err_msg = NULL;

    gtuber_utils_json_array_foreach (reader, NULL,
        (GtuberFunc) _find_error_cb, &err_msg);

    if (!err_msg)
      err_msg = "Could not parse metadata";

    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
    "%s", err_msg);

    gtuber_utils_json_go_back (reader, 1);
  }

  if (!*error) {
    switch (self->media_type) {
      case TWITCH_MEDIA_CHANNEL:
        _read_channel_metadata (reader, info, error);
        break;
      case TWITCH_MEDIA_VIDEO:
        _read_video_metadata (reader, info, error);
        break;
      case TWITCH_MEDIA_CLIP:
        _read_clip_metadata (reader, info, error);
        break;
      default:
        g_assert_not_reached ();
    }
  }
  g_object_unref (reader);
}

static GtuberFlow
parse_json_stream (GtuberTwitch *self, GInputStream *stream,
    GtuberMediaInfo *info, GError **error)
{
  JsonParser *parser;

  parser = json_parser_new ();
  json_parser_load_from_stream (parser, stream, NULL, error);
  if (*error)
    goto finish;

  gtuber_utils_json_parser_debug (parser);

  switch (self->last_req) {
    case GQL_REQ_ACCESS_TOKEN:
    case GQL_REQ_ACCESS_TOKEN_CLIP:
      _parse_access_token_data (self, parser, info, error);
      break;
    case GQL_REQ_METADATA_CHANNEL:
    case GQL_REQ_METADATA_VIDEO:
    case GQL_REQ_METADATA_CLIP:
      _parse_metadata (self, parser, info, error);
      break;
    default:
      g_assert_not_reached ();
  }

finish:
  g_object_unref (parser);

  if (*error)
    return GTUBER_FLOW_ERROR;

  /* Clips do not have HLS manifests, so we are done here */
  if (self->last_req == GQL_REQ_METADATA_CLIP)
    return GTUBER_FLOW_OK;

  return GTUBER_FLOW_RESTART;
}

static void
make_soup_msg (const char *method, const char *uri_string,
    gchar *req_body, SoupMessage **msg)
{
  SoupMessageHeaders *headers;

  *msg = soup_message_new (method, uri_string);
  headers = soup_message_get_request_headers (*msg);

  soup_message_headers_replace (headers, "Referer", "https://player.twitch.tv");
  soup_message_headers_replace (headers, "Origin", "https://player.twitch.tv");
  soup_message_headers_append (headers, "Client-ID", "kimne78kx3ncx6brgo4mv6wki5h1ko");

  if (req_body)
    gtuber_utils_common_msg_take_request (*msg, "application/json", req_body);
}

static GtuberFlow
create_gql_msg (GtuberTwitch *self, GqlReqType req_type,
    SoupMessage **msg, GError **error)
{
  const gchar *op_name, *sha256;
  gchar *req_body, *variables;

  switch (req_type) {
    case GQL_REQ_ACCESS_TOKEN:
      op_name = "PlaybackAccessToken";
      sha256 = "0828119ded1c13477966434e15800ff57ddacf13ba1911c129dc2200705b0712";
      variables = g_strdup_printf (
      "    \"isLive\": %s,\n"
      "    \"login\": \"%s\",\n"
      "    \"isVod\": %s,\n"
      "    \"vodID\": \"%s\",\n"
      "    \"playerType\": \"embed\"\n",
          self->media_type == TWITCH_MEDIA_CHANNEL ? "true" : "false",
          self->media_type == TWITCH_MEDIA_CHANNEL ? self->video_id : "",
          self->media_type == TWITCH_MEDIA_VIDEO ? "true" : "false",
          self->media_type == TWITCH_MEDIA_VIDEO ? self->video_id : "");
      break;
    case GQL_REQ_ACCESS_TOKEN_CLIP:
      op_name = "VideoAccessToken_Clip";
      sha256 = "36b89d2507fce29e5ca551df756d27c1cfe079e2609642b4390aa4c35796eb11";
      variables = g_strdup_printf (
      "    \"slug\": \"%s\"\n",
          self->video_id);
      break;
    case GQL_REQ_METADATA_CHANNEL:
      op_name = "StreamMetadata";
      sha256 = "059c4653b788f5bdb2f5a2d2a24b0ddc3831a15079001a3d927556a96fb0517f";
      variables = g_strdup_printf (
      "    \"channelLogin\": \"%s\"\n",
          self->video_id);
      break;
    case GQL_REQ_METADATA_VIDEO:
      op_name = "VideoMetadata";
      sha256 = "cb3b1eb2f2d2b2f65b8389ba446ec521d76c3aa44f5424a1b1d235fe21eb4806";
      variables = g_strdup_printf (
      "    \"channelLogin\": \"\",\n"
      "    \"videoID\": \"%s\"\n",
          self->video_id);
      break;
    case GQL_REQ_METADATA_CLIP:
      op_name = "ClipsTitle";
      sha256 = "f6cca7f2fdfbfc2cecea0c88452500dae569191e58a265f97711f8f2a838f5b4";
      variables = g_strdup_printf (
      "    \"slug\": \"%s\"\n",
          self->video_id);
      break;
    default:
      goto fail;
  }

  req_body = g_strdup_printf ("{\n"
  "  \"operationName\": \"%s\",\n"
  "  \"extensions\": {\n"
  "    \"persistedQuery\": {\n"
  "      \"version\": 1,\n"
  "      \"sha256Hash\": \"%s\"\n"
  "    }\n"
  "  },\n"
  "  \"variables\": {\n%s"
  "  }\n"
  "}",
      op_name, sha256, variables);

  g_free (variables);

  g_debug ("Request body: %s", req_body);
  make_soup_msg ("POST", "https://gql.twitch.tv/gql", req_body, msg);

  self->last_req = req_type;

  return GTUBER_FLOW_OK;

fail:
  g_set_error (error, GTUBER_WEBSITE_ERROR,
      GTUBER_WEBSITE_ERROR_REQUEST_CREATE_FAILED,
      "Could not assemble GQL request body");

  return GTUBER_FLOW_ERROR;
}

static GtuberFlow
create_hls_msg (GtuberTwitch *self, SoupMessage **msg, GError **error)
{
  gchar *p_id = g_strdup_printf ("%i", (rand () % 9000000) + 1000000);
  gchar *path, *query, *uri_str;

  path = (self->media_type == TWITCH_MEDIA_CHANNEL)
    ? g_strdup_printf ("/api/channel/hls/%s.m3u8", self->video_id)
    : g_strdup_printf ("/vod/%s.m3u8", self->video_id);

  query = soup_form_encode (
      "allow_source", "true",
      "allow_audio_only", "true",
      "allow_spectre", "false",
      "fast_bread", "true",
      "p", p_id,
      "player", "twitchweb",
      "player_backend", "mediaplayer",
      "sig", self->signature,
      "token", self->access_token,
      NULL);

  g_free (p_id);

  uri_str = g_uri_join (G_URI_FLAGS_ENCODED, "https", NULL,
      "usher.ttvnw.net", -1, path, query, NULL);

  g_free (path);
  g_free (query);

  g_debug ("HLS manifest URI: %s", uri_str);
  make_soup_msg ("GET", uri_str, NULL, msg);
  g_free (uri_str);

  self->download_hls = TRUE;

  return GTUBER_FLOW_OK;
}

static GtuberFlow
gtuber_twitch_create_request (GtuberWebsite *website,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error)
{
  GtuberTwitch *self = GTUBER_TWITCH (website);

  if (!self->access_token || !self->signature) {
    GqlReqType req_type = (self->media_type == TWITCH_MEDIA_CLIP)
      ? GQL_REQ_ACCESS_TOKEN_CLIP
      : GQL_REQ_ACCESS_TOKEN;

    return create_gql_msg (self, req_type, msg, error);
  }

  switch (self->last_req) {
    case GQL_REQ_NONE:
    case GQL_REQ_ACCESS_TOKEN:
      return (self->media_type == TWITCH_MEDIA_CHANNEL)
        ? create_gql_msg (self, GQL_REQ_METADATA_CHANNEL, msg, error)
        : create_gql_msg (self, GQL_REQ_METADATA_VIDEO, msg, error);
    case GQL_REQ_ACCESS_TOKEN_CLIP:
      return create_gql_msg (self, GQL_REQ_METADATA_CLIP, msg, error);
    default:
      return create_hls_msg (self, msg, error);
  }
}

static GtuberFlow
gtuber_twitch_parse_input_stream (GtuberWebsite *website,
    GInputStream *stream, GtuberMediaInfo *info, GError **error)
{
  GtuberTwitch *self = GTUBER_TWITCH (website);

  if (self->download_hls) {
    if (gtuber_utils_common_parse_hls_input_stream (stream, info, error))
      return GTUBER_FLOW_OK;

    return GTUBER_FLOW_ERROR;
  }

  return parse_json_stream (self, stream, info, error);
}

static GtuberFlow
gtuber_twitch_set_user_req_headers (GtuberWebsite *website,
    SoupMessageHeaders *req_headers, GHashTable *user_headers, GError **error)
{
  /* Only used for API */
  soup_message_headers_remove (req_headers, "Client-ID");

  return GTUBER_WEBSITE_CLASS (parent_class)->set_user_req_headers (website,
      req_headers, user_headers, error);
}

GtuberWebsite *
plugin_query (GUri *uri)
{
  gint match;
  gchar *id;

  if ((id = gtuber_utils_common_obtain_uri_id_from_paths (uri, &match,
      "/*/clip/",
      "/videos/",
      "/",
      NULL))) {
    GtuberTwitch *twitch = gtuber_twitch_new ();
    twitch->video_id = id;

    switch (match) {
      case 0:
        twitch->media_type = TWITCH_MEDIA_CLIP;
        break;
      case 1:
        twitch->media_type = TWITCH_MEDIA_VIDEO;
        break;
      default:
        twitch->media_type = TWITCH_MEDIA_CHANNEL;
        break;
    }

    g_debug ("Requested type: %i, video: %s",
        twitch->media_type, twitch->video_id);

    return GTUBER_WEBSITE (twitch);
  }

  return NULL;
}
