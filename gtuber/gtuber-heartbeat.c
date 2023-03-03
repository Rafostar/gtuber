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

/**
 * SECTION:gtuber-heartbeat
 * @title: GtuberHeartbeat
 * @short_description: a base class for creating heartbeat objects
 */

#include "gtuber-heartbeat.h"
#include "gtuber-heartbeat-private.h"

struct _GtuberHeartbeatPrivate
{
  SoupSession *session;

  GSource *ping_source;
  GCancellable *cancellable;
  guint interval;

  GHashTable *req_headers;
};

#define parent_class gtuber_heartbeat_parent_class
G_DEFINE_TYPE_WITH_CODE (GtuberHeartbeat, gtuber_heartbeat, GTUBER_TYPE_THREADED_OBJECT,
    G_ADD_PRIVATE (GtuberHeartbeat))
G_DEFINE_QUARK (gtuberheartbeat-error-quark, gtuber_heartbeat_error)

static void
insert_header_cb (const gchar *name, const gchar *value, SoupMessageHeaders *headers)
{
  if (!soup_message_headers_get_one (headers, name)) {
    soup_message_headers_append (headers, name, value);
    g_debug ("Heartbeat added request header, %s: %s", name, value);
  }
}

static gboolean
ping_cb (GtuberHeartbeat *self)
{
  GtuberHeartbeatPrivate *priv = gtuber_heartbeat_get_instance_private (self);
  GtuberHeartbeatClass *heartbeat_class = GTUBER_HEARTBEAT_GET_CLASS (self);
  SoupMessageHeaders *headers;
  SoupMessage *msg = NULL;
  GInputStream *stream = NULL;
  GError *my_error = NULL;
  GtuberFlow flow;

  g_debug ("Heartbeat invoked, thread: %p", g_thread_self ());

  GTUBER_THREADED_OBJECT_LOCK (self);

  if (g_cancellable_is_cancelled (priv->cancellable))
    g_cancellable_reset (priv->cancellable);

  GTUBER_THREADED_OBJECT_UNLOCK (self);

beginning:
  g_debug ("Heartbeat ping");
  flow = heartbeat_class->ping (self, &msg, &my_error);

  if (my_error)
    flow = GTUBER_FLOW_ERROR;
  if (flow != GTUBER_FLOW_OK)
    goto decide_flow;
  if (!msg)
    goto finish;

  headers = soup_message_get_request_headers (msg);
  g_hash_table_foreach (priv->req_headers, (GHFunc) insert_header_cb, headers);

  stream = soup_session_send (priv->session, msg, priv->cancellable, &my_error);

  if (!my_error) {
    g_debug ("Heartbeat pong");
    flow = heartbeat_class->pong (self, msg, stream, &my_error);
    if (my_error)
      flow = GTUBER_FLOW_ERROR;
  }
  if (stream) {
    if (g_input_stream_close (stream, NULL, NULL))
      g_debug ("Heartbeat input stream closed");
    else
      g_warning ("Heartbeat input stream could not be closed");

    g_object_unref (stream);
  }
  if (flow != GTUBER_FLOW_OK)
    goto decide_flow;

finish:
  if (!msg && !my_error) {
    g_set_error (&my_error, GTUBER_HEARTBEAT_ERROR,
        GTUBER_HEARTBEAT_ERROR_PING_FAILED,
        "Heartbeat ping message has not been created");
  }
  g_clear_object (&msg);

  if (my_error) {
    g_debug ("%s, stopping heartbeat", my_error->message);
    g_clear_error (&my_error);

    return G_SOURCE_REMOVE;
  }

  return G_SOURCE_CONTINUE;

decide_flow:
  switch (flow) {
    case GTUBER_FLOW_RESTART:
      g_clear_object (&msg);
      goto beginning;
    case GTUBER_FLOW_ERROR:
      if (!my_error) {
        g_set_error (&my_error, GTUBER_HEARTBEAT_ERROR,
            GTUBER_HEARTBEAT_ERROR_OTHER,
            "Heartbeat encountered an error");
      }
      goto finish;
    default:
      break;
  }

  g_assert_not_reached ();
  return G_SOURCE_REMOVE;
}

/* Call with lock */
static void
_add_ping_source (GtuberHeartbeat *self)
{
  GtuberHeartbeatPrivate *priv = gtuber_heartbeat_get_instance_private (self);
  GtuberThreadedObject *threaded_object = GTUBER_THREADED_OBJECT (self);

  g_return_if_fail (priv->ping_source == NULL);
  g_return_if_fail (priv->interval > 0);

  g_debug ("Heartbeat start");

  priv->ping_source = g_timeout_source_new (priv->interval);
  g_source_set_callback (priv->ping_source,
      (GSourceFunc) ping_cb, self, NULL);
  g_source_attach (priv->ping_source,
      gtuber_threaded_object_get_context (threaded_object));
}

/* Call with lock */
static void
_remove_ping_source (GtuberHeartbeat *self)
{
  GtuberHeartbeatPrivate *priv = gtuber_heartbeat_get_instance_private (self);

  if (!priv->ping_source)
    return;

  g_debug ("Heartbeat stop");

  g_cancellable_cancel (priv->cancellable);

  g_source_destroy (priv->ping_source);
  g_source_unref (priv->ping_source);
  priv->ping_source = NULL;
}

static void
gtuber_heartbeat_thread_start (GtuberThreadedObject *threaded_object)
{
  GtuberHeartbeat *self = GTUBER_HEARTBEAT (threaded_object);
  GtuberHeartbeatPrivate *priv = gtuber_heartbeat_get_instance_private (self);

  /* Create Soup session after thread push */
  priv->session = soup_session_new ();
}

static void
gtuber_heartbeat_thread_stop (GtuberThreadedObject *threaded_object)
{
  GtuberHeartbeat *self = GTUBER_HEARTBEAT (threaded_object);
  GtuberHeartbeatPrivate *priv = gtuber_heartbeat_get_instance_private (self);

  g_object_unref (priv->session);
}

static GtuberFlow
gtuber_heartbeat_ping (GtuberHeartbeat *self,
    SoupMessage **msg, GError **error)
{
  return (*error == NULL) ? GTUBER_FLOW_OK : GTUBER_FLOW_ERROR;
}

static GtuberFlow
gtuber_heartbeat_pong (GtuberHeartbeat *self,
    SoupMessage *msg, GInputStream *stream, GError **error)
{
  return (*error == NULL) ? GTUBER_FLOW_OK : GTUBER_FLOW_ERROR;
}

static void
gtuber_heartbeat_init (GtuberHeartbeat *self)
{
  GtuberHeartbeatPrivate *priv = gtuber_heartbeat_get_instance_private (self);

  priv->cancellable = g_cancellable_new ();
}

static void
gtuber_heartbeat_dispose (GObject *object)
{
  GtuberHeartbeat *self = GTUBER_HEARTBEAT (object);

  GTUBER_THREADED_OBJECT_LOCK (self);

  /* Remove before stopping main loop, to not
   * trigger ping after Soup session is gone */
  _remove_ping_source (self);

  GTUBER_THREADED_OBJECT_UNLOCK (self);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gtuber_heartbeat_finalize (GObject *object)
{
  GtuberHeartbeat *self = GTUBER_HEARTBEAT (object);
  GtuberHeartbeatPrivate *priv = gtuber_heartbeat_get_instance_private (self);

  g_debug ("Heartbeat finalize");

  g_object_unref (priv->cancellable);
  if (priv->req_headers)
    g_hash_table_unref (priv->req_headers);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtuber_heartbeat_class_init (GtuberHeartbeatClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtuberThreadedObjectClass *threaded_object_class = (GtuberThreadedObjectClass *) klass;
  GtuberHeartbeatClass *heartbeat_class = (GtuberHeartbeatClass *) klass;

  gobject_class->dispose = gtuber_heartbeat_dispose;
  gobject_class->finalize = gtuber_heartbeat_finalize;

  threaded_object_class->thread_start = gtuber_heartbeat_thread_start;
  threaded_object_class->thread_stop = gtuber_heartbeat_thread_stop;

  heartbeat_class->ping = gtuber_heartbeat_ping;
  heartbeat_class->pong = gtuber_heartbeat_pong;
}

/**
 * gtuber_heartbeat_set_interval:
 * @heartbeat: a #GtuberHeartbeat
 * @interval: ping interval in milliseconds
 *
 * Sets how often ping should be performed.
 * Interval value is in milliseconds.
 */
void
gtuber_heartbeat_set_interval (GtuberHeartbeat *self, guint interval)
{
  GtuberHeartbeatPrivate *priv;

  g_return_if_fail (GTUBER_IS_HEARTBEAT (self));
  g_return_if_fail (interval >= 1000);

  priv = gtuber_heartbeat_get_instance_private (self);

  GTUBER_THREADED_OBJECT_LOCK (self);
  if (priv->interval != interval) {
    priv->interval = interval;

    /* Update ping source if already running */
    if (priv->ping_source) {
      _remove_ping_source (self);
      _add_ping_source (self);
    }
  }
  GTUBER_THREADED_OBJECT_UNLOCK (self);
}

void
gtuber_heartbeat_start_with_headers (GtuberHeartbeat *self, GHashTable *req_headers)
{
  GtuberHeartbeatPrivate *priv = gtuber_heartbeat_get_instance_private (self);

  GTUBER_THREADED_OBJECT_LOCK (self);

  priv->req_headers = g_hash_table_ref (req_headers);
  _add_ping_source (self);

  GTUBER_THREADED_OBJECT_UNLOCK (self);
}
