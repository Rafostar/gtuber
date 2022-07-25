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

#include "utils/common/gtuber-utils-common.h"
#include "utils/json/gtuber-utils-json.h"

#include "gtuber-niconico-heartbeat.h"

struct _GtuberNiconicoHeartbeat
{
  GtuberHeartbeat parent;

  gchar *api_uri;
  JsonNode *root;
};

#define parent_class gtuber_niconico_heartbeat_parent_class
G_DEFINE_TYPE (GtuberNiconicoHeartbeat, gtuber_niconico_heartbeat,
    GTUBER_TYPE_HEARTBEAT)

static void
gtuber_niconico_heartbeat_init (GtuberNiconicoHeartbeat *self)
{
}

static void
gtuber_niconico_heartbeat_finalize (GObject *object)
{
  GtuberNiconicoHeartbeat *self = GTUBER_NICONICO_HEARTBEAT (object);

  g_free (self->api_uri);

  if (self->root)
    json_node_unref (self->root);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GtuberFlow
gtuber_niconico_heartbeat_ping (GtuberHeartbeat *heartbeat,
    SoupMessage **msg, GError **error)
{
  GtuberNiconicoHeartbeat *self = GTUBER_NICONICO_HEARTBEAT (heartbeat);
  JsonObject *obj;
  JsonNode *data_node;
  JsonReader *reader;
  gchar *uri, *req_body;
  const gchar *err_msg, *session_id;

  if (!JSON_NODE_HOLDS_OBJECT (self->root)) {
    err_msg = "API data JSON node is invalid";
    goto fail;
  }

  obj = json_node_get_object (self->root);
  if (!json_object_has_member (obj, "data")) {
    err_msg = "API data JSON is missing \"data\" object";
    goto fail;
  }
  data_node = json_object_get_member (obj, "data");

  reader = json_reader_new (data_node);
  session_id = gtuber_utils_json_get_string (reader, "session", "id", NULL);
  g_object_unref (reader);

  if (!session_id) {
    err_msg = "API data JSON is missing session id";
    goto fail;
  }

  uri = g_strdup_printf ("%s/%s?_format=json&_method=PUT",
      self->api_uri, session_id);
  g_debug ("Heartbeat ping URI: %s", uri);

  *msg = soup_message_new ("POST", uri);
  g_free (uri);

  req_body = json_to_string (data_node, TRUE);
  g_debug ("Heartbeat ping data: %s", req_body);
  gtuber_utils_common_msg_take_request (*msg, "application/json", req_body);

  return GTUBER_FLOW_OK;

fail:
  g_set_error (error, GTUBER_WEBSITE_ERROR,
      GTUBER_HEARTBEAT_ERROR_PING_FAILED,
      "%s", err_msg);
  return GTUBER_FLOW_ERROR;
}

static GtuberFlow
gtuber_niconico_heartbeat_pong (GtuberHeartbeat *heartbeat,
    SoupMessage *msg, GInputStream *stream, GError **error)
{
  GtuberNiconicoHeartbeat *self = GTUBER_NICONICO_HEARTBEAT (heartbeat);
  JsonParser *parser;

  parser = json_parser_new ();
  if (json_parser_load_from_stream (parser, stream, NULL, error)) {
    gtuber_utils_json_parser_debug (parser);

    /* Replace JSON node used for next ping */
    json_node_unref (self->root);
    self->root = json_parser_steal_root (parser);
    g_debug ("Replaced API data JSON node");
  }
  g_object_unref (parser);

  return GTUBER_FLOW_OK;
}

static void
gtuber_niconico_heartbeat_class_init (GtuberNiconicoHeartbeatClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtuberHeartbeatClass *heartbeat_class = (GtuberHeartbeatClass *) klass;

  gobject_class->finalize = gtuber_niconico_heartbeat_finalize;

  heartbeat_class->ping = gtuber_niconico_heartbeat_ping;
  heartbeat_class->pong = gtuber_niconico_heartbeat_pong;
}

GtuberHeartbeat *
gtuber_niconico_heartbeat_new (const gchar *api_uri, JsonNode *root)
{
  GtuberNiconicoHeartbeat *self;

  g_return_val_if_fail (api_uri != NULL, NULL);
  g_return_val_if_fail (root != NULL, NULL);

  self = g_object_new (GTUBER_TYPE_NICONICO_HEARTBEAT, NULL);
  self->api_uri = g_strdup (api_uri);
  self->root = json_node_ref (root);

  g_debug ("Heartbeat API URI: %s", self->api_uri);

  return GTUBER_HEARTBEAT (self);
}
