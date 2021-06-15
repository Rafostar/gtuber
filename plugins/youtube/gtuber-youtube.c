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

#include <gtuber/gtuber.h>
#include <json-glib/json-glib.h>

#include "gtuber-youtube.h"

struct _GtuberYoutube
{
  GtuberWebsite parent;

  gchar *video_id;

  gchar *visitor_data;
  gchar *client_version;
  gint64 sts;
};

struct _GtuberYoutubeClass
{
  GtuberWebsiteClass parent_class;
};

#define parent_class gtuber_youtube_parent_class
G_DEFINE_TYPE (GtuberYoutube, gtuber_youtube, GTUBER_TYPE_WEBSITE)

static void gtuber_youtube_finalize (GObject *object);

static GtuberFlow gtuber_youtube_create_request (GtuberWebsite *website,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error);
static GtuberFlow gtuber_youtube_parse_input_stream (GtuberWebsite *website,
    GInputStream *stream, GtuberMediaInfo *info, GError **error);

static void
gtuber_youtube_init (GtuberYoutube *self)
{
  self = gtuber_youtube_get_instance_private (self);

  self->visitor_data = g_strdup ("");
  self->client_version = g_strdup ("2.20210605.09.00");
  self->sts = 0;
}

static void
gtuber_youtube_class_init (GtuberYoutubeClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtuberWebsiteClass *website_class = (GtuberWebsiteClass *) klass;

  gobject_class->finalize = gtuber_youtube_finalize;

  website_class->handles_input_stream = TRUE;
  website_class->create_request = gtuber_youtube_create_request;
  website_class->parse_input_stream = gtuber_youtube_parse_input_stream;
}

static void
gtuber_youtube_finalize (GObject *object)
{
  GtuberYoutube *self = GTUBER_YOUTUBE (object);

  g_debug ("Youtube finalize");

  g_free (self->video_id);

  g_free (self->visitor_data);
  g_free (self->client_version);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static gchar *
gtuber_youtube_get_player_req_body (GtuberYoutube *self, GtuberMediaInfo *info)
{
  JsonBuilder *builder;
  JsonGenerator *gen;
  JsonNode *root;
  gchar *ua, *yt_url, *req_body;

  ua = g_strdup_printf ("%s,gzip(gfe)",
      gtuber_website_get_user_agent (GTUBER_WEBSITE (self)));
  yt_url = g_strdup_printf ("https://www.youtube.com/watch?v=%s", self->video_id);

  builder = json_builder_new ();
  json_builder_begin_object (builder);

  json_builder_set_member_name (builder, "videoId");
  json_builder_add_string_value (builder, self->video_id);

  json_builder_set_member_name (builder, "context");
  json_builder_begin_object (builder);
  {
    json_builder_set_member_name (builder, "client");
    json_builder_begin_object (builder);
    {
      json_builder_set_member_name (builder, "visitorData");
      json_builder_add_string_value (builder, self->visitor_data);

      json_builder_set_member_name (builder, "userAgent");
      json_builder_add_string_value (builder, ua);

      json_builder_set_member_name (builder, "clientName");
      json_builder_add_string_value (builder, "WEB");

      json_builder_set_member_name (builder, "clientVersion");
      json_builder_add_string_value (builder, self->client_version);

      json_builder_set_member_name (builder, "osName");
      json_builder_add_string_value (builder, "X11");

      json_builder_set_member_name (builder, "osVersion");
      json_builder_add_string_value (builder, "");

      json_builder_set_member_name (builder, "originalUrl");
      json_builder_add_string_value (builder, yt_url);

      json_builder_set_member_name (builder, "browserName");
      json_builder_add_string_value (builder, "Firefox");

      json_builder_set_member_name (builder, "browserVersion");
      json_builder_add_string_value (builder,
          gtuber_website_get_browser_version (GTUBER_WEBSITE (self)));

      json_builder_set_member_name (builder, "playerType");
      json_builder_add_string_value (builder, "UNIPLAYER");

      json_builder_end_object (builder);
    }
    json_builder_set_member_name (builder, "user");
    json_builder_begin_object (builder);
    {
      json_builder_set_member_name (builder, "lockedSafetyMode");
      json_builder_add_boolean_value (builder, FALSE);

      json_builder_end_object (builder);
    }
    json_builder_set_member_name (builder, "request");
    json_builder_begin_object (builder);
    {
      json_builder_set_member_name (builder, "useSsl");
      json_builder_add_boolean_value (builder, TRUE);

      json_builder_set_member_name (builder, "internalExperimentFlags");
      json_builder_begin_array (builder);
      json_builder_end_array (builder);

      json_builder_set_member_name (builder, "consistencyTokenJars");
      json_builder_begin_array (builder);
      json_builder_end_array (builder);

      json_builder_end_object (builder);
    }
    json_builder_end_object (builder);
  }
  json_builder_set_member_name (builder, "playbackContext");
  json_builder_begin_object (builder);
  {
    json_builder_set_member_name (builder, "contentPlaybackContext");
    json_builder_begin_object (builder);
    {
      json_builder_set_member_name (builder, "html5Preference");
      json_builder_add_string_value (builder, "HTML5_PREF_WANTS");

      json_builder_set_member_name (builder, "lactMilliseconds");
      json_builder_add_string_value (builder, "1069");

      json_builder_set_member_name (builder, "referer");
      json_builder_add_string_value (builder, yt_url);

      json_builder_set_member_name (builder, "signatureTimestamp");
      json_builder_add_int_value (builder, self->sts);

      json_builder_set_member_name (builder, "autoCaptionsDefaultOn");
      json_builder_add_boolean_value (builder, FALSE);

      json_builder_set_member_name (builder, "liveContext");
      json_builder_begin_object (builder);
      {
        json_builder_set_member_name (builder, "startWalltime");
        json_builder_add_string_value (builder, "0");

        json_builder_end_object (builder);
      }
      json_builder_end_object (builder);
    }
    json_builder_end_object (builder);
  }
  json_builder_set_member_name (builder, "captionParams");
  json_builder_begin_object (builder);
  {
    json_builder_end_object (builder);
  }
  json_builder_end_object (builder);

  root = json_builder_get_root (builder);
  gen = json_generator_new ();
  json_generator_set_root (gen, root);

  req_body = json_generator_to_data (gen, NULL);

  json_node_free (root);
  g_object_unref (gen);
  g_object_unref (builder);

  g_free (ua);
  g_free (yt_url);

  return req_body;
}

static gboolean
read_member (JsonReader *reader, const gchar *member,
    gboolean *had_err)
{
  gboolean success;

  success = json_reader_read_member (reader, member);

  /* For required members */
  if (had_err) {
    if (!success)
      g_debug ("Error reading member: %s", member);
      *had_err |= !success;
  }

  return success;
}

static gboolean
parse_stream (JsonReader *reader, GtuberStream *stream)
{
  GHashTable *params;
  gboolean had_err = FALSE;

  if (read_member (reader, "itag", &had_err))
    gtuber_stream_set_itag (stream, json_reader_get_int_value (reader));
  json_reader_end_member (reader);
  if (read_member (reader, "url", &had_err))
    gtuber_stream_set_uri (stream, json_reader_get_string_value (reader));
  json_reader_end_member (reader);
  if (read_member (reader, "bitrate", NULL))
    gtuber_stream_set_bitrate (stream, json_reader_get_int_value (reader));
  json_reader_end_member (reader);
  if (read_member (reader, "width", NULL))
    gtuber_stream_set_width (stream, json_reader_get_int_value (reader));
  json_reader_end_member (reader);
  if (read_member (reader, "height", NULL))
    gtuber_stream_set_height (stream, json_reader_get_int_value (reader));
  json_reader_end_member (reader);
  if (read_member (reader, "fps", NULL))
    gtuber_stream_set_fps (stream, json_reader_get_int_value (reader));
  json_reader_end_member (reader);

  /* Parse mime type and codecs */
  if (read_member (reader, "mimeType", NULL)) {
    gchar **strv;

    strv = g_strsplit (json_reader_get_string_value (reader), ";", 2);

    if (strv[1]) {
      params = g_uri_parse_params (g_strstrip (strv[1]),
          -1, ";", G_URI_PARAMS_WWW_FORM, NULL);
      if (params) {
        GtuberStreamMimeType mime_type;
        gchar *codecs;

        mime_type = gtuber_utils_get_mime_type_from_string (strv[0]);
        gtuber_stream_set_mime_type (stream, mime_type);

        codecs = g_hash_table_lookup (params, "codecs");
        g_strstrip (g_strdelimit (codecs, "\"", ' '));

        switch (mime_type) {
          case GTUBER_STREAM_MIME_TYPE_AUDIO_MP4:
          case GTUBER_STREAM_MIME_TYPE_AUDIO_WEBM:
            gtuber_stream_set_codecs (stream, NULL, codecs);
            break;
          default:{
            gchar **cstrv;

            cstrv = g_strsplit (codecs, ",", 2);
            if (g_strv_length (cstrv) > 1)
              gtuber_stream_set_codecs (stream, cstrv[0], g_strstrip (cstrv[1]));
            else
              gtuber_stream_set_codecs (stream, cstrv[0], NULL);

            g_strfreev (cstrv);
            break;
          }
        }
        g_hash_table_remove_all (params);
      }
    }
    g_strfreev (strv);
  }
  json_reader_end_member (reader);

  return !had_err;
}

static void
gtuber_youtube_parse_data (GtuberYoutube *self,
    JsonParser *parser, GtuberMediaInfo *info, GError **error)
{
  JsonReader *reader;
  gboolean had_err = FALSE;

  reader = json_reader_new (json_parser_get_root (parser));

  /* Check if video is playable */
  if (read_member (reader, "playabilityStatus", &had_err)) {
    if (read_member (reader, "status", &had_err)) {
      if (strcmp (json_reader_get_string_value (reader), "OK") != 0) {
        g_set_error (error, GTUBER_WEBSITE_ERROR, GTUBER_WEBSITE_ERROR_OTHER,
            "Video is not playable");
        goto finish;
      }
    }
    json_reader_end_member (reader);
  }
  json_reader_end_member (reader);

  if (read_member (reader, "videoDetails", &had_err)) {
    if (read_member (reader, "videoId", &had_err))
      gtuber_media_info_set_id (info, json_reader_get_string_value (reader));
    json_reader_end_member (reader);
    if (read_member (reader, "title", &had_err))
      gtuber_media_info_set_title (info, json_reader_get_string_value (reader));
    json_reader_end_member (reader);
    if (read_member (reader, "shortDescription", &had_err))
      gtuber_media_info_set_description (info, json_reader_get_string_value (reader));
    json_reader_end_member (reader);
    if (read_member (reader, "lengthSeconds", &had_err))
      gtuber_media_info_set_duration (info, g_ascii_strtoull (
          json_reader_get_string_value (reader), NULL, 10));
    json_reader_end_member (reader);
  }
  json_reader_end_member (reader);

  if (read_member (reader, "streamingData", &had_err)) {
    gint i, count;

    /* Add streams */
    if (read_member (reader, "formats", &had_err)) {
      if (json_reader_is_array (reader)) {
        count = json_reader_count_elements (reader);
        for (i = 0; i < count; i++) {
          if (json_reader_read_element (reader, i)) {
            GtuberStream *stream;

            stream = gtuber_stream_new ();
            if (parse_stream (reader, stream))
              gtuber_media_info_add_stream (info, stream);
            else
              g_object_unref (stream);
          }
          json_reader_end_element (reader);
        }
      }
    }
    json_reader_end_member (reader);

    /* Add adaptive streams */
    if (read_member (reader, "adaptiveFormats", &had_err)) {
      if (json_reader_is_array (reader)) {
        count = json_reader_count_elements (reader);
        for (i = 0; i < count; i++) {
          if (json_reader_read_element (reader, i)) {
            GtuberAdaptiveStream *adaptive_stream;

            adaptive_stream = gtuber_adaptive_stream_new ();
            if (parse_stream (reader, GTUBER_STREAM (adaptive_stream))) {
              if (read_member (reader, "initRange", &had_err)) {
                guint64 start = 0, end = 0;

                if (read_member (reader, "start", &had_err))
                  start = g_ascii_strtoull (json_reader_get_string_value (reader), NULL, 10);
                json_reader_end_member (reader);
                if (read_member (reader, "end", &had_err))
                  end = g_ascii_strtoull (json_reader_get_string_value (reader), NULL, 10);
                json_reader_end_member (reader);

                gtuber_adaptive_stream_set_init_range (adaptive_stream, start, end);
              }
              json_reader_end_member (reader);

              if (read_member (reader, "indexRange", &had_err)) {
                guint64 start = 0, end = 0;

                if (read_member (reader, "start", &had_err))
                  start = g_ascii_strtoull (json_reader_get_string_value (reader), NULL, 10);
                json_reader_end_member (reader);
                if (read_member (reader, "end", &had_err))
                  end = g_ascii_strtoull (json_reader_get_string_value (reader), NULL, 10);
                json_reader_end_member (reader);

                gtuber_adaptive_stream_set_index_range (adaptive_stream, start, end);
              }
              json_reader_end_member (reader);

              gtuber_media_info_add_adaptive_stream (info, adaptive_stream);
            } else {
              g_object_unref (adaptive_stream);
            }
          }
          json_reader_end_element (reader);
        }
      }
    }
    json_reader_end_member (reader);
  }
  json_reader_end_member (reader);

  /* TODO: Update cache */
  if (read_member (reader, "responseContext", &had_err)) {
    if (read_member (reader, "visitorData", &had_err)) {
      const gchar *visitor_data;

      visitor_data = json_reader_get_string_value (reader);
      if (visitor_data && strcmp (self->visitor_data, visitor_data) != 0) {
        g_free (self->visitor_data);
        self->visitor_data = g_strdup (json_reader_get_string_value (reader));
        g_debug ("Updated visitor_data: %s", self->visitor_data);
      }
      json_reader_end_member (reader);
    }
    json_reader_end_member (reader);
  }

finish:
  if (had_err)
    g_debug ("One or more YouTube data values could not be parsed");

  g_object_unref (reader);
}

static GtuberFlow
gtuber_youtube_create_request (GtuberWebsite *website,
    GtuberMediaInfo *info, SoupMessage **msg, GError **error)
{
  GtuberYoutube *self = GTUBER_YOUTUBE (website);
  gchar *req_body;

  req_body = gtuber_youtube_get_player_req_body (self, info);
  if (G_UNLIKELY (req_body == NULL)) {
    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_REQUEST_CREATE_FAILED,
        "Could not assemble request body");
    return GTUBER_FLOW_ERROR;
  }
  g_debug ("Post: %s", req_body);

  *msg = soup_message_new ("POST",
      "https://www.youtube.com/youtubei/v1/player?"
      "key=AIzaSyAO_FJ2SlqU8Q4STEHLGCilw_Y9_11qcW8");
  soup_message_set_request (*msg, "application/json",
      SOUP_MEMORY_TAKE, req_body, strlen (req_body));

  return GTUBER_FLOW_OK;
}

static GtuberFlow
gtuber_youtube_parse_input_stream (GtuberWebsite *website,
    GInputStream *stream, GtuberMediaInfo *info, GError **error)
{
  GtuberYoutube *self = GTUBER_YOUTUBE (website);
  JsonParser *parser;
  GError *my_error = NULL;

  parser = json_parser_new ();
  json_parser_load_from_stream (parser, stream, NULL, &my_error);
  if (!my_error) {
#if GLIB_CHECK_VERSION (2,68,0)
    if (!g_log_writer_default_would_drop (G_LOG_LEVEL_DEBUG, "GtuberYoutube"))
#endif
    {
      JsonGenerator *gen;
      gchar *res_data;

      gen = json_generator_new ();
      json_generator_set_pretty (gen, TRUE);
      json_generator_set_root (gen, json_parser_get_root (parser));
      res_data = json_generator_to_data (gen, NULL);
      g_debug ("%s", res_data);

      g_free (res_data);
      g_object_unref (gen);
    }

    gtuber_youtube_parse_data (self, parser, info, &my_error);
  }
  g_object_unref (parser);

  if (my_error) {
    g_propagate_error (error, my_error);
    return GTUBER_FLOW_ERROR;
  }

  return GTUBER_FLOW_OK;
}

GtuberWebsite *
query_plugin (GUri *uri)
{
  GtuberYoutube *youtube;
  GHashTable *params;
  const gchar *host, *query;
  gchar *video_id;

  host = g_uri_get_host (uri);

  if (!g_str_has_suffix (host, "youtube.com"))
    goto try_path;

  query = g_uri_get_query (uri);
  if (!query)
    goto fail;

  g_debug ("URI query: %s", query);

  params = g_uri_parse_params (query, -1,
      "&amp;", G_URI_PARAMS_NONE, NULL);

  video_id = g_strdup (g_hash_table_lookup (params, "v"));
  g_hash_table_remove_all (params);
  if (video_id)
    goto create;

try_path:
  if (!g_str_has_suffix (host, "youtu.be"))
    goto fail;
  else {
    const gchar *path = g_uri_get_path (uri);

    g_debug ("URI path: %s", path);

    if (!path || strlen (path) != 12)
      goto fail;

    video_id = g_strdup (path + 1);
    if (!video_id)
      goto fail;
  }

create:
  g_debug ("Video ID: %s", video_id);

  youtube = g_object_new (GTUBER_TYPE_YOUTUBE, NULL);
  youtube->video_id = video_id;

  return GTUBER_WEBSITE (youtube);

fail:
  g_debug ("Unsupported URI");

  return NULL;
}
