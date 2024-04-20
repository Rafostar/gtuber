/*
 * Copyright (C) 2021 Rafał Dzięgiel <rafostar.github@gmail.com>
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
#include "utils/xml/gtuber-utils-xml.h"
#include "utils/youtube/gtuber-utils-youtube.h"

#define GTUBER_YOUTUBE_ANDROID_CLI_VER "18.22.37"
#define GTUBER_YOUTUBE_ANDROID_PARAMS "CgIIAdgDAQ%3D%3D"
#define GTUBER_YOUTUBE_ANDROID_MAJOR "13"
#define GTUBER_YOUTUBE_ANDROID_SDK_MAJOR 33

#define GTUBER_YOUTUBE_IOS_CLI_VER "18.20.3"
#define GTUBER_YOUTUBE_IOS_DEVICE "iPhone14,3"
#define GTUBER_YOUTUBE_IOS_SYS_VER "15_6"

#define GTUBER_YOUTUBE_X_ORIGIN "https://www.youtube.com"

#define ANDROID_UA_FORMAT \
    "%s/%s (Linux; U; Android " GTUBER_YOUTUBE_ANDROID_MAJOR "; %s;" \
    " sdk_gphone64_x86_64 Build/UPB4.230623.005) gzip"

#define IOS_UA_FORMAT \
    "%s/%s (" GTUBER_YOUTUBE_IOS_DEVICE "; U; CPU iOS " GTUBER_YOUTUBE_IOS_SYS_VER " like Mac OS X; %s)"

/* XXX: Must be updated when client params are added */
#define MAX_ROOT_PARAMS 1
#define MAX_CLIENT_PARAMS 1

GTUBER_WEBSITE_PLUGIN_EXPORT_HOSTS (
  "youtube.com",
  "youtu.be",
  NULL
)
GTUBER_WEBSITE_PLUGIN_DECLARE (Youtube, youtube, YOUTUBE)

typedef enum
{
  YOUTUBE_GET_VIDEO_ID = 1,
  YOUTUBE_GET_API_DATA,
  YOUTUBE_GET_HLS,
  YOUTUBE_EXTRACTION_FINISH,
} YoutubeStep;

struct _GtuberYoutube
{
  GtuberWebsite parent;

  gchar *video_id;
  gchar *hls_uri;

  gchar *visitor_data;
  gchar *locale;
  gchar *ua;

  gboolean use_mod_uri;

  YoutubeStep step;
  guint client;
};

#define parent_class gtuber_youtube_parent_class
GTUBER_WEBSITE_PLUGIN_DEFINE (Youtube, youtube)

struct GtuberYoutubeClient
{
  const gchar *name;
  const gchar *id;
  const gchar *app;
  const gchar *version;
  const gchar *api_key;
  const gchar *root_params[2 * MAX_ROOT_PARAMS + 1];
  const gchar *client_params[2 * MAX_CLIENT_PARAMS + 1];
};

struct GtuberYoutubeClient clients[] =
{
  {
    .name = "IOS",
    .id = "5",
    .app = "com.google.ios.youtube",
    .version = GTUBER_YOUTUBE_IOS_CLI_VER,
    .api_key = "AIzaSyB-63vPrdThhKuerbB2N_l7Kwwcxj6yUAc",
    .root_params = { NULL },
    .client_params = {
      "deviceModel", GTUBER_YOUTUBE_IOS_DEVICE, NULL
    }
  },
  {
    .name = "IOS_MESSAGES_EXTENSION", // iOS Embedded
    .id = "66",
    .app = "com.google.ios.youtube",
    .version = GTUBER_YOUTUBE_IOS_CLI_VER,
    .api_key = "AIzaSyDCU8hByM-4DrUqRUYnGn-3llEO78bcxq8",
    .root_params = { NULL },
    .client_params = {
      "deviceModel", GTUBER_YOUTUBE_IOS_DEVICE, NULL
    }
  },
  {
    .name = "ANDROID", // ANDROID with EMBED clientScreen
    .id = "3",
    .app = "com.google.android.youtube",
    .version = GTUBER_YOUTUBE_ANDROID_CLI_VER,
    .api_key = "AIzaSyA8eiZmM1FaDVjRy-df2KTyQ_vz_yYM39w",
    .root_params = {
      "params", GTUBER_YOUTUBE_ANDROID_PARAMS, NULL
    },
    .client_params = {
      "clientScreen", "EMBED", NULL
    }
  },
  {
    .name = "ANDROID",
    .id = "3",
    .app = "com.google.android.youtube",
    .version = GTUBER_YOUTUBE_ANDROID_CLI_VER,
    .api_key = "AIzaSyA8eiZmM1FaDVjRy-df2KTyQ_vz_yYM39w",
    .root_params = {
      "params", GTUBER_YOUTUBE_ANDROID_PARAMS, NULL
    },
    .client_params = { NULL }
  },
  {
    .name = "ANDROID_EMBEDDED_PLAYER",
    .id = "55",
    .app = "com.google.android.youtube",
    .version = GTUBER_YOUTUBE_ANDROID_CLI_VER,
    .api_key = "AIzaSyCjc_pVEDi4qsv5MtC2dMXzpIaDoRFLsxw",
    .root_params = {
      "params", GTUBER_YOUTUBE_ANDROID_PARAMS, NULL
    },
    .client_params = { NULL }
  }
};

static void
gtuber_youtube_update_user_agent (GtuberYoutube *self)
{
  const gchar *format_str = NULL;

  if (g_str_has_prefix (clients[self->client].name, "ANDROID"))
    format_str = ANDROID_UA_FORMAT;
  else if (g_str_has_prefix (clients[self->client].name, "IOS"))
    format_str = IOS_UA_FORMAT;
  else
    g_assert_not_reached ();

  g_free (self->ua);
  self->ua = g_strdup_printf (format_str,
      clients[self->client].app, clients[self->client].version, self->locale);

  g_debug ("Using User-Agent: %s", self->ua);
}

static void
gtuber_youtube_init (GtuberYoutube *self)
{
  self->step = YOUTUBE_GET_VIDEO_ID;
}

static void
gtuber_youtube_finalize (GObject *object)
{
  GtuberYoutube *self = GTUBER_YOUTUBE (object);

  g_debug ("Youtube finalize");

  g_free (self->video_id);
  g_free (self->hls_uri);

  g_free (self->visitor_data);
  g_free (self->locale);
  g_free (self->ua);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gchar *
_modify_uri (const gchar *uri_str)
{
  GUri *parsed_uri;
  gchar *tmp_uri = NULL, *mod_uri;

  g_debug ("Parsing URI: %s", uri_str);

  if ((parsed_uri = g_uri_parse (uri_str, G_URI_FLAGS_ENCODED, NULL))) {
    const gchar *query = g_uri_get_query (parsed_uri);
    gchar *mod_host = NULL;

    if (G_LIKELY (query != NULL)) {
      GHashTable *params;
      const gchar *mn, *fvip;

      params = g_uri_parse_params (query, -1,
          "&", G_URI_PARAMS_NONE, NULL);

      mn = g_hash_table_lookup (params, "mn");
      fvip = g_hash_table_lookup (params, "fvip");

      if (mn && fvip) {
        gchar **mnstrv = g_strsplit (mn, ",", 0);

        if (g_strv_length (mnstrv) == 2)
          mod_host = g_strdup_printf ("rr%s---%s.googlevideo.com", fvip, mnstrv[1]);

        g_strfreev (mnstrv);
      }

      g_hash_table_unref (params);
    }

    if (mod_host) {
      tmp_uri = g_uri_join (G_URI_FLAGS_ENCODED,
          g_uri_get_scheme (parsed_uri),
          NULL,
          mod_host,
          -1,
          g_uri_get_path (parsed_uri),
          query,
          NULL);

      g_debug ("Host change: %s -> %s",
          g_uri_get_host (parsed_uri), mod_host);

      g_free (mod_host);
    }

    g_uri_unref (parsed_uri);
  }

  if (G_LIKELY (tmp_uri != NULL)) {
    mod_uri = gtuber_utils_common_obtain_uri_with_query_as_path (tmp_uri);
    g_free (tmp_uri);
  } else {
    /* If we failed parsing, just use original URI */
    mod_uri = gtuber_utils_common_obtain_uri_with_query_as_path (uri_str);
  }

  g_debug ("Final URI: %s", mod_uri);

  return mod_uri;
}

static void
_read_stream_info (GtuberYoutube *self, JsonReader *reader, GtuberStream *stream)
{
  const gchar *uri_str, *yt_mime;
  gchar *final_uri;

  uri_str = gtuber_utils_json_get_string (reader, "url", NULL);

  /* No point continuing without URI */
  if (!uri_str)
    return;

  final_uri = (!self->use_mod_uri)
      ? gtuber_utils_common_obtain_uri_with_query_as_path (uri_str)
      : _modify_uri (uri_str);

  gtuber_stream_set_uri (stream, final_uri);
  g_free (final_uri);

  gtuber_stream_set_itag (stream, gtuber_utils_json_get_int (reader, "itag", NULL));
  gtuber_stream_set_bitrate (stream, gtuber_utils_json_get_int (reader, "bitrate", NULL));
  gtuber_stream_set_width (stream, gtuber_utils_json_get_int (reader, "width", NULL));
  gtuber_stream_set_height (stream, gtuber_utils_json_get_int (reader, "height", NULL));
  gtuber_stream_set_fps (stream, gtuber_utils_json_get_int (reader, "fps", NULL));

  /* Parse mime type and codecs */
  if ((yt_mime = gtuber_utils_json_get_string (reader, "mimeType", NULL))) {
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

static void
_read_stream_cb (JsonReader *reader, GtuberMediaInfo *info, gpointer user_data)
{
  GtuberYoutube *self = GTUBER_YOUTUBE (user_data);
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
  _read_stream_info (self, reader, stream);

  gtuber_media_info_add_stream (info, stream);
}

static void
_read_adaptive_stream_cb (JsonReader *reader, GtuberMediaInfo *info, gpointer user_data)
{
  GtuberYoutube *self = GTUBER_YOUTUBE (user_data);
  GtuberAdaptiveStream *astream;
  const gchar *stream_type;

  stream_type = gtuber_utils_json_get_string (reader, "type", NULL);

  if (!g_strcmp0 (stream_type, "FORMAT_STREAM_TYPE_OTF")) {
    /* FIXME: OTF requires fetching init at "/sq/0" first
     * then remaining fragments by number instead of range */
    return;
  }

  astream = gtuber_adaptive_stream_new ();
  _read_stream_info (self, reader, GTUBER_STREAM (astream));

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

static GtuberFlow
parse_api_data (GtuberYoutube *self, GInputStream *stream,
    GtuberMediaInfo *info, GError **error)
{
  JsonParser *parser;
  JsonReader *reader = NULL;
  const gchar *status, *video_id, *visitor_data;
  GtuberFlow flow = GTUBER_FLOW_OK;
  gboolean is_live = FALSE;

  parser = json_parser_new ();
  json_parser_load_from_stream (parser, stream, NULL, error);
  if (*error)
    goto finish;

  gtuber_utils_json_parser_debug (parser);
  reader = json_reader_new (json_parser_get_root (parser));

  /* Check if video is playable */
  status = gtuber_utils_json_get_string (reader, "playabilityStatus", "status", NULL);
  video_id = gtuber_utils_json_get_string (reader, "videoDetails", "videoId", NULL);
  if (g_strcmp0 (status, "OK") != 0
      || g_strcmp0 (video_id, self->video_id) != 0) {
    if (G_N_ELEMENTS (clients) > self->client + 1) {
      g_debug ("Video is not playable, trying again with a different client...");

      self->client++;
      gtuber_youtube_update_user_agent (self);

      flow = GTUBER_FLOW_RESTART;
    } else {
      const gchar *reason;

      reason = gtuber_utils_json_get_string (reader, "playabilityStatus", "reason", NULL);

      g_set_error (error, GTUBER_WEBSITE_ERROR,
          GTUBER_WEBSITE_ERROR_OTHER, "%s",
          (reason != NULL) ? reason : "Video is not playable");
    }
    goto finish;
  }

  gtuber_media_info_set_id (info, video_id);

  if (gtuber_utils_json_go_to (reader, "videoDetails", NULL)) {
    const gchar *title, *desc, *duration;

    title = gtuber_utils_json_get_string (reader, "title", NULL);
    gtuber_media_info_set_title (info, title);

    desc = gtuber_utils_json_get_string (reader, "shortDescription", NULL);
    gtuber_media_info_set_description (info, desc);
    gtuber_utils_youtube_insert_chapters_from_description (info, desc);

    duration = gtuber_utils_json_get_string (reader, "lengthSeconds", NULL);
    if (duration)
      gtuber_media_info_set_duration (info, g_ascii_strtoull (duration, NULL, 10));

    is_live = gtuber_utils_json_get_boolean (reader, "isLiveContent", NULL);

    gtuber_utils_json_go_back (reader, 1);
  }

  if (gtuber_utils_json_go_to (reader, "streamingData", NULL)) {
    if (is_live)
      self->hls_uri = g_strdup (gtuber_utils_json_get_string (reader, "hlsManifestUrl", NULL));

    if (!self->hls_uri) {
      if (gtuber_utils_json_go_to (reader, "formats", NULL)) {
        gtuber_utils_json_array_foreach (reader, info,
            (GtuberFunc) _read_stream_cb, self);
        gtuber_utils_json_go_back (reader, 1);
      }
      if (gtuber_utils_json_go_to (reader, "adaptiveFormats", NULL)) {
        gtuber_utils_json_array_foreach (reader, info,
            (GtuberFunc) _read_adaptive_stream_cb, self);
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

    gtuber_youtube_cache_write ("visitor_data", self->visitor_data, 24 * 3600);
  }

finish:
  if (*error)
    flow = GTUBER_FLOW_ERROR;

  g_clear_object (&reader);
  g_clear_object (&parser);

  return flow;
}

static gchar *
obtain_video_id (GInputStream *stream, GError **error)
{
  JsonParser *parser;
  xmlDoc *doc;
  gchar *data, *json_str, *video_id = NULL;

  if (!(data = gtuber_utils_common_input_stream_to_data (stream, error)))
    goto finish;

  doc = gtuber_utils_xml_load_html_from_data (data, error);
  g_free (data);

  if (!doc)
    goto finish;

  g_debug ("Downloaded HTML");

  json_str = gtuber_utils_xml_obtain_json_in_node (doc, "ytInitialPlayerResponse");
  xmlFreeDoc (doc);

  if (!json_str)
    goto finish;

  parser = json_parser_new ();
  if ((json_parser_load_from_data (parser, json_str, -1, error))) {
    JsonReader *reader;

    g_debug ("Got initial response JSON");
    gtuber_utils_json_parser_debug (parser);

    reader = json_reader_new (json_parser_get_root (parser));
    video_id = g_strdup (gtuber_utils_json_get_string (reader, "playabilityStatus",
        "liveStreamability", "liveStreamabilityRenderer", "videoId", NULL));

    g_object_unref (reader);
  }
  g_object_unref (parser);

  g_free (json_str);

finish:
  if (!video_id && *error == NULL) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Could not extract video ID");
  }
  return video_id;
}

static gchar *
obtain_player_req_body (GtuberYoutube *self)
{
  gchar *req_body, **parts;
  guint i;

  parts = g_strsplit (self->locale, "_", 0);

  GTUBER_UTILS_JSON_BUILD_OBJECT (&req_body, {
    GTUBER_UTILS_JSON_ADD_NAMED_OBJECT ("context", {
      GTUBER_UTILS_JSON_ADD_NAMED_OBJECT ("client", {
        GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("clientName", clients[self->client].name);
        GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("clientVersion", clients[self->client].version);
        if (g_str_has_prefix (clients[self->client].name, "ANDROID")) {
          GTUBER_UTILS_JSON_ADD_KEY_VAL_INT ("androidSdkVersion", GTUBER_YOUTUBE_ANDROID_SDK_MAJOR);
        }
        GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("userAgent", self->ua);
        GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("hl", parts[0]);
        GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("gl", parts[1]);
        GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("visitorData", self->visitor_data);
        GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("timeZone", "UTC"); // the same time as in "SAPISID"
        GTUBER_UTILS_JSON_ADD_KEY_VAL_INT ("utcOffsetMinutes", 0);

        for (i = 0; clients[self->client].client_params[i]; i += 2) {
          GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING (
              clients[self->client].client_params[i],
              clients[self->client].client_params[i + 1]);
        }
      });
      GTUBER_UTILS_JSON_ADD_NAMED_OBJECT ("thirdParty", {
        gchar *embed_url;

        embed_url = g_strdup_printf ("https://www.youtube.com/watch?v=%s", self->video_id);
        GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("embedUrl", embed_url);

        g_free (embed_url);
      });
      GTUBER_UTILS_JSON_ADD_NAMED_OBJECT ("user", {
        GTUBER_UTILS_JSON_ADD_KEY_VAL_BOOLEAN ("lockedSafetyMode", FALSE);
      });
    });
    GTUBER_UTILS_JSON_ADD_NAMED_OBJECT ("playbackContext", {
      GTUBER_UTILS_JSON_ADD_NAMED_OBJECT ("contentPlaybackContext", {
        GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("html5Preference", "HTML5_PREF_WANTS");
      });
    });
    GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("videoId", self->video_id);
    GTUBER_UTILS_JSON_ADD_KEY_VAL_BOOLEAN ("contentCheckOk", TRUE);
    GTUBER_UTILS_JSON_ADD_KEY_VAL_BOOLEAN ("racyCheckOk", TRUE);

    for (i = 0; clients[self->client].root_params[i]; i += 2) {
      GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING (
          clients[self->client].root_params[i],
          clients[self->client].root_params[i + 1]);
    }
  });

  g_strfreev (parts);

  return req_body;
}

static gchar *
obtain_auth_header_value (GtuberYoutube *self)
{
  GtuberWebsite *website = GTUBER_WEBSITE (self);
  SoupCookieJar *jar;
  gchar *auth_header = NULL;

  if ((jar = gtuber_website_get_cookies_jar (website))) {
    GUri *website_uri;
    GSList *cookies_list, *list;
    const gchar *sapisid = NULL;

    website_uri = gtuber_website_get_uri (website);
    cookies_list = soup_cookie_jar_get_cookie_list (jar, website_uri, TRUE);

    for (list = cookies_list; list != NULL; list = g_slist_next (list)) {
      SoupCookie *cookie = list->data;
      const gchar *cookie_name;

      cookie_name = soup_cookie_get_name (cookie);

      /* Value stored in either "SAPISID" or "__Secure-3PAPISID" */
      if (!strcmp (cookie_name, "SAPISID") || !strcmp (cookie_name, "__Secure-3PAPISID")) {
        sapisid = soup_cookie_get_value (cookie);
        break;
      }
    }

    if (sapisid) {
      GChecksum *checksum;
      GDateTime *date_time;
      gchar *sapisid_full;
      gint64 curr_time;

      date_time = g_date_time_new_now_utc ();
      curr_time = g_date_time_to_unix (date_time);
      g_date_time_unref (date_time);

      sapisid_full = g_strdup_printf ("%" G_GINT64_FORMAT " %s %s",
          curr_time, sapisid, GTUBER_YOUTUBE_X_ORIGIN);

      checksum = g_checksum_new (G_CHECKSUM_SHA1);
      g_checksum_update (checksum, (guchar *) sapisid_full, strlen (sapisid_full));
      g_free (sapisid_full);

      auth_header = g_strdup_printf ("SAPISIDHASH %" G_GINT64_FORMAT "_%s",
          curr_time, g_checksum_get_string (checksum));
      g_checksum_free (checksum);
    }

    g_slist_free_full (cookies_list, (GDestroyNotify) soup_cookie_free);
  }

  return auth_header;
}

static SoupMessage *
obtain_api_msg (GtuberYoutube *self)
{
  SoupMessage *msg;
  SoupMessageHeaders *headers;
  SoupCookieJar *jar;
  gchar *req_uri, *req_body;

  g_debug ("Try with client: %i (%s)",
      self->client, clients[self->client].name);

  req_uri = g_strdup_printf (
      "https://www.youtube.com/youtubei/v1/player?key=%s",
      clients[self->client].api_key);

  msg = soup_message_new ("POST", req_uri);
  g_free (req_uri);

  headers = soup_message_get_request_headers (msg);

  soup_message_headers_append (headers, "X-YouTube-Client-Name", clients[self->client].id);
  soup_message_headers_append (headers, "X-YouTube-Client-Version", clients[self->client].version);

  if ((jar = gtuber_website_get_cookies_jar (GTUBER_WEBSITE (self)))) {
    gchar *auth_str, *cookies_str;

    if ((auth_str = obtain_auth_header_value (self))) {
      soup_message_headers_replace (headers, "Authorization", auth_str);

      /* Must be the same origin as used to create "Authorization" header value */
      soup_message_headers_append (headers, "X-Origin", GTUBER_YOUTUBE_X_ORIGIN);
      soup_message_headers_append (headers, "X-Goog-AuthUser", "0");

      g_free (auth_str);
    }

    cookies_str = soup_cookie_jar_get_cookies (jar, soup_message_get_uri (msg), TRUE);
    g_debug ("Request cookies: %s", cookies_str);

    soup_message_headers_replace (headers, "Cookie", cookies_str);
    g_free (cookies_str);
  }

  req_body = obtain_player_req_body (self);
  g_debug ("Request body: %s", req_body);

  gtuber_utils_common_msg_take_request (msg, "application/json", req_body);

  return msg;
}

static void
gtuber_youtube_prepare (GtuberWebsite *website)
{
  GtuberYoutube *self = GTUBER_YOUTUBE (website);
  const gchar *const *langs;
  const gchar *env_str;
  guint i;

  env_str = g_getenv ("GTUBER_YOUTUBE_MOD_URI");
  self->use_mod_uri = (!env_str || g_str_has_prefix (env_str, "1"));

  if (G_LIKELY (self->video_id != NULL))
    self->step++;

  self->visitor_data = gtuber_youtube_cache_read ("visitor_data");
  if (!self->visitor_data)
    self->visitor_data = g_strdup ("");

  langs = g_get_language_names ();
  for (i = 0; langs[i]; i++) {
    if (strlen (langs[i]) == 5 && ((langs[i])[2] == '_')) {
      self->locale = g_strdup (langs[i]);
      break;
    }
  }
  if (!self->locale)
    self->locale = g_strdup ("en_US");

  g_debug ("Using locale: %s", self->locale);

  gtuber_youtube_update_user_agent (self);
}

static GtuberFlow
gtuber_youtube_create_request (GtuberWebsite *website,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error)
{
  GtuberYoutube *self = GTUBER_YOUTUBE (website);
  SoupMessageHeaders *headers;

  g_debug ("Create request step: %u", self->step);

  switch (self->step) {
    case YOUTUBE_GET_VIDEO_ID:
      *msg = soup_message_new_from_uri ("GET", gtuber_website_get_uri (website));
      break;
    case YOUTUBE_GET_API_DATA:
      *msg = obtain_api_msg (self);
      break;
    case YOUTUBE_GET_HLS:
      *msg = soup_message_new ("GET", self->hls_uri);
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  headers = soup_message_get_request_headers (*msg);

  soup_message_headers_replace (headers, "User-Agent", self->ua);
  soup_message_headers_replace (headers, "Origin", GTUBER_YOUTUBE_X_ORIGIN);

  soup_message_headers_append (headers, "X-Goog-Api-Format-Version", "2");
  soup_message_headers_append (headers, "X-Goog-Visitor-Id", self->visitor_data);

  return GTUBER_FLOW_OK;
}

static GtuberFlow
gtuber_youtube_parse_input_stream (GtuberWebsite *website,
    GInputStream *stream, GtuberMediaInfo *info, GError **error)
{
  GtuberYoutube *self = GTUBER_YOUTUBE (website);
  GtuberFlow flow = GTUBER_FLOW_OK;

  g_debug ("Parse step: %u", self->step);

  switch (self->step) {
    case YOUTUBE_GET_VIDEO_ID:
      self->video_id = obtain_video_id (stream, error);
      break;
    case YOUTUBE_GET_API_DATA:
      flow = parse_api_data (self, stream, info, error);
      break;
    case YOUTUBE_GET_HLS:
      gtuber_utils_youtube_parse_hls_input_stream (stream, info, error);
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  if (*error)
    return GTUBER_FLOW_ERROR;
  if (flow == GTUBER_FLOW_OK)
    self->step++;

  /* Check if next step should be skipped */
  if (self->step == YOUTUBE_GET_HLS && !self->hls_uri)
    self->step++;

  return (self->step >= YOUTUBE_EXTRACTION_FINISH)
      ? GTUBER_FLOW_OK
      : GTUBER_FLOW_RESTART;
}

static GtuberFlow
gtuber_youtube_set_user_req_headers (GtuberWebsite *website,
    SoupMessageHeaders *req_headers, GHashTable *user_headers, GError **error)
{
  GtuberYoutube *self = GTUBER_YOUTUBE (website);

  /* Update visitor ID header */
  soup_message_headers_replace (req_headers, "X-Goog-Visitor-Id", self->visitor_data);

  /* Remove "Authorization" specific headers */
  soup_message_headers_remove (req_headers, "X-Origin");
  soup_message_headers_remove (req_headers, "X-Goog-AuthUser");

  return GTUBER_WEBSITE_CLASS (parent_class)->set_user_req_headers (website,
      req_headers, user_headers, error);
}

static void
gtuber_youtube_class_init (GtuberYoutubeClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtuberWebsiteClass *website_class = (GtuberWebsiteClass *) klass;

  gobject_class->finalize = gtuber_youtube_finalize;

  website_class->prepare = gtuber_youtube_prepare;
  website_class->create_request = gtuber_youtube_create_request;
  website_class->parse_input_stream = gtuber_youtube_parse_input_stream;
  website_class->set_user_req_headers = gtuber_youtube_set_user_req_headers;
}

GtuberWebsite *
plugin_query (GUri *uri)
{
  gchar *id;
  gboolean matched, is_video = FALSE;

  matched = gtuber_utils_common_uri_matches_hosts (uri, NULL,
      "youtu.be", NULL);

  id = (matched)
      ? gtuber_utils_common_obtain_uri_id_from_paths (uri, NULL, "/", NULL)
      : gtuber_utils_common_obtain_uri_query_value (uri, "v");

  if (!id) {
    id = gtuber_utils_common_obtain_uri_id_from_paths (uri, NULL,
        "/v/", "/embed/", NULL);
  }

  if (!id) {
    gchar *suffix;

    suffix = gtuber_utils_common_obtain_uri_id_from_paths (uri, NULL, "/*/", NULL);
    is_video = (!g_ascii_strncasecmp (suffix, "live", 4));

    g_free (suffix);
  }

  if (id || is_video) {
    GtuberYoutube *youtube;

    youtube = gtuber_youtube_new ();
    youtube->video_id = id;

    g_debug ("Requested video: %s", youtube->video_id);

    return GTUBER_WEBSITE (youtube);
  }

  return NULL;
}
