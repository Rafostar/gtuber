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

#define GTUBER_YOUTUBE_CLI_VERSION "18.15.37"
#define GTUBER_YOUTUBE_ANDROID_MAJOR 11
#define GTUBER_YOUTUBE_ANDROID_SDK_MAJOR 30
#define GTUBER_YOUTUBE_X_ORIGIN "https://www.youtube.com"

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

  YoutubeStep step;
  guint try_count;
};

#define parent_class gtuber_youtube_parent_class
GTUBER_WEBSITE_PLUGIN_DEFINE (Youtube, youtube)

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

static void
_read_stream_info (JsonReader *reader, GtuberStream *stream)
{
  const gchar *uri_str, *yt_mime;
  gchar *mod_uri;

  uri_str = gtuber_utils_json_get_string (reader, "url", NULL);

  /* No point continuing without URI */
  if (!uri_str)
    return;

  mod_uri = gtuber_utils_common_obtain_uri_with_query_as_path (uri_str);
  gtuber_stream_set_uri (stream, mod_uri);
  g_free (mod_uri);

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
  _read_stream_info (reader, stream);

  gtuber_media_info_add_stream (info, stream);
}

static void
_read_adaptive_stream_cb (JsonReader *reader, GtuberMediaInfo *info, gpointer user_data)
{
  GtuberAdaptiveStream *astream;
  const gchar *stream_type;

  stream_type = gtuber_utils_json_get_string (reader, "type", NULL);

  if (!g_strcmp0 (stream_type, "FORMAT_STREAM_TYPE_OTF")) {
    /* FIXME: OTF requires fetching init at "/sq/0" first
     * then remaining fragments by number instead of range */
    return;
  }

  astream = gtuber_adaptive_stream_new ();
  _read_stream_info (reader, GTUBER_STREAM (astream));

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
  const gchar *status, *visitor_data;
  GtuberFlow flow = GTUBER_FLOW_OK;

  parser = json_parser_new ();
  json_parser_load_from_stream (parser, stream, NULL, error);
  if (*error)
    goto finish;

  gtuber_utils_json_parser_debug (parser);
  reader = json_reader_new (json_parser_get_root (parser));

  /* Check if video is playable */
  status = gtuber_utils_json_get_string (reader, "playabilityStatus", "status", NULL);
  if (g_strcmp0 (status, "OK")) {
    if (self->try_count < 2) {
      flow = GTUBER_FLOW_RESTART;
      g_debug ("Video is not playable, trying again...");
    } else {
      const gchar *reason;

      reason = gtuber_utils_json_get_string (reader, "playabilityStatus", "reason", NULL);

      g_set_error (error, GTUBER_WEBSITE_ERROR,
          GTUBER_WEBSITE_ERROR_OTHER, "%s",
          (reason != NULL) ? reason : "Video is not playable");
    }
    goto finish;
  }

  if (gtuber_utils_json_go_to (reader, "videoDetails", NULL)) {
    const gchar *id, *title, *desc, *duration;

    id = gtuber_utils_json_get_string (reader, "videoId", NULL);
    gtuber_media_info_set_id (info, id);

    title = gtuber_utils_json_get_string (reader, "title", NULL);
    gtuber_media_info_set_title (info, title);

    desc = gtuber_utils_json_get_string (reader, "shortDescription", NULL);
    gtuber_media_info_set_description (info, desc);
    gtuber_utils_youtube_insert_chapters_from_description (info, desc);

    duration = gtuber_utils_json_get_string (reader, "lengthSeconds", NULL);
    if (duration)
      gtuber_media_info_set_duration (info, g_ascii_strtoull (duration, NULL, 10));

    gtuber_utils_json_go_back (reader, 1);
  }

  if (gtuber_utils_json_go_to (reader, "streamingData", NULL)) {
    self->hls_uri = g_strdup (gtuber_utils_json_get_string (reader, "hlsManifestUrl", NULL));

    if (!self->hls_uri) {
      if (gtuber_utils_json_go_to (reader, "formats", NULL)) {
        gtuber_utils_json_array_foreach (reader, info,
            (GtuberFunc) _read_stream_cb, NULL);
        gtuber_utils_json_go_back (reader, 1);
      }
      if (gtuber_utils_json_go_to (reader, "adaptiveFormats", NULL)) {
        gtuber_utils_json_array_foreach (reader, info,
            (GtuberFunc) _read_adaptive_stream_cb, NULL);
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
  const gchar *cliScreen;

  /* Get EMBED video info on first try.
   * If it fails, try default one on next try */
  cliScreen = (self->try_count == 1)
      ? "EMBED"
      : NULL;

  parts = g_strsplit (self->locale, "_", 0);

  GTUBER_UTILS_JSON_BUILD_OBJECT (&req_body, {
    GTUBER_UTILS_JSON_ADD_NAMED_OBJECT ("context", {
      GTUBER_UTILS_JSON_ADD_NAMED_OBJECT ("client", {
        GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("clientName", "ANDROID");
        GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("clientVersion", GTUBER_YOUTUBE_CLI_VERSION);
        GTUBER_UTILS_JSON_ADD_KEY_VAL_INT ("androidSdkVersion", GTUBER_YOUTUBE_ANDROID_SDK_MAJOR);
        GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("userAgent", self->ua);
        GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("clientScreen", cliScreen);
        GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("hl", parts[0]);
        GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("gl", parts[1]);
        GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("visitorData", self->visitor_data);
        GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("timeZone", "UTC"); // the same time as in "SAPISID"
        GTUBER_UTILS_JSON_ADD_KEY_VAL_INT ("utcOffsetMinutes", 0);
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
    GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("video_id", self->video_id);
    GTUBER_UTILS_JSON_ADD_KEY_VAL_BOOLEAN ("contentCheckOk", TRUE);
    GTUBER_UTILS_JSON_ADD_KEY_VAL_BOOLEAN ("racyCheckOk", TRUE);
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
  gchar *req_body;

  self->try_count++;
  g_debug ("Try number: %i", self->try_count);

  msg = soup_message_new ("POST",
      "https://www.youtube.com/youtubei/v1/player?"
      "key=AIzaSyA8eiZmM1FaDVjRy-df2KTyQ_vz_yYM39w");
  headers = soup_message_get_request_headers (msg);

  soup_message_headers_append (headers, "X-YouTube-Client-Name", "3"); // 3 = ANDROID
  soup_message_headers_append (headers, "X-YouTube-Client-Version", GTUBER_YOUTUBE_CLI_VERSION);

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
  guint i;

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

  self->ua = g_strdup_printf ("com.google.android.youtube/%s(Linux; U; Android %i; %s) gzip",
      GTUBER_YOUTUBE_CLI_VERSION, GTUBER_YOUTUBE_ANDROID_MAJOR, self->locale);
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
