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
#include "gtuber-stream-private.h"

#define CHUNK_SIZE 8192

struct _GtuberProxyPrivate
{
  SoupServer *server;
  SoupSession *session;

  gchar *proxy_uri;
  gchar *media_path;

  GPtrArray *streams;
  GHashTable *req_headers;
  GHashTable *org_uris;
};

#define parent_class gtuber_proxy_parent_class
G_DEFINE_TYPE_WITH_CODE (GtuberProxy, gtuber_proxy, GTUBER_TYPE_THREADED_OBJECT,
    G_ADD_PRIVATE (GtuberProxy))
G_DEFINE_QUARK (gtuberproxy-error-quark, gtuber_proxy_error)

static void
copy_header_cb_unlocked (const gchar *key, const gchar *val, GtuberProxyPrivate *priv)
{
  g_hash_table_insert (priv->req_headers, g_strdup (key), g_strdup (val));
}

static void
insert_header_cb (const gchar *name, const gchar *value, SoupMessageHeaders *headers)
{
  soup_message_headers_replace (headers, name, value);
  //g_debug ("Proxy set request header, %s: %s", name, value);
}

static void
import_streams_unlocked (GtuberProxyPrivate *priv, GPtrArray *streams)
{
  int i;

  if (!priv->proxy_uri)
    return;

  for (i = 0; i < streams->len; ++i) {
    GtuberStream *stream = (GtuberStream *) g_ptr_array_index (streams, i);
    gchar *itag_str = g_strdup_printf ("%u", stream->itag);

    /* Move original URIs from stream to proxy */
    g_hash_table_insert (priv->org_uris, itag_str, stream->uri);
    stream->uri = g_strdup_printf ("%s%s?itag=%u", priv->proxy_uri, priv->media_path + 1, stream->itag);

    g_ptr_array_add (priv->streams, g_object_ref (stream));
  }
}

static void
finished_cb (SoupServerMessage *srv_msg, GCancellable *cancellable)
{
  g_debug ("FIN");

  /* When message finished too early (failed) */
  g_cancellable_cancel (cancellable);
  g_signal_handlers_disconnect_by_func (srv_msg, finished_cb, cancellable);
}

static void
stream_read_async_cb (GInputStream *istream, GAsyncResult *result, SoupServerMessage *srv_msg)
{
  GCancellable *cancellable = g_object_get_data (G_OBJECT (srv_msg), "cancellable");
  SoupMessageBody *msg_body;
  GBytes *bytes;
  GError *error = NULL;
  gsize size;

  bytes = g_input_stream_read_bytes_finish (istream, result, &error);

  if (error) {
    soup_server_message_set_status (srv_msg,
        SOUP_STATUS_INTERNAL_SERVER_ERROR, error->message);

    goto finish;
  }

  size = g_bytes_get_size (bytes);
  msg_body = soup_server_message_get_response_body (srv_msg);

  /* No more data (download done) */
  if (size == 0) {
    soup_message_body_complete (msg_body);
    soup_server_message_unpause (srv_msg);

    goto finish;
  }

  soup_message_body_append_bytes (msg_body, bytes);
  soup_server_message_unpause (srv_msg);

  /* All is fine, continue to read next chunk */
  g_input_stream_read_bytes_async (istream, CHUNK_SIZE, G_PRIORITY_DEFAULT,
      cancellable, (GAsyncReadyCallback) stream_read_async_cb, g_object_ref (srv_msg));

finish:
  if (bytes)
    g_bytes_unref (bytes);

  g_object_unref (srv_msg);
  g_clear_error (&error);
}

static void
send_msg_async_cb (SoupSession *session, GAsyncResult *result, SoupServerMessage *srv_msg)
{
  GInputStream *istream;
  GError *error = NULL;

  istream = soup_session_send_finish (session, result, &error);

  /* Message sent, start reading response */
  if (!error) {
    GCancellable *cancellable = g_object_get_data (G_OBJECT (srv_msg), "cancellable");

    g_input_stream_read_bytes_async (istream, CHUNK_SIZE, G_PRIORITY_DEFAULT,
        cancellable, (GAsyncReadyCallback) stream_read_async_cb, srv_msg);
  } else {
    soup_server_message_set_status (srv_msg,
        SOUP_STATUS_INTERNAL_SERVER_ERROR, error->message);
    soup_server_message_unpause (srv_msg);

    /* Free earlier reffed message */
    g_object_unref (srv_msg);
    g_error_free (error);
  }

  g_object_unref (istream);
}

static void
forward_response_headers (SoupMessage *cli_msg, SoupServerMessage *srv_msg)
{
  SoupMessageHeaders *cli_resp_headers = soup_message_get_response_headers (cli_msg);
  SoupMessageHeaders *srv_resp_headers = soup_server_message_get_response_headers (srv_msg);
  SoupStatus cli_status = soup_message_get_status (cli_msg);
  const char *cli_phrase = soup_message_get_reason_phrase (cli_msg);

  soup_server_message_set_status (srv_msg, cli_status, cli_phrase);
  soup_message_headers_foreach (cli_resp_headers,
      (SoupMessageHeadersForeachFunc) insert_header_cb, srv_resp_headers);
  //soup_message_headers_remove (srv_resp_headers, "Content-Length");

  soup_server_message_unpause (srv_msg);
}

static GtuberFlow
fetch_stream (GtuberProxy *self, SoupServerMessage *srv_msg,
    const char *org_uri, GError **error)
{
  GtuberProxyClass *proxy_class = GTUBER_PROXY_GET_CLASS (self);
  GtuberProxyPrivate *priv = gtuber_proxy_get_instance_private (self);
  SoupMessage *cli_msg = NULL;
  SoupMessageHeaders *srv_req_headers, *cli_req_headers, *srv_resp_headers;
  GCancellable *cancellable;
  GtuberFlow flow;

  srv_req_headers = soup_server_message_get_request_headers (srv_msg);

  /* Set user request headers determined by plugin */
  GTUBER_THREADED_OBJECT_LOCK (self);
  g_hash_table_foreach (priv->req_headers, (GHFunc) insert_header_cb, srv_req_headers);
  GTUBER_THREADED_OBJECT_UNLOCK (self);

forward:
  flow = proxy_class->forward_request (self, srv_msg, org_uri, &cli_msg, error);
    
  if (!cli_msg || *error != NULL) {
    flow = GTUBER_FLOW_ERROR;

    if (*error == NULL) {
/*
      g_set_error (&error, GTUBER_PROXY_ERROR,
          GTUBER_PROXY_ERROR_OTHER,
          "Proxy request message has not been created");
*/
    }
  }
  if (flow == GTUBER_FLOW_RESTART) {
    if (*error) {
      g_debug ("Retrying after error: %s", (*error)->message);
      g_error_free (*error);
    }
    goto forward;
  }

  cli_req_headers = soup_message_get_request_headers (cli_msg);

  /* Copy headers from proxy request to actual destination */
  soup_message_headers_foreach (srv_req_headers,
      (SoupMessageHeadersForeachFunc) insert_header_cb, cli_req_headers);

  /* Remove headers that we should not forward */
  soup_message_headers_remove (cli_req_headers, "Host");
  soup_message_headers_remove (cli_req_headers, "Connection");

  srv_resp_headers = soup_server_message_get_response_headers (srv_msg);
  soup_message_headers_set_encoding (srv_resp_headers, SOUP_ENCODING_CHUNKED);

  g_signal_connect (cli_msg, "got-headers", G_CALLBACK (forward_response_headers), srv_msg);

  cancellable = g_cancellable_new ();
  g_signal_connect (srv_msg, "finished", G_CALLBACK (finished_cb), cancellable);
  g_object_set_data_full (G_OBJECT (srv_msg), "cancellable", cancellable, g_object_unref);

  /* Proxy message while keeping a ref on server msg, so it will not be freed early */
  soup_session_send_async (priv->session, cli_msg, G_PRIORITY_DEFAULT, cancellable,
      (GAsyncReadyCallback) send_msg_async_cb, g_object_ref (srv_msg));
  g_object_unref (cli_msg);

  soup_server_message_pause (srv_msg);

  return GTUBER_FLOW_OK;
}

static void
server_cb (SoupServer *server, SoupServerMessage *srv_msg,
    const char *path, GHashTable *query, GtuberProxy *self)
{
  GtuberProxyPrivate *priv = gtuber_proxy_get_instance_private (self);
  GError *error = NULL;
  const gchar *itag_str, *org_uri = NULL;
  GtuberFlow flow = GTUBER_FLOW_ERROR;

  g_debug ("Handling proxy request");

  /* We only proxy HTTP GET methods */
  if (soup_server_message_get_method (srv_msg) != SOUP_METHOD_GET)
    goto finish;

  /* Query is needed for itag */
  if (!query)
    goto finish;

  GTUBER_THREADED_OBJECT_LOCK (self);

  /* Wrong media path */
  if (g_strcmp0 (path, priv->media_path)) {
    GTUBER_THREADED_OBJECT_UNLOCK (self);
    goto finish;
  }

  if ((itag_str = g_hash_table_lookup (query, "itag")))
    org_uri = g_hash_table_lookup (priv->org_uris, itag_str);

  GTUBER_THREADED_OBJECT_UNLOCK (self);

  if (org_uri)
    flow = fetch_stream (self, srv_msg, org_uri, &error);

finish:
  if (flow != GTUBER_FLOW_OK) {
    soup_server_message_set_status (srv_msg, SOUP_STATUS_NOT_FOUND,
        (error) ? error->message : NULL);
  }
  g_clear_error (&error);
}

static void
gtuber_proxy_thread_start (GtuberThreadedObject *threaded_object)
{
  GtuberProxy *self = GTUBER_PROXY (threaded_object);
  GtuberProxyPrivate *priv = gtuber_proxy_get_instance_private (self);
  GSList *uris;
  GError *soup_error = NULL;

  /* Create Soup server and session after thread push */
  priv->server = soup_server_new (NULL, NULL);
  priv->session = soup_session_new ();

  soup_server_add_handler (priv->server, "/", (SoupServerCallback) server_cb,
      self, NULL);
  soup_server_listen_local (priv->server, 0, SOUP_SERVER_LISTEN_IPV4_ONLY, &soup_error);

  if (soup_error) {
    g_error ("Could not start proxy server, reason: %s", soup_error->message);
    g_error_free (soup_error);

    return;
  }

  uris = soup_server_get_uris (priv->server);

  GTUBER_THREADED_OBJECT_LOCK (self);
  priv->proxy_uri = g_uri_to_string (uris->data);
  g_debug ("Listening on: %s", priv->proxy_uri);
  GTUBER_THREADED_OBJECT_UNLOCK (self);

  g_slist_free (uris);
}

static void
gtuber_proxy_thread_stop (GtuberThreadedObject *threaded_object)
{
  GtuberProxy *self = GTUBER_PROXY (threaded_object);
  GtuberProxyPrivate *priv = gtuber_proxy_get_instance_private (self);

  g_object_unref (priv->server);
  g_object_unref (priv->session);
}

static GtuberFlow
gtuber_proxy_forward_request (GtuberProxy *self, SoupServerMessage *srv_msg,
    const char *org_uri, SoupMessage **msg, GError **error)
{
  /* Ensure we do not leak here if subclass sets message and calls parent */
  if (*msg == NULL)
    *msg = soup_message_new (soup_server_message_get_method (srv_msg), org_uri);

  return GTUBER_FLOW_OK;
}

static void
gtuber_proxy_init (GtuberProxy *self)
{
  GtuberProxyPrivate *priv = gtuber_proxy_get_instance_private (self);

  priv->streams =
      g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
  priv->org_uris =
      g_hash_table_new_full ((GHashFunc) g_str_hash, (GEqualFunc) g_str_equal,
          (GDestroyNotify) g_free, (GDestroyNotify) g_free);
  priv->req_headers =
      g_hash_table_new_full ((GHashFunc) g_str_hash, (GEqualFunc) g_str_equal,
          (GDestroyNotify) g_free, (GDestroyNotify) g_free);
}

static void
gtuber_proxy_finalize (GObject *object)
{
  GtuberProxy *self = GTUBER_PROXY (object);
  GtuberProxyPrivate *priv = gtuber_proxy_get_instance_private (self);

  g_debug ("Proxy finalize");

  g_free (priv->proxy_uri);
  g_free (priv->media_path);

  g_ptr_array_unref (priv->streams);
  g_hash_table_unref (priv->req_headers);
  g_hash_table_unref (priv->org_uris);

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

  proxy_class->forward_request = gtuber_proxy_forward_request;
}

GtuberStream *
gtuber_proxy_get_requested_stream (GtuberProxy *self, SoupServerMessage *srv_msg)
{
  GUri *uri = soup_server_message_get_uri (srv_msg);
  GHashTable *parsed_query;
  GtuberStream *stream = NULL;
  const gchar *itag_str;

  parsed_query = g_uri_parse_params (g_uri_get_query (uri), -1, "&", G_URI_PARAMS_WWW_FORM, NULL);

  if (!parsed_query)
    return NULL;

  if ((itag_str = g_hash_table_lookup (parsed_query, "itag"))) {
    GtuberProxyPrivate *priv = gtuber_proxy_get_instance_private (self);
    guint i, itag = g_ascii_strtoull (itag_str, NULL, 10);

    GTUBER_THREADED_OBJECT_LOCK (self);

    for (i = 0; i < priv->streams->len; ++i) {
      GtuberStream *tmp_stream = (GtuberStream *) g_ptr_array_index (priv->streams, i);

      if (gtuber_stream_get_itag (tmp_stream) == itag) {
        stream = tmp_stream;
        break;
      }
    }

    GTUBER_THREADED_OBJECT_UNLOCK (self);
  }

  g_hash_table_unref (parsed_query);

  return stream;
}

void
gtuber_proxy_configure (GtuberProxy *self, const gchar *media_id,
    GPtrArray *streams, GPtrArray *adaptive_streams, GHashTable *req_headers)
{
  GtuberProxyPrivate *priv = gtuber_proxy_get_instance_private (self);

  GTUBER_THREADED_OBJECT_LOCK (self);

  priv->media_path = g_strdup_printf ("/gtuber/%s", media_id);
  g_hash_table_foreach (req_headers, (GHFunc) copy_header_cb_unlocked, priv);
  import_streams_unlocked (priv, streams);
  import_streams_unlocked (priv, adaptive_streams);

  GTUBER_THREADED_OBJECT_UNLOCK (self);
}
