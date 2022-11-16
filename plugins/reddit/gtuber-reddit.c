/*
 * Copyright (C) 2022 Rafał Dzięgiel <rafostar.github@gmail.com>
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

static void
gtuber_reddit_init (GtuberReddit *self)
{
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

static GtuberFlow
parse_response_data (GtuberReddit *self, JsonParser *parser,
    GtuberMediaInfo *info, GError **error)
{
  JsonReader *reader = json_reader_new (json_parser_get_root (parser));
  const gchar *id_uri;
  GtuberFlow flow = GTUBER_FLOW_OK;

  /* Data is at: res[0].data.children[0].data */
  if (!gtuber_utils_json_go_to (reader,
      GTUBER_UTILS_JSON_ARRAY_INDEX (0), "data", "children",
      GTUBER_UTILS_JSON_ARRAY_INDEX (0), "data", NULL)) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Could not read response JSON data");
    goto finish;
  }

  /* If not a reddit video, take new URI and do a reconfigure with it.
   * Maybe some other plugin will be able to handle it. */
  if (!gtuber_utils_json_get_boolean (reader, "is_video", NULL)) {
    g_debug ("Requested media is not a reddit video");
    flow = GTUBER_FLOW_RECONFIGURE;
  }

  id_uri = gtuber_utils_json_get_string (reader, "url", NULL);
  if (!id_uri)
    id_uri = gtuber_utils_json_get_string (reader, "url_overridden_by_dest", NULL);

  if (id_uri && (flow == GTUBER_FLOW_RECONFIGURE)) {
    g_debug ("Reconfiguring with updated URI: %s", id_uri);
    gtuber_website_set_uri_from_string (GTUBER_WEBSITE (self), id_uri, error);
    goto finish;
  }

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

  /* FIXME: Support parsing DASH files */
  //self->dash_uri = g_strdup (gtuber_utils_json_get_string (reader, "dash_url", NULL));
  self->hls_uri = g_strdup (gtuber_utils_json_get_string (reader, "hls_url", NULL));

  if (!self->dash_uri && !self->hls_uri) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Could not extract any media URIs");
  }

finish:
  g_object_unref (reader);

  return (*error)
      ? GTUBER_FLOW_ERROR
      : flow;
}

static void
update_hls_audio_bitrates (GtuberMediaInfo *info)
{
  GPtrArray *astreams;
  guint i;

  astreams = gtuber_media_info_get_adaptive_streams (info);

  for (i = 0; i < astreams->len; i++) {
    GtuberAdaptiveStream *astream;
    GtuberStream *stream;
    GtuberAdaptiveStreamManifest manifest_type;

    astream = g_ptr_array_index (astreams, i);
    stream = GTUBER_STREAM (astream);
    manifest_type = gtuber_adaptive_stream_get_manifest_type (astream);

    /* Update HLS audio-only streams that do not have bitrate set yet */
    if (manifest_type == GTUBER_ADAPTIVE_STREAM_MANIFEST_HLS
        && gtuber_stream_get_bitrate (stream) == 0
        && gtuber_stream_get_audio_codec (stream)
        && !gtuber_stream_get_video_codec (stream)) {
      gchar **parts;
      const gchar *uri;
      guint j;

      /* Extract bitrate info from URI */
      uri = gtuber_stream_get_uri (stream);
      parts = g_strsplit (uri, "_", 0);

      for (j = 0; parts[j]; j++) {
        guint bitrate;

        if (!g_ascii_isdigit (parts[j][0]))
          continue;

        /* Kb/s -> bit/s */
        bitrate = 1000 * g_ascii_strtoull (parts[j], NULL, 10);
        gtuber_stream_set_bitrate (stream, bitrate);

        g_debug ("Updated HLS audio itag %u bitrate: %u",
            gtuber_stream_get_itag (stream),
            gtuber_stream_get_bitrate (stream));
        break;
      }

      g_strfreev (parts);
    }
  }
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
  GtuberFlow flow = GTUBER_FLOW_OK;

  if (self->dash_uri) {
    /* FIXME: Support parsing DASH files */

    g_free (self->dash_uri);
    self->dash_uri = NULL;
    goto finish;
  }

  if (self->hls_uri) {
    if (gtuber_utils_common_parse_hls_input_stream_with_base_uri (stream,
        info, self->hls_uri, error))
      update_hls_audio_bitrates (info);

    g_free (self->hls_uri);
    self->hls_uri = NULL;
    goto finish;
  }

  parser = json_parser_new ();
  if (json_parser_load_from_stream (parser, stream, NULL, error)) {
    gtuber_utils_json_parser_debug (parser);
    flow = parse_response_data (self, parser, info, error);
  }
  g_object_unref (parser);

finish:
  if (*error)
    return GTUBER_FLOW_ERROR;
  if (self->dash_uri || self->hls_uri)
    return GTUBER_FLOW_RESTART;

  return flow;
}

static void
gtuber_reddit_class_init (GtuberRedditClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtuberWebsiteClass *website_class = (GtuberWebsiteClass *) klass;

  gobject_class->finalize = gtuber_reddit_finalize;

  website_class->prepare = gtuber_reddit_prepare;
  website_class->create_request = gtuber_reddit_create_request;
  website_class->read_response = gtuber_reddit_read_response;
  website_class->parse_input_stream = gtuber_reddit_parse_input_stream;
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
