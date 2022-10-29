/*
 * Copyright (C) 2022 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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
#include "utils/xml/gtuber-utils-xml.h"

#include "gtuber-niconico-heartbeat.h"

GTUBER_WEBSITE_PLUGIN_EXPORT_HOSTS (
  "nicovideo.jp",
  NULL
)
GTUBER_WEBSITE_PLUGIN_DECLARE (Niconico, niconico, NICONICO)

struct _GtuberNiconico
{
  GtuberWebsite parent;

  gchar *video_id;
  gchar *api_uri;
  gchar *api_data;
  gchar *hls_uri;

  gboolean can_stream;
};

#define parent_class gtuber_niconico_parent_class
GTUBER_WEBSITE_PLUGIN_DEFINE (Niconico, niconico)

static void
gtuber_niconico_init (GtuberNiconico *self)
{
}

static void
gtuber_niconico_finalize (GObject *object)
{
  GtuberNiconico *self = GTUBER_NICONICO (object);

  g_debug ("Niconico finalize");

  g_free (self->video_id);
  g_free (self->api_uri);
  g_free (self->api_data);
  g_free (self->hls_uri);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static SoupMessage *
_obtain_nvapi_post_msg (GtuberNiconico *self, JsonReader *reader,
    GError **error)
{
  SoupMessage *msg;
  SoupMessageHeaders *headers;
  const gchar *tracking_id;
  gchar *nvapi_uri;

  if (!(tracking_id = gtuber_utils_json_get_string (reader,
      "media", "delivery", "trackingId", NULL))) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_REQUEST_CREATE_FAILED,
        "API data is missing \"trackingId\"");
    return NULL;
  }

  nvapi_uri = g_strdup_printf ("https://nvapi.nicovideo.jp/v1/2ab0cbaa/watch?t=%s",
      tracking_id);

  g_debug ("NVAPI URI: %s", nvapi_uri);
  msg = soup_message_new ("GET", nvapi_uri);
  g_free (nvapi_uri);

  /* These headers are for NVAPI request only */
  headers = soup_message_get_request_headers (msg);
  soup_message_headers_append (headers, "X-Frontend-Id", "6");
  soup_message_headers_append (headers, "X-Frontend-Version", "0");
  soup_message_headers_append (headers, "X-Request-With", "https://www.nicovideo.jp");

  return msg;
}

static SoupMessage *
_parse_and_obtain_session_post_msg (GtuberNiconico *self, JsonReader *reader,
    GtuberMediaInfo *info, GError **error)
{
  gchar *req_body = NULL;

  gtuber_utils_json_go_to (reader, "video", NULL);

  gtuber_media_info_set_id (info,
      gtuber_utils_json_get_string (reader, "id", NULL));
  gtuber_media_info_set_title (info,
      gtuber_utils_json_get_string (reader, "title", NULL));
  gtuber_media_info_set_description (info,
      gtuber_utils_json_get_string (reader, "description", NULL));
  gtuber_media_info_set_duration (info,
      gtuber_utils_json_get_int (reader, "duration", NULL));

  gtuber_utils_json_go_back (reader, 1);

  if (!gtuber_utils_json_go_to (reader, "media", "delivery", "movie", "session", NULL)) {
    g_debug ("Missing API session data");
    goto fail;
  }

  GTUBER_UTILS_JSON_BUILD_OBJECT (&req_body, {
    GTUBER_UTILS_JSON_ADD_NAMED_OBJECT ("session", {
      const gchar *proto_name;

      GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("recipe_id",
          gtuber_utils_json_get_string (reader, "recipeId", NULL));
      GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("content_id",
          gtuber_utils_json_get_string (reader, "contentId", NULL));

      GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("content_type", "movie");
      GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("content_uri", "");
      GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("timing_constraint", "unlimited");

      GTUBER_UTILS_JSON_ADD_NAMED_ARRAY ("content_src_id_sets", {
        GTUBER_UTILS_JSON_ADD_OBJECT ({
          GTUBER_UTILS_JSON_ADD_NAMED_ARRAY ("content_src_ids", {
            gint i, vid_count, aud_count;

            vid_count = gtuber_utils_json_count_elements (reader, "videos", NULL);
            aud_count = gtuber_utils_json_count_elements (reader, "audios", NULL);

            if (vid_count < 1 || aud_count < 1) {
              g_debug ("Missing API \"%s\" array", (vid_count < 1) ? "videos" : "audios");
              break;
            }

            /* Combine each video format with each audio format */
            for (i = 0; i < vid_count; i++) {
              const gchar *vid_id;
              gint j;

              vid_id = gtuber_utils_json_get_string (reader, "videos",
                  GTUBER_UTILS_JSON_ARRAY_INDEX (i), NULL);

              for (j = 0; j < aud_count; j++) {
                const gchar *aud_id;

                aud_id = gtuber_utils_json_get_string (reader, "audios",
                    GTUBER_UTILS_JSON_ARRAY_INDEX (j), NULL);

                GTUBER_UTILS_JSON_ADD_OBJECT ({
                  GTUBER_UTILS_JSON_ADD_NAMED_OBJECT ("src_id_to_mux", {
                    GTUBER_UTILS_JSON_ADD_NAMED_ARRAY ("video_src_ids", {
                      GTUBER_UTILS_JSON_ADD_VAL_STRING (vid_id);
                    });
                    GTUBER_UTILS_JSON_ADD_NAMED_ARRAY ("audio_src_ids", {
                      GTUBER_UTILS_JSON_ADD_VAL_STRING (aud_id);
                    });
                  });
                });
              }
            }
          });
        });
      });
      GTUBER_UTILS_JSON_ADD_NAMED_OBJECT ("keep_method", {
        GTUBER_UTILS_JSON_ADD_NAMED_OBJECT ("heartbeat", {
          GTUBER_UTILS_JSON_ADD_KEY_VAL_INT ("lifetime",
              gtuber_utils_json_get_int (reader, "heartbeatLifetime", NULL));
        });
      });
      GTUBER_UTILS_JSON_ADD_NAMED_OBJECT ("protocol", {
        if (!gtuber_utils_json_go_to (reader, "protocols",
            GTUBER_UTILS_JSON_ARRAY_INDEX (0), NULL)) {
          g_debug ("Missing API protocols array");
          break;
        }

        if (!(proto_name = gtuber_utils_json_get_string (reader, NULL))) {
          g_debug ("Missing API protocol name");
          break;
        }
        GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("name", proto_name);

        gtuber_utils_json_go_back (reader, 2);

        GTUBER_UTILS_JSON_ADD_NAMED_OBJECT ("parameters", {
          GTUBER_UTILS_JSON_ADD_NAMED_OBJECT ("http_parameters", {
            GTUBER_UTILS_JSON_ADD_NAMED_OBJECT ("parameters", {
              GTUBER_UTILS_JSON_ADD_NAMED_OBJECT ("hls_parameters", {
                if (!gtuber_utils_json_go_to (reader, "urls",
                    GTUBER_UTILS_JSON_ARRAY_INDEX (0), NULL)) {
                  g_debug ("Missing API urls array");
                  break;
                }

                if (!(self->api_uri = g_strdup (
                    gtuber_utils_json_get_string (reader, "url", NULL)))) {
                  g_debug ("Missing API URI");
                  break;
                }

                GTUBER_UTILS_JSON_ADD_KEY_VAL_YES_NO ("use_well_known_port",
                    gtuber_utils_json_get_boolean (reader, "isWellKnownPort", NULL));
                GTUBER_UTILS_JSON_ADD_KEY_VAL_YES_NO ("use_ssl",
                    gtuber_utils_json_get_boolean (reader, "isSsl", NULL));

                GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("transfer_preset", "");
                GTUBER_UTILS_JSON_ADD_KEY_VAL_INT ("segment_duration", 6000);

                gtuber_utils_json_go_back (reader, 2);
              });
            });
          });
        });
      });
      GTUBER_UTILS_JSON_ADD_NAMED_OBJECT ("session_operation_auth", {
        GTUBER_UTILS_JSON_ADD_NAMED_OBJECT ("session_operation_auth_by_signature", {
          GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("token",
              gtuber_utils_json_get_string (reader, "token", NULL));
          GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("signature",
              gtuber_utils_json_get_string (reader, "signature", NULL));
        });
      });
      GTUBER_UTILS_JSON_ADD_NAMED_OBJECT ("content_auth", {
        /* Auth type takes value from key named as used protocol name */
        GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("auth_type",
            gtuber_utils_json_get_string (reader, "authTypes", proto_name, NULL));

        GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("service_id", "nicovideo");
        GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("service_user_id",
            gtuber_utils_json_get_string (reader, "serviceUserId", NULL));
        GTUBER_UTILS_JSON_ADD_KEY_VAL_INT ("content_key_timeout",
            gtuber_utils_json_get_int (reader, "contentKeyTimeout", NULL));
      });
      GTUBER_UTILS_JSON_ADD_NAMED_OBJECT ("client_info", {
        GTUBER_UTILS_JSON_ADD_KEY_VAL_STRING ("player_id",
            gtuber_utils_json_get_string (reader, "playerId", NULL));
      });
      GTUBER_UTILS_JSON_ADD_KEY_VAL_INT ("priority",
          gtuber_utils_json_get_int (reader, "priority", NULL));
    });
  });

  if (req_body && self->api_uri) {
    SoupMessage *msg;
    gchar *post_uri;

    post_uri = g_strdup_printf ("%s?_format=json", self->api_uri);

    g_debug ("POST URI: %s", post_uri);
    msg = soup_message_new ("POST", post_uri);
    g_free (post_uri);

    g_debug ("POST body: %s", req_body);
    gtuber_utils_common_msg_take_request (msg, "application/json", req_body);

    return msg;
  }

fail:
  g_free (req_body);

  g_set_error (error, GTUBER_WEBSITE_ERROR,
      GTUBER_WEBSITE_ERROR_REQUEST_CREATE_FAILED,
      "API data could not be parsed or is missing some values");
  return NULL;
}

static GtuberFlow
gtuber_niconico_create_request (GtuberWebsite *website,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error)
{
  GtuberNiconico *self = GTUBER_NICONICO (website);
  SoupMessageHeaders *headers;
  gchar *referer;

  /* First, download HTML */
  if (!self->api_data) {
    *msg = soup_message_new ("GET", gtuber_website_get_uri_string (website));
    goto have_msg;
  }

  /* Post using API data */
  if (!self->hls_uri) {
    JsonParser *parser = json_parser_new ();

    if (json_parser_load_from_data (parser, self->api_data, -1, error)) {
      JsonReader *reader = json_reader_new (json_parser_get_root (parser));

      if (!self->can_stream) {
        gtuber_utils_json_parser_debug (parser);
        *msg = _obtain_nvapi_post_msg (self, reader, error);
      } else {
        *msg = _parse_and_obtain_session_post_msg (self, reader, info, error);
      }
      g_object_unref (reader);
    }
    g_object_unref (parser);

    if (*msg && *error == NULL)
      goto have_msg;

    return GTUBER_FLOW_ERROR;
  }

  /* Finally, get HLS */
  *msg = soup_message_new ("GET", self->hls_uri);

have_msg:
  referer = g_uri_to_string_partial (gtuber_website_get_uri (website),
      G_URI_HIDE_QUERY | G_URI_HIDE_FRAGMENT);

  headers = soup_message_get_request_headers (*msg);
  soup_message_headers_replace (headers, "Origin", "https://www.nicovideo.jp");
  soup_message_headers_replace (headers, "Referer", referer);

  g_free (referer);

  return GTUBER_FLOW_OK;
}

static GtuberFlow
gtuber_niconico_parse_input_stream (GtuberWebsite *website,
    GInputStream *stream, GtuberMediaInfo *info, GError **error)
{
  GtuberNiconico *self = GTUBER_NICONICO (website);
  JsonParser *parser;

  /* Extract API data from XML first */
  if (!self->api_data) {
    xmlDoc *doc;
    gchar *data;

    if (!(data = gtuber_utils_common_input_stream_to_data (stream, error)))
      return GTUBER_FLOW_ERROR;

    doc = gtuber_utils_xml_load_html_from_data (data, error);
    g_free (data);

    if (!doc)
      return GTUBER_FLOW_ERROR;

    self->api_data = g_strdup (gtuber_utils_xml_get_property_content (doc, "data-api-data"));
    xmlFreeDoc (doc);

    if (!self->api_data) {
      g_set_error (error, GTUBER_WEBSITE_ERROR,
          GTUBER_WEBSITE_ERROR_PARSE_FAILED,
          "Could not extract niconico API data");
      return GTUBER_FLOW_ERROR;
    }

    return GTUBER_FLOW_RESTART;
  }

  if (self->hls_uri) {
    if (gtuber_utils_common_parse_hls_input_stream_with_base_uri (stream,
        info, self->hls_uri, error)) {
      GPtrArray *astreams;
      guint i;

      astreams = gtuber_media_info_get_adaptive_streams (info);
      for (i = 0; i < astreams->len; i++) {
        GtuberStream *stream;

        stream = GTUBER_STREAM (g_ptr_array_index (astreams, i));

        /* XXX: Niconico does only "avc1" + "mp4a" AFAIK,
         * original HLS does not have codecs set, so fill them here */
        if (!gtuber_stream_get_video_codec (stream)
            && !gtuber_stream_get_audio_codec (stream))
          gtuber_stream_set_codecs (stream, "avc1", "mp4a");
      }

      return GTUBER_FLOW_OK;
    }

    return GTUBER_FLOW_ERROR;
  }

  parser = json_parser_new ();
  if (json_parser_load_from_stream (parser, stream, NULL, error)) {
    JsonReader *reader;

    gtuber_utils_json_parser_debug (parser);
    reader = json_reader_new (json_parser_get_root (parser));

    /* If we cannot stream here yet, it means we are in middle
     * of trying to obtain streaming permissions, so check them */
    if (!self->can_stream) {
      if (!(self->can_stream = (gtuber_utils_json_get_int (reader,
          "meta", "status", NULL) == 200))) {
        g_set_error (error, GTUBER_WEBSITE_ERROR,
            GTUBER_WEBSITE_ERROR_PARSE_FAILED,
            "Could not obtain streaming permission");
      }
    } else {
      /* We have HLS URI now, so also add heartbeat */
      if ((self->hls_uri = g_strdup (gtuber_utils_json_get_string (reader,
          "data", "session", "content_uri", NULL)))) {
        GtuberHeartbeat *heartbeat;
        guint interval;

        heartbeat = gtuber_niconico_heartbeat_new (self->api_uri,
            json_parser_get_root (parser));

        /* 1/3 of lifetime as interval */
        interval = (gtuber_utils_json_get_int (reader, "data", "session",
            "keep_method", "heartbeat", "lifetime", NULL) / 3);
        g_debug ("Heartbeat interval: %ums", interval);
        gtuber_heartbeat_set_interval (heartbeat, interval);

        gtuber_media_info_take_heartbeat (info, heartbeat);
      } else {
        g_set_error (error, GTUBER_WEBSITE_ERROR,
            GTUBER_WEBSITE_ERROR_PARSE_FAILED,
            "No content URI in session response");
      }
    }
    g_object_unref (reader);
  }
  g_object_unref (parser);

  if (*error)
    return GTUBER_FLOW_ERROR;

  return GTUBER_FLOW_RESTART;
}

static void
gtuber_niconico_class_init (GtuberNiconicoClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtuberWebsiteClass *website_class = (GtuberWebsiteClass *) klass;

  gobject_class->finalize = gtuber_niconico_finalize;

  website_class->create_request = gtuber_niconico_create_request;
  website_class->parse_input_stream = gtuber_niconico_parse_input_stream;
}

GtuberWebsite *
plugin_query (GUri *uri)
{
  gchar *id;

  id = gtuber_utils_common_obtain_uri_id_from_paths (uri, NULL,
      "/watch/", NULL);

  if (id) {
    GtuberNiconico *niconico;

    niconico = gtuber_niconico_new ();
    niconico->video_id = id;

    g_debug ("Requested video: %s", niconico->video_id);

    return GTUBER_WEBSITE (niconico);
  }

  return NULL;
}
