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

GTUBER_WEBSITE_PLUGIN_EXPORT_HOSTS (
  "reddit.com",
  "redditmedia.com",
  NULL
)

GTUBER_WEBSITE_PLUGIN_DECLARE (Reddit, reddit, REDDIT)

struct _GtuberReddit
{
  GtuberWebsite parent;

  gchar *dash_uri;
  gchar *hls_uri;

  gchar *cookie_str;
};

#define parent_class gtuber_reddit_parent_class
GTUBER_WEBSITE_PLUGIN_DEFINE (Reddit, reddit)

static void gtuber_reddit_finalize (GObject *object);

static void gtuber_reddit_prepare (GtuberWebsite *website);
static GtuberFlow gtuber_reddit_create_request (GtuberWebsite *website,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error);
static GtuberFlow gtuber_reddit_read_response (GtuberWebsite *website,
    SoupMessage *msg, GError **error);
static GtuberFlow gtuber_reddit_parse_input_stream (GtuberWebsite *website,
    GInputStream *stream, GtuberMediaInfo *info, GError **error);

static void
gtuber_reddit_init (GtuberReddit *self)
{
}

static void
gtuber_reddit_class_init (GtuberRedditClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtuberWebsiteClass *website_class = (GtuberWebsiteClass *) klass;

  gobject_class->finalize = gtuber_reddit_finalize;

  website_class->handles_input_stream = TRUE;
  website_class->prepare = gtuber_reddit_prepare;
  website_class->create_request = gtuber_reddit_create_request;
  website_class->read_response = gtuber_reddit_read_response;
  website_class->parse_input_stream = gtuber_reddit_parse_input_stream;
}

static void
gtuber_reddit_finalize (GObject *object)
{
  GtuberReddit *self = GTUBER_REDDIT (object);

  g_debug ("Reddit finalize");

  g_free (self->dash_uri);
  g_free (self->hls_uri);

  g_free (self->cookie_str);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
parse_response_data (GtuberReddit *self, JsonParser *parser,
    GtuberMediaInfo *info, GError **error)
{
  JsonReader *reader = json_reader_new (json_parser_get_root (parser));
  const gchar *id_uri, *direct_uri;

  /* Data is at: res[0].data.children[0].data */
  if (!json_reader_is_array (reader)
      || !json_reader_read_element (reader, 0)
      || !gtuber_utils_json_go_to (reader, "data", "children", NULL)
      || !json_reader_is_array (reader)
      || !json_reader_read_element (reader, 0)
      || !gtuber_utils_json_go_to (reader, "data", NULL)) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Could not read response JSON data");
    goto finish;
  }

  if (!gtuber_utils_json_get_boolean (reader, "is_video", NULL)) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Requested media is not a video");
    goto finish;
  }

  id_uri = gtuber_utils_json_get_string (reader, "url", NULL);
  if (!id_uri)
    id_uri = gtuber_utils_json_get_string (reader, "url_overridden_by_dest", NULL);

  if (!id_uri || !g_str_has_prefix (id_uri, "https://v.redd.it/")) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Data is missing reddit video ID");
    goto finish;
  }

  gtuber_media_info_set_id (info, id_uri + 18);
  gtuber_media_info_set_title (info, gtuber_utils_json_get_string (reader, "title", NULL));

  if (!gtuber_utils_json_go_to (reader, "media", "reddit_video", NULL)
      && !gtuber_utils_json_go_to (reader, "secure_media", "reddit_video", NULL)) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Data is missing reddit video info");
    goto finish;
  }

  gtuber_media_info_set_duration (info, gtuber_utils_json_get_int (reader, "duration", NULL));

  if ((direct_uri = gtuber_utils_json_get_string (reader, "fallback_url", NULL))) {
    GtuberStream *stream = gtuber_stream_new ();

    gtuber_stream_set_uri (stream, direct_uri);
    gtuber_stream_set_bitrate (stream, gtuber_utils_json_get_int (reader, "bitrate_kbps", NULL));
    gtuber_stream_set_width (stream, gtuber_utils_json_get_int (reader, "width", NULL));
    gtuber_stream_set_height (stream, gtuber_utils_json_get_int (reader, "height", NULL));
    gtuber_stream_set_itag (stream, 200);

    /* XXX: Reddit fallback URI is always "avc1" + "mp4a" AFAIK */
    gtuber_stream_set_codecs (stream, "avc1", "mp4a");

    gtuber_media_info_add_stream (info, stream);
  }

  /* FIXME: Support parsing DASH files */
  //self->dash_uri = g_strdup (gtuber_utils_json_get_string (reader, "dash_url", NULL));
  self->hls_uri = g_strdup (gtuber_utils_json_get_string (reader, "hls_url", NULL));

  if (!direct_uri && !self->dash_uri && !self->hls_uri) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Could not extract any media URIs");
  }

finish:
  g_object_unref (reader);
}

static void
gtuber_reddit_prepare (GtuberWebsite *website)
{
  GtuberReddit *self = GTUBER_REDDIT (website);

  self->cookie_str = gtuber_reddit_cache_read ("cookie_str");
}

static GtuberFlow
gtuber_reddit_create_request (GtuberWebsite *website,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error)
{
  GtuberReddit *self = GTUBER_REDDIT (website);
  SoupMessageHeaders *headers;
  gboolean needs_cookies = FALSE;

  if (!self->dash_uri && !self->hls_uri) {
    GUri *guri, *website_uri;
    gchar *path;

    website_uri = gtuber_website_get_uri (website);
    needs_cookies = TRUE;

    path = g_strdup_printf ("%s.json", g_uri_get_path (website_uri));
    guri = g_uri_build (G_URI_FLAGS_ENCODED,
        "https", NULL,
        g_uri_get_host (website_uri),
        g_uri_get_port (website_uri),
        path, NULL, NULL);
    g_free (path);

    *msg = soup_message_new_from_uri ("GET", guri);
    g_uri_unref (guri);
  } else if (self->dash_uri) {
    *msg = soup_message_new ("GET", self->dash_uri);
  } else if (self->hls_uri) {
    *msg = soup_message_new ("GET", self->hls_uri);
  }

  headers = soup_message_get_request_headers (*msg);
  soup_message_headers_replace (headers, "Origin", "https://www.reddit.com");
  soup_message_headers_replace (headers, "Referer", "https://www.reddit.com/");

  if (needs_cookies && self->cookie_str) {
    soup_message_headers_replace (headers, "Cookie", self->cookie_str);
    g_debug ("Using cookie: %s", self->cookie_str);
  }

  return GTUBER_FLOW_OK;
}

static GtuberFlow
gtuber_reddit_read_response (GtuberWebsite *website,
    SoupMessage *msg, GError **error)
{
  GtuberReddit *self = GTUBER_REDDIT (website);

  if (!self->cookie_str) {
    GSList *cookies;

    if (!(cookies = soup_cookies_from_response (msg))) {
      g_debug ("No cookies in response");
      return GTUBER_FLOW_OK;
    }

    if ((self->cookie_str = soup_cookies_to_cookie_header (cookies)))
      gtuber_reddit_cache_write ("cookie_str", self->cookie_str, 3600);

    soup_cookies_free (cookies);
  }

  return GTUBER_FLOW_OK;
}

static GtuberFlow
gtuber_reddit_parse_input_stream (GtuberWebsite *website,
    GInputStream *stream, GtuberMediaInfo *info, GError **error)
{
  GtuberReddit *self = GTUBER_REDDIT (website);
  JsonParser *parser;

  if (self->dash_uri) {
    /* FIXME: Support parsing DASH files */

    g_free (self->dash_uri);
    self->dash_uri = NULL;
    goto finish;
  }

  if (self->hls_uri) {
    gtuber_utils_common_parse_hls_input_stream_with_base_uri (stream,
        info, self->hls_uri, error);

    g_free (self->hls_uri);
    self->hls_uri = NULL;
    goto finish;
  }

  parser = json_parser_new ();
  if (json_parser_load_from_stream (parser, stream, NULL, error)) {
    gtuber_utils_json_parser_debug (parser);
    parse_response_data (self, parser, info, error);
  }
  g_object_unref (parser);

finish:
  if (*error)
    return GTUBER_FLOW_ERROR;
  if (self->dash_uri || self->hls_uri)
    return GTUBER_FLOW_RESTART;

  return GTUBER_FLOW_OK;
}

GtuberWebsite *
plugin_query (GUri *uri)
{
  GtuberReddit *reddit;
  const gchar *path = g_uri_get_path (uri);

  /* Assume that valid URI path starts with "/r/" */
  if (!path || !g_str_has_prefix (path, "/r/"))
    return NULL;

  reddit = gtuber_reddit_new ();
  g_debug ("Requested video: %s", path);

  return GTUBER_WEBSITE (reddit);
}
