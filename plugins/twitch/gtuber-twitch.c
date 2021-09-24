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

#include "gtuber-twitch.h"
#include "utils/common/gtuber-utils-common.h"
#include "utils/json/gtuber-utils-json.h"

#define TWITCH_CLIENT_ID "kimne78kx3ncx6brgo4mv6wki5h1ko"

typedef enum
{
  GQL_REQ_NONE,
  GQL_REQ_ACCESS_TOKEN,
  GQL_REQ_METADATA_CHANNEL,
  GQL_REQ_METADATA_VIDEO,
} GqlReqType;

struct _GtuberTwitch
{
  GtuberWebsite parent;

  gchar *channel_id;
  gchar *video_id;

  gchar *access_token;
  gchar *signature;

  GqlReqType last_req;
  gboolean download_hls;
};

struct _GtuberTwitchClass
{
  GtuberWebsiteClass parent_class;
};

#define parent_class gtuber_twitch_parent_class
G_DEFINE_TYPE (GtuberTwitch, gtuber_twitch, GTUBER_TYPE_WEBSITE)

static void gtuber_twitch_finalize (GObject *object);

static GtuberFlow gtuber_twitch_create_request (GtuberWebsite *website,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error);
static GtuberFlow gtuber_twitch_parse_input_stream (GtuberWebsite *website,
    GInputStream *stream, GtuberMediaInfo *info, GError **error);

static void
gtuber_twitch_init (GtuberTwitch *self)
{
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
}

static void
gtuber_twitch_finalize (GObject *object)
{
  GtuberTwitch *self = GTUBER_TWITCH (object);

  g_debug ("Twitch finalize");

  g_free (self->channel_id);
  g_free (self->video_id);

  g_free (self->access_token);
  g_free (self->signature);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
_parse_access_token_data (GtuberTwitch *self, JsonParser *parser, GError **error)
{
  //const GtuberCache *cache;
  JsonReader *reader = json_reader_new (json_parser_get_root (parser));
  gchar *data_type = g_strdup_printf ("%sPlaybackAccessToken",
      self->channel_id ? "stream" : "video");

  g_free (self->access_token);
  self->access_token = g_strdup (gtuber_utils_json_get_string (reader,
      "data", data_type, "value", NULL));
  g_debug ("Downloaded access token: %s", self->access_token);

  g_free (self->signature);
  self->signature = g_strdup (gtuber_utils_json_get_string (reader,
      "data", data_type, "signature", NULL));
  g_debug ("Downloaded signature: %s", self->signature);

  g_free (data_type);
  g_object_unref (reader);

  /* We cannot do anything without access token */
  if (!self->access_token || !self->signature) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Could not parse access token data");
    return;
  }

  /* TODO: Set show_ads to false in access_token */

  /* TODO: Store to cache
  cache = gtuber_cache_get ();
  gtuber_cache_store_value (cache, "access-token", self->access_token);
  gtuber_cache_store_value (cache, "signature", self->signature);
  */
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
  guint64 duration;

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
  g_debug ("Video duration: %ld", duration);
}

static void
_parse_metadata (GtuberTwitch *self, JsonParser *parser,
    GtuberMediaInfo *info, GError **error)
{
  JsonReader *reader = json_reader_new (json_parser_get_root (parser));

  if (!json_reader_is_array (reader)) {
    if (self->channel_id)
      _read_channel_metadata (reader, info, error);
    else
      _read_video_metadata (reader, info, error);
  } else {
    gint i, count = json_reader_count_elements (reader);
    for (i = 0; i < count; i++) {
      if (json_reader_read_element (reader, i)) {
        if (json_reader_read_member (reader, "error")) {
          g_set_error (error, GTUBER_WEBSITE_ERROR,
              GTUBER_WEBSITE_ERROR_PARSE_FAILED,
              "%s", json_reader_get_string_value (reader));
          break;
        }
        json_reader_end_member (reader);
      }
      json_reader_end_element (reader);
    }
    if (!*error) {
      g_set_error (error, GTUBER_WEBSITE_ERROR,
          GTUBER_WEBSITE_ERROR_PARSE_FAILED,
          "Could not parse metadata");
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
      _parse_access_token_data (self, parser, error);
      break;
    case GQL_REQ_METADATA_CHANNEL:
    case GQL_REQ_METADATA_VIDEO:
      _parse_metadata (self, parser, info, error);
      break;
    default:
      g_assert_not_reached ();
  }

finish:
  g_object_unref (parser);

  if (*error)
    return GTUBER_FLOW_ERROR;

  return GTUBER_FLOW_RESTART;
}

static void
make_soup_msg (const char *method, const char *uri_string,
    gchar *req_body, SoupMessage **msg)
{
  SoupMessageHeaders *headers;

  *msg = soup_message_new (method, uri_string);
  headers = (*msg)->request_headers;

  soup_message_headers_replace (headers, "Referer", "https://player.twitch.tv");
  soup_message_headers_replace (headers, "Origin", "https://player.twitch.tv");
  soup_message_headers_append (headers, "Client-ID", TWITCH_CLIENT_ID);

  if (req_body) {
    soup_message_set_request (*msg, "application/json",
        SOUP_MEMORY_TAKE, req_body, strlen (req_body));
  }
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
          self->channel_id != NULL ? "true" : "false",
          self->channel_id ? self->channel_id : "",
          self->video_id != NULL ? "true" : "false",
          self->video_id ? self->video_id : "");
      break;
    case GQL_REQ_METADATA_CHANNEL:
      op_name = "StreamMetadata";
      sha256 = "059c4653b788f5bdb2f5a2d2a24b0ddc3831a15079001a3d927556a96fb0517f";
      variables = g_strdup_printf (
      "    \"channelLogin\": \"%s\"\n",
          self->channel_id);
      break;
    case GQL_REQ_METADATA_VIDEO:
      op_name = "VideoMetadata";
      sha256 = "cb3b1eb2f2d2b2f65b8389ba446ec521d76c3aa44f5424a1b1d235fe21eb4806";
      variables = g_strdup_printf (
      "    \"channelLogin\": \"\",\n"
      "    \"videoID\": \"%s\"\n",
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
  SoupURI *uri = soup_uri_new ("https://usher.ttvnw.net");
  gchar *p_id = g_strdup_printf ("%i", (rand () % 9000000) + 1000000);
  gchar *path, *uri_str;

  path = (self->channel_id)
    ? g_strdup_printf ("/api/channel/hls/%s.m3u8", self->channel_id)
    : g_strdup_printf ("/vod/%s.m3u8", self->video_id);
  soup_uri_set_path (uri, path);
  g_free (path);

  soup_uri_set_query_from_fields (uri,
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

  uri_str = soup_uri_to_string (uri, FALSE);
  soup_uri_free (uri);

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

  /* TODO: Implement caching
  if (!self->access_token || !self->signature) {
    const GtuberCache *cache = gtuber_cache_get ();

    if (!self->access_token)
      gtuber_cache_restore_value (cache, "access-token", &self->access_token);
    if (!self->signature)
      gtuber_cache_restore_value (cache, "signature", &self->signature);
  }
  */
  if (!self->access_token || !self->signature)
    return create_gql_msg (self, GQL_REQ_ACCESS_TOKEN, msg, error);

  switch (self->last_req) {
    case GQL_REQ_NONE:
    case GQL_REQ_ACCESS_TOKEN:
      return (self->channel_id)
        ? create_gql_msg (self, GQL_REQ_METADATA_CHANNEL, msg, error)
        : create_gql_msg (self, GQL_REQ_METADATA_VIDEO, msg, error);
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

GtuberWebsite *
query_plugin (GUri *uri)
{
  GtuberTwitch *twitch = NULL;
  const gchar *host, *path;
  gchar **parts;

  host = g_uri_get_host (uri);

  if (!g_str_has_suffix (host, "twitch.tv"))
    goto fail;

  path = g_uri_get_path (uri);
  if (!path)
    goto fail;

  g_debug ("URI path: %s", path);

  parts = g_strsplit (path, "/", 4);
  if (parts[1]) {
    if (!parts[2]) {
      twitch = g_object_new (GTUBER_TYPE_TWITCH, NULL);
      twitch->channel_id = g_strdup (parts[1]);
      g_debug ("Requested live channel: %s", twitch->channel_id);
    } else if (!parts[3] && strcmp (parts[1], "videos") == 0) {
      twitch = g_object_new (GTUBER_TYPE_TWITCH, NULL);
      twitch->video_id = g_strdup (parts[2]);
      g_debug ("Requested video: %s", twitch->video_id);
    }
  }
  g_strfreev (parts);

  if (twitch)
    return GTUBER_WEBSITE (twitch);

fail:
  return NULL;
}
