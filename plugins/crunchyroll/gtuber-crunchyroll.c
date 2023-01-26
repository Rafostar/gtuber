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
#include "utils/xml/gtuber-utils-xml.h"

#define CRUNCHYROLL_DEFAULT_HOST  "crunchyroll.com"
#define CRUNCHYROLL_DEFAULT_WWW   "www." CRUNCHYROLL_DEFAULT_HOST
#define CRUNCHYROLL_DEFAULT_URI   "https://" CRUNCHYROLL_DEFAULT_WWW
#define CRUNCHYROLL_DEFAULT_LANG  "en-US"
#define CRUNCHYROLL_DEFAULT_AUDIO "ja-JP"

GTUBER_WEBSITE_PLUGIN_EXPORT_HOSTS (
  CRUNCHYROLL_DEFAULT_HOST,
  NULL
)
GTUBER_WEBSITE_PLUGIN_DECLARE (Crunchyroll, crunchyroll, CRUNCHYROLL)

/* XXX: The order is important in some cases, as for 2 chars
 * code the first one matching is used (e.g. "es" -> "es-419") */
static const gchar *cr_langs[] = {
  CRUNCHYROLL_DEFAULT_LANG,
  "es-419",
  "es-ES",
  "fr-FR",
  "pt-BR",
  "pt-PT",
  "ar-SA",
  "it-IT",
  "de-DE",
  "ru-RU",
  NULL
};

typedef enum
{
  CRUNCHYROLL_GET_AUTH_TOKEN = 1,
  CRUNCHYROLL_GET_ACCESS_TOKEN,
  CRUNCHYROLL_GET_POLICY_RESPONSE,
  CRUNCHYROLL_GET_MEDIA_INFO,
  CRUNCHYROLL_GET_STREAMS_DATA,
  CRUNCHYROLL_GET_HLS_URI,
  CRUNCHYROLL_EXTRACTION_FINISH,
} CrunchyrollStep;

struct _GtuberCrunchyroll
{
  GtuberWebsite parent;

  gchar *video_id;
  const gchar *language;
  gchar *etp_rt;

  CrunchyrollStep step;

  gchar *auth_token;
  gchar *token_type;
  gchar *access_token;
  gchar *policy_response;
  gchar *media_guid;
  gchar *hls_uri;
};

#define parent_class gtuber_crunchyroll_parent_class
GTUBER_WEBSITE_PLUGIN_DEFINE (Crunchyroll, crunchyroll)

static void
gtuber_crunchyroll_init (GtuberCrunchyroll *self)
{
  self->step = CRUNCHYROLL_GET_AUTH_TOKEN;
}

static void
gtuber_crunchyroll_finalize (GObject *object)
{
  GtuberCrunchyroll *self = GTUBER_CRUNCHYROLL (object);

  g_debug ("Crunchyroll finalize");

  g_free (self->video_id);
  g_free (self->etp_rt);

  g_free (self->auth_token);
  g_free (self->token_type);
  g_free (self->access_token);
  g_free (self->policy_response);
  g_free (self->media_guid);
  g_free (self->hls_uri);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static SoupMessage *
_make_cms_request (GtuberCrunchyroll *self, const gchar *subpath, GError **error)
{
  SoupMessage *msg = NULL;
  JsonReader *reader;

  if (!(reader = gtuber_utils_json_read_data (self->policy_response, error)))
    return NULL;

  if (gtuber_utils_json_go_to (reader, "cms_web", NULL)) {
    GUri *guri;
    gchar *path, *query;

    path = g_build_path ("/", "/cms/v2",
        gtuber_utils_json_get_string (reader, "bucket", NULL),
        subpath, NULL);

    query = soup_form_encode (
        "locale", self->language,
        "Signature", gtuber_utils_json_get_string (reader, "signature", NULL),
        "Policy", gtuber_utils_json_get_string (reader, "policy", NULL),
        "Key-Pair-Id", gtuber_utils_json_get_string (reader, "key_pair_id", NULL),
        NULL);

    guri = g_uri_build (G_URI_FLAGS_ENCODED, "https", NULL,
        CRUNCHYROLL_DEFAULT_WWW, -1, path, query, NULL);

    g_free (path);
    g_free (query);

    msg = soup_message_new_from_uri ("GET", guri);
    g_uri_unref (guri);
  }

  g_object_unref (reader);

  return msg;
}

static gboolean
_enter_lang_fuzzy (JsonReader *reader, const gchar *req_lang)
{
  gchar **video_langs;
  guint i;
  gboolean success = FALSE;

  video_langs = json_reader_list_members (reader);
  if (!video_langs)
    return FALSE;

  for (i = 0; video_langs[i]; i++) {
    if (req_lang[0] == video_langs[i][0]
        && req_lang[1] == video_langs[i][1]) {
      if ((success = gtuber_utils_json_go_to (reader, video_langs[i], NULL))) {
        g_debug ("Using fuzzy language matching: %s => %s",
            req_lang, video_langs[i]);
        break;
      }
    }
  }
  g_strfreev (video_langs);

  return success;
}

static SoupMessage *
obtain_access_token_msg (GtuberCrunchyroll *self)
{
  SoupMessage *msg;
  SoupMessageHeaders *headers;
  GUri *guri;
  gchar *req_uri, *auth_str;

  req_uri = g_strdup_printf ("%s/auth/v1/token", CRUNCHYROLL_DEFAULT_URI);
  guri = g_uri_parse (req_uri, G_URI_FLAGS_ENCODED, NULL);
  msg = soup_message_new_from_uri ("POST", guri);

  headers = soup_message_get_request_headers (msg);
  auth_str = g_strdup_printf ("Basic %s", self->auth_token);
  soup_message_headers_replace (headers, "Authorization", auth_str);

  if (self->etp_rt) {
    SoupCookieJar *jar;
    gchar *cookies_str;

    jar = gtuber_website_get_cookies_jar (GTUBER_WEBSITE (self));

    cookies_str = soup_cookie_jar_get_cookies (jar, guri, TRUE);
    g_debug ("Request cookies: %s", cookies_str);

    soup_message_headers_replace (headers, "Cookie", cookies_str);
    g_free (cookies_str);
  }

  gtuber_utils_common_msg_take_request (msg,
      "application/x-www-form-urlencoded",
      g_strdup_printf ("grant_type=%s",
          (self->etp_rt) ? "etp_rt_cookie" : "client_id"));

  g_uri_unref (guri);
  g_free (req_uri);
  g_free (auth_str);

  return msg;
}

static SoupMessage *
obtain_policy_response_msg (GtuberCrunchyroll *self)
{
  SoupMessage *msg;
  SoupMessageHeaders *headers;
  gchar *req_uri, *auth_str;

  req_uri = g_strdup_printf ("%s/index/v2", CRUNCHYROLL_DEFAULT_URI);
  msg = soup_message_new ("GET", req_uri);

  headers = soup_message_get_request_headers (msg);
  auth_str = g_strdup_printf ("%s %s", self->token_type, self->access_token);
  soup_message_headers_replace (headers, "Authorization", auth_str);

  g_free (req_uri);
  g_free (auth_str);

  return msg;
}

static SoupMessage *
obtain_media_info_msg (GtuberCrunchyroll *self, GError **error)
{
  SoupMessage *msg;
  gchar *subpath;

  subpath = g_build_path ("/", "objects", self->video_id, NULL);
  msg = _make_cms_request (self, subpath, error);

  g_free (subpath);

  return msg;
}

static SoupMessage *
obtain_streams_data_msg (GtuberCrunchyroll *self, GError **error)
{
  SoupMessage *msg;
  gchar *subpath;

  subpath = g_build_path ("/", "videos", self->media_guid, "streams", NULL);
  msg = _make_cms_request (self, subpath, error);

  g_free (subpath);

  return msg;
}

static void
read_auth_token (GtuberCrunchyroll *self, GInputStream *stream, GError **error)
{
  JsonReader *reader;
  xmlDoc *doc;
  gchar *data, *json_str;

  if (!(data = gtuber_utils_common_input_stream_to_data (stream, error)))
    return;

  doc = gtuber_utils_xml_load_html_from_data (data, error);
  g_free (data);

  if (!doc)
    return;

  json_str = gtuber_utils_xml_obtain_json_in_node (doc, "__APP_CONFIG__");
  xmlFreeDoc (doc);

  if (!json_str) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Could not extract Crunchyroll JSON data");
    return;
  }

  if ((reader = gtuber_utils_json_read_data (json_str, error))) {
    const gchar *cli_id;

    cli_id = (self->etp_rt)
        ? gtuber_utils_json_get_string (reader, "cxApiParams", "accountAuthClientId", NULL)
        : gtuber_utils_json_get_string (reader, "cxApiParams", "anonClientId", NULL);

    if (cli_id) {
      gchar *tmp_token;

      tmp_token = g_strdup_printf ("%s:", cli_id);
      self->auth_token = g_base64_encode ((guchar *) tmp_token, strlen (tmp_token));
      g_free (tmp_token);

      g_debug ("Auth token: %s", self->auth_token);
    }

    if (!self->auth_token) {
      g_set_error (error, GTUBER_WEBSITE_ERROR,
          GTUBER_WEBSITE_ERROR_PARSE_FAILED,
          "Missing data for querying Crunchyroll API");
    }

    g_object_unref (reader);
  }

  g_free (json_str);
}

static void
read_access_token (GtuberCrunchyroll *self, GInputStream *stream, GError **error)
{
  JsonReader *reader;

  g_debug ("Reading access token");

  if (!(reader = gtuber_utils_json_read_stream (stream, error)))
    return;

  self->access_token = g_strdup (gtuber_utils_json_get_string (reader, "access_token", NULL));
  self->token_type = g_strdup (gtuber_utils_json_get_string (reader, "token_type", NULL));

  g_object_unref (reader);

  if (!self->access_token || !self->token_type) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Could not obtain access token data");
  }
}

static void
read_policy_response (GtuberCrunchyroll *self, GInputStream *stream, GError **error)
{
  JsonReader *reader;
  gboolean valid = FALSE;

  g_debug ("Reading policy response");

  if (!(reader = gtuber_utils_json_read_stream (stream, error)))
    return;

  g_debug ("Policy response validation");

  if (!gtuber_utils_json_get_boolean (reader, "service_available", NULL)
      || !gtuber_utils_json_go_to (reader, "cms_web", NULL))
    goto finish;

  /* Make sure response has all data we need */
  valid = (gtuber_utils_json_get_string (reader, "bucket", NULL)
      && gtuber_utils_json_get_string (reader, "policy", NULL)
      && gtuber_utils_json_get_string (reader, "signature", NULL)
      && gtuber_utils_json_get_string (reader, "key_pair_id", NULL));

  if (valid) {
    GDateTime *date_time;
    const gchar *exp_date;
    gint64 epoch;

    self->policy_response = gtuber_utils_json_reader_to_string (reader);

    exp_date = gtuber_utils_json_get_string (reader, "expires", NULL);
    date_time = g_date_time_new_from_iso8601 (exp_date, NULL);
    epoch = g_date_time_to_unix (date_time);

    g_date_time_unref (date_time);

    /* Cache received policy_response and etp_rt used to create it */
    gtuber_crunchyroll_cache_write_epoch ("policy_response",
        self->policy_response, epoch);
    gtuber_crunchyroll_cache_write_epoch ("etp_rt",
        self->etp_rt, epoch);
  }

finish:
  if (!valid && *error == NULL) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Could not validate policy response data");
  }

  g_object_unref (reader);
}

static void
read_media_info (GtuberCrunchyroll *self, GInputStream *stream,
    GtuberMediaInfo *info, GError **error)
{
  JsonReader *reader;
  const gchar *audio_locale, *series_title, *ep_title;
  gint i, versions_count;

  g_debug ("Reading media info");

  if (!(reader = gtuber_utils_json_read_stream (stream, error)))
    return;

  if (!gtuber_utils_json_go_to (reader, "items",
      GTUBER_UTILS_JSON_ARRAY_INDEX (0), NULL)) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Missing video info in JSON response data");
    goto finish;
  }

  if (gtuber_utils_json_get_boolean (reader, "episode_metadata", "is_premium_only", NULL)
      && !gtuber_utils_json_get_string (reader, "playback", NULL)) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Requested video is for premium users only");
    goto finish;
  }

  versions_count = gtuber_utils_json_count_elements (reader, "episode_metadata", "versions", NULL);
  if (versions_count < 1) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "No playable versions of requested video");
    goto finish;
  }

  audio_locale = gtuber_utils_json_get_string (reader, "episode_metadata", "audio_locale", NULL);
  if (!audio_locale)
    audio_locale = CRUNCHYROLL_DEFAULT_AUDIO;

  if (gtuber_utils_json_go_to (reader, "episode_metadata", "versions", NULL)) {
    for (i = 0; i < versions_count; i++) {
      if (gtuber_utils_json_go_to (reader, GTUBER_UTILS_JSON_ARRAY_INDEX (i), NULL)) {
        const gchar *audio_ver;

        audio_ver = gtuber_utils_json_get_string (reader, "audio_locale", NULL);
        if (!g_strcmp0 (audio_locale, audio_ver))
          self->media_guid = g_strdup (gtuber_utils_json_get_string (reader, "media_guid", NULL));

        gtuber_utils_json_go_back (reader, 1);
      }
      if (self->media_guid)
        break;
    }
    gtuber_utils_json_go_back (reader, 2);
  }

  if (!self->media_guid) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Could not extract stream media ID");
    goto finish;
  }
  g_debug ("Found stream media ID: %s", self->media_guid);

  gtuber_media_info_set_id (info, gtuber_utils_json_get_string (reader, "id", NULL));

  series_title = gtuber_utils_json_get_string (reader, "episode_metadata", "series_title", NULL);
  ep_title = gtuber_utils_json_get_string (reader, "title", NULL);

  if (series_title && ep_title) {
    gchar *title;
    gint s_number, ep_number;

    s_number = gtuber_utils_json_get_int (reader, "episode_metadata", "season_number", NULL);
    ep_number = gtuber_utils_json_get_int (reader, "episode_metadata", "episode_number", NULL);

    if (s_number > 0 && ep_number > 0) {
      title = g_strdup_printf ("%s S%02dE%02d - %s",
          series_title, s_number, ep_number, ep_title);
    } else {
      title = g_strdup_printf ("%s - %s", series_title, ep_title);
    }

    gtuber_media_info_set_title (info, title);
    g_free (title);
  } else {
    if (ep_title)
      gtuber_media_info_set_title (info, ep_title);
    else if (series_title)
      gtuber_media_info_set_title (info, series_title);
  }
  g_debug ("Media title: %s", gtuber_media_info_get_title (info));

  gtuber_media_info_set_description (info,
      gtuber_utils_json_get_string (reader, "description", NULL));
  gtuber_media_info_set_duration (info,
      gtuber_utils_json_get_int (reader, "episode_metadata", "duration_ms", NULL) / 1000);

  gtuber_utils_json_go_back (reader, 2);

finish:
  g_object_unref (reader);
}

static void
read_streaming_uris (GtuberCrunchyroll *self, GInputStream *stream,
    GtuberMediaInfo *info, GError **error)
{
  JsonReader *reader;
  const gchar *audio_locale, *req_lang;
  gboolean is_dubbed;

  g_debug ("Reading streaming URIs");

  if (!(reader = gtuber_utils_json_read_stream (stream, error)))
    return;

  audio_locale = gtuber_utils_json_get_string (reader, "audio_locale", NULL);

  if (!gtuber_utils_json_go_to (reader, "streams", "adaptive_hls", NULL)) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "No adaptive HLS URIs");
    goto finish;
  }

  is_dubbed = (audio_locale && strcmp (audio_locale, CRUNCHYROLL_DEFAULT_AUDIO) != 0);
  g_debug ("Dubbing detected: %s (%s)",
      is_dubbed ? "yes" : "no", audio_locale);

  /* Empty string has no hardsubs */
  req_lang = (!is_dubbed)
      ? self->language
      : "";

  /* Try to use the same language as in requested URI,
   * otherwise fallback to default language */
  if (!gtuber_utils_json_go_to (reader, req_lang, NULL)
      && !_enter_lang_fuzzy (reader, self->language)
      && !gtuber_utils_json_go_to (reader, CRUNCHYROLL_DEFAULT_LANG, NULL)) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Could not find suitable video language");
    goto finish;
  }

  self->hls_uri = g_strdup (gtuber_utils_json_get_string (reader, "url", NULL));
  if (!self->hls_uri) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "Could not find HLS manifest URI");
    goto finish;
  }

  /* FIXME: Extract DASH URI */

finish:
  g_object_unref (reader);
}

static void
gtuber_crunchyroll_prepare (GtuberWebsite *website)
{
  GtuberCrunchyroll *self = GTUBER_CRUNCHYROLL (website);
  SoupCookieJar *jar;
  gchar *ext_lang, *cached_etp_rt = NULL;
  gboolean etp_rt_changed = FALSE;

  if ((jar = gtuber_website_get_cookies_jar (website))) {
    GUri *auth_uri;
    GSList *cookies_list, *list;
    gchar *req_uri;

    /* We got cookies jar, so compare it with our last value */
    cached_etp_rt = gtuber_crunchyroll_cache_read ("etp_rt");
    g_debug ("Cached etp_rt: %s", cached_etp_rt);

    /* We only need to use cookies when requesting for auth token */
    req_uri = g_strdup_printf ("%s/auth/v1/token", CRUNCHYROLL_DEFAULT_URI);
    auth_uri = g_uri_parse (req_uri, G_URI_FLAGS_ENCODED, NULL);
    g_free (req_uri);

    cookies_list = soup_cookie_jar_get_cookie_list (jar, auth_uri, TRUE);
    g_uri_unref (auth_uri);

    for (list = cookies_list; list != NULL; list = g_slist_next (list)) {
      SoupCookie *cookie = list->data;
      const gchar *cookie_name;

      cookie_name = soup_cookie_get_name (cookie);

      if (!strcmp (cookie_name, "etp_rt")) {
        self->etp_rt = g_strdup (soup_cookie_get_value (cookie));
        break;
      }
    }
    g_debug ("Browser etp_rt: %s", self->etp_rt);

    g_slist_free_full (cookies_list, (GDestroyNotify) soup_cookie_free);
  }

  etp_rt_changed = (g_strcmp0 (self->etp_rt, cached_etp_rt) != 0);
  g_free (cached_etp_rt);

  g_debug ("Token changed: %s", etp_rt_changed ? "yes" : "no");

  if (!etp_rt_changed) {
    /* If we restored cached policy, skip steps to get it */
    if ((self->policy_response = gtuber_crunchyroll_cache_read ("policy_response")))
      self->step = CRUNCHYROLL_GET_POLICY_RESPONSE + 1;
  }

  ext_lang = gtuber_utils_common_obtain_uri_id_from_paths (
      gtuber_website_get_uri (website), NULL, "/", NULL);

  if (ext_lang && strlen (ext_lang) <= 5) {
    guint i;

    for (i = 0; cr_langs[i]; i++) {
      const gchar *cr_lang = cr_langs[i];

      if (!g_ascii_strncasecmp (ext_lang, cr_lang, 5)
          || g_str_has_prefix (cr_lang, ext_lang)) {
        self->language = cr_lang;
        break;
      }
    }
  }

  g_free (ext_lang);

  if (!self->language)
    self->language = CRUNCHYROLL_DEFAULT_LANG;

  g_debug ("Using language: %s", self->language);
}

static GtuberFlow
gtuber_crunchyroll_create_request (GtuberWebsite *website,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error)
{
  GtuberCrunchyroll *self = GTUBER_CRUNCHYROLL (website);

  g_debug ("Create request step: %u", self->step);

  switch (self->step) {
    case CRUNCHYROLL_GET_AUTH_TOKEN:
      *msg = soup_message_new_from_uri ("GET", gtuber_website_get_uri (website));
      break;
    case CRUNCHYROLL_GET_ACCESS_TOKEN:
      *msg = obtain_access_token_msg (self);
      break;
    case CRUNCHYROLL_GET_POLICY_RESPONSE:
      *msg = obtain_policy_response_msg (self);
      break;
    case CRUNCHYROLL_GET_MEDIA_INFO:
      *msg = obtain_media_info_msg (self, error);
      break;
    case CRUNCHYROLL_GET_STREAMS_DATA:
      *msg = obtain_streams_data_msg (self, error);
      break;
    case CRUNCHYROLL_GET_HLS_URI:
      *msg = soup_message_new ("GET", self->hls_uri);
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  if (*msg) {
    SoupMessageHeaders *headers;
    gchar *referer;

    headers = soup_message_get_request_headers (*msg);
    referer = g_uri_to_string_partial (gtuber_website_get_uri (website),
        G_URI_HIDE_QUERY | G_URI_HIDE_FRAGMENT);

    soup_message_headers_replace (headers, "Origin", CRUNCHYROLL_DEFAULT_URI);
    soup_message_headers_replace (headers, "Referer", referer);
    soup_message_headers_replace (headers, "Accept-Language", "*");

    g_free (referer);
  }

  return (*error)
      ? GTUBER_FLOW_ERROR
      : GTUBER_FLOW_OK;
}

static GtuberFlow
gtuber_crunchyroll_parse_input_stream (GtuberWebsite *website,
    GInputStream *stream, GtuberMediaInfo *info, GError **error)
{
  GtuberCrunchyroll *self = GTUBER_CRUNCHYROLL (website);

  g_debug ("Parse step: %u", self->step);

  switch (self->step) {
    case CRUNCHYROLL_GET_AUTH_TOKEN:
      read_auth_token (self, stream, error);
      break;
    case CRUNCHYROLL_GET_ACCESS_TOKEN:
      read_access_token (self, stream, error);
      break;
    case CRUNCHYROLL_GET_POLICY_RESPONSE:
      read_policy_response (self, stream, error);
      break;
    case CRUNCHYROLL_GET_MEDIA_INFO:
      read_media_info (self, stream, info, error);
      break;
    case CRUNCHYROLL_GET_STREAMS_DATA:
      read_streaming_uris (self, stream, info, error);
      break;
    /* FIXME: DASH support */
    case CRUNCHYROLL_GET_HLS_URI:
      gtuber_utils_common_parse_hls_input_stream (stream, info, error);
      break;
    default:
      g_assert_not_reached ();
      break;
  }

  if (*error)
    return GTUBER_FLOW_ERROR;

  /* Proceed to next step */
  self->step++;

  return (self->step >= CRUNCHYROLL_EXTRACTION_FINISH)
      ? GTUBER_FLOW_OK
      : GTUBER_FLOW_RESTART;
}

static void
gtuber_crunchyroll_class_init (GtuberCrunchyrollClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtuberWebsiteClass *website_class = (GtuberWebsiteClass *) klass;

  gobject_class->finalize = gtuber_crunchyroll_finalize;

  website_class->prepare = gtuber_crunchyroll_prepare;
  website_class->create_request = gtuber_crunchyroll_create_request;
  website_class->parse_input_stream = gtuber_crunchyroll_parse_input_stream;
}

GtuberWebsite *
plugin_query (GUri *uri)
{
  GtuberCrunchyroll *cr = NULL;
  gchar *id;

  id = gtuber_utils_common_obtain_uri_id_from_paths (uri, NULL,
      "/*/watch/", "/watch/", NULL);

  if (id) {
    cr = gtuber_crunchyroll_new ();
    cr->video_id = id;

    g_debug ("Requested video: %s", cr->video_id);
  }

  return GTUBER_WEBSITE (cr);
}
