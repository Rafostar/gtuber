/*
 * Copyright (C) 2023 Rafał Dzięgiel <rafostar.github@gmail.com>
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
 * SECTION:gtuber-proxy
 * @title: GtuberProxy
 * @short_description: a base class for creating proxy objects
 */

#include "gtuber-proxy.h"
#include "gtuber-proxy-private.h"

struct _GtuberProxyPrivate
{
  SoupServer *server;

  gchar *id;
  GPtrArray *streams;
  GPtrArray *adaptive_streams;
  GHashTable *req_headers;
};

#define parent_class gtuber_proxy_parent_class
G_DEFINE_TYPE_WITH_CODE (GtuberProxy, gtuber_proxy, GTUBER_TYPE_THREADED_OBJECT,
    G_ADD_PRIVATE (GtuberProxy))
G_DEFINE_QUARK (gtuberproxy-error-quark, gtuber_proxy_error)

static void
insert_header_cb (const gchar *name, const gchar *value, SoupMessageHeaders *headers)
{
  soup_message_headers_replace (headers, name, value);
  g_debug ("Proxy set request header, %s: %s", name, value);
}

static GtuberStream *
find_stream_for_itag (const GPtrArray *streams, guint itag)
{
  gint i;

  for (i = 0; i < streams->len; ++i) {
    GtuberStream *stream = g_ptr_array_index (streams, i);

    if (gtuber_stream_get_itag (stream) == itag)
      return stream;
  }

  return NULL;
}

static void
fetch_id_cb (SoupServer *server, SoupServerMessage *msg,
    const char *path, GHashTable *query, GtuberProxy *self)
{
  GtuberProxyClass *proxy_class = GTUBER_PROXY_GET_CLASS (self);
  GtuberProxyPrivate *priv = gtuber_proxy_get_instance_private (self);
  GtuberStream *stream = NULL;
  SoupMessageHeaders *headers;
  GError *my_error = NULL;
  const gchar *itag_str = g_hash_table_lookup (query, "itag");
  GtuberFlow flow;

  if (itag_str) {
    guint itag = g_ascii_strtoull (itag_str, NULL, 10);

    if ((stream = find_stream_for_itag (priv->streams, itag)))
      goto fetch;
    if ((stream = find_stream_for_itag (priv->adaptive_streams, itag)))
      goto fetch;
  }

fetch:
  headers = soup_server_message_get_request_headers (msg);
  g_hash_table_foreach (priv->req_headers, (GHFunc) insert_header_cb, headers);

  flow = proxy_class->fetch_stream (self, msg, stream, &my_error);
  if (my_error)
    flow = GTUBER_FLOW_ERROR;

  if (flow == GTUBER_FLOW_ERROR)
    goto fail;

  g_clear_error (&my_error);

  if (flow == GTUBER_FLOW_RESTART)
    goto fetch;

  return;

fail:
  soup_server_message_set_status (msg, SOUP_STATUS_INTERNAL_SERVER_ERROR,
      (my_error) ? my_error->message : soup_status_get_phrase (SOUP_STATUS_INTERNAL_SERVER_ERROR));
  g_clear_error (&my_error);
}

static void
gtuber_proxy_thread_start (GtuberThreadedObject *threaded_object)
{
  GtuberProxy *self = GTUBER_PROXY (threaded_object);
  GtuberProxyPrivate *priv = gtuber_proxy_get_instance_private (self);

  /* Create Soup server after thread push */
  priv->server = soup_server_new (NULL, NULL);

  soup_server_add_handler (priv->server, "/", (SoupServerCallback) fetch_id_cb,
      self, NULL);
}

static void
gtuber_proxy_thread_stop (GtuberThreadedObject *threaded_object)
{
  GtuberProxy *self = GTUBER_PROXY (threaded_object);
  GtuberProxyPrivate *priv = gtuber_proxy_get_instance_private (self);

  g_object_unref (priv->server);
}

static GtuberFlow
gtuber_proxy_fetch_stream (GtuberProxy *proxy, SoupServerMessage *msg,
    GtuberStream *stream, GError **error)
{
  return (*error == NULL) ? GTUBER_FLOW_OK : GTUBER_FLOW_ERROR;
}

static void
gtuber_proxy_init (GtuberProxy *self)
{
}

static void
gtuber_proxy_finalize (GObject *object)
{
  GtuberProxy *self = GTUBER_PROXY (object);
  GtuberProxyPrivate *priv = gtuber_proxy_get_instance_private (self);

  g_debug ("Proxy finalize");

  g_free (priv->id);
  if (priv->streams)
    g_ptr_array_unref (priv->streams);
  if (priv->adaptive_streams)
    g_ptr_array_unref (priv->adaptive_streams);
  if (priv->req_headers)
    g_hash_table_unref (priv->req_headers);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtuber_proxy_class_init (GtuberProxyClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtuberThreadedObjectClass *threaded_object_class = (GtuberThreadedObjectClass *) klass;
  GtuberProxyClass *proxy_class = (GtuberProxyClass *) klass;

  gobject_class->finalize = gtuber_proxy_finalize;

  threaded_object_class->thread_start = gtuber_proxy_thread_start;
  threaded_object_class->thread_stop = gtuber_proxy_thread_stop;

  proxy_class->fetch_stream = gtuber_proxy_fetch_stream;
}

void
gtuber_proxy_configure (GtuberProxy *self, const gchar *id, GPtrArray *streams,
    GPtrArray *adaptive_streams, GHashTable *req_headers)
{
  GtuberProxyPrivate *priv = gtuber_proxy_get_instance_private (self);

  GTUBER_THREADED_OBJECT_LOCK (self);

  priv->id = g_strdup (id);
  priv->streams = g_ptr_array_ref (streams);
  priv->adaptive_streams = g_ptr_array_ref (adaptive_streams);
  priv->req_headers = g_hash_table_ref (req_headers);

  GTUBER_THREADED_OBJECT_UNLOCK (self);
}
