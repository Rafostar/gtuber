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

#include <glib-object.h>
#include <gmodule.h>
#include <gio/gio.h>
#include <libsoup/soup.h>

#include "gtuber-client.h"
#include "gtuber-media-info.h"
#include "gtuber-loader-private.h"
#include "gtuber-website.h"

struct _GtuberClient
{
  GObject parent;

  gchar *user_agent;
  gchar *browser_version;

  /* Soup data */
  SoupSession *session;

  /* Private */
  gchar *module_name;
};

struct _GtuberClientClass
{
  GObjectClass parent_class;
};

#define parent_class gtuber_client_parent_class
G_DEFINE_TYPE (GtuberClient, gtuber_client, G_TYPE_OBJECT)
G_DEFINE_QUARK (gtuberclient-error-quark, gtuber_client_error)

static void gtuber_client_finalize (GObject *object);

static void
gtuber_client_init (GtuberClient *self)
{
  self = gtuber_client_get_instance_private (self);

  /* TODO: Make it always up-to-date */
  self->browser_version = g_strdup ("86.0");
  self->user_agent = g_strdup_printf (
      "Mozilla/5.0 (X11; Linux x86_64; rv:%s) Gecko/20100101 Firefox/%s",
      self->browser_version, self->browser_version);

  self->session = soup_session_new_with_options (
      "timeout", 7,
      "max_conns_per_host", 1,
      "user_agent", self->user_agent,
      NULL);

  self->module_name = NULL;
}

static void
gtuber_client_class_init (GtuberClientClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->finalize = gtuber_client_finalize;
}

static void
gtuber_client_finalize (GObject *object)
{
  GtuberClient *self = GTUBER_CLIENT (object);

  g_debug ("Client finalize");

  g_free (self->browser_version);
  g_free (self->user_agent);

  g_object_unref (self->session);

  g_free (self->module_name);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static GUri *
uri_to_guri (const gchar *uri, GError **error)
{
  GUri *guri;
  GUriFlags flags;
  GError *my_error = NULL;

  flags = G_URI_FLAGS_NON_DNS |
      G_URI_FLAGS_ENCODED_QUERY |
      G_URI_FLAGS_ENCODED_PATH;

  guri = g_uri_parse (uri, flags, &my_error);

  if (my_error)
    g_propagate_error (error, my_error);

  return guri;
}

static void
gtuber_client_configure_website (GtuberClient *self,
    GtuberWebsite *website, const gchar *uri)
{
  /* Plugin may set custom website config during init,
   * otherwise set it with current values */
  if (!gtuber_website_get_uri (website))
    gtuber_website_set_uri (website, uri);
  if (!gtuber_website_get_user_agent (website) ||
      !gtuber_website_get_browser_version (website)) {
    gtuber_website_set_browser (website, self->user_agent, self->browser_version);
  }
}

static void
gtuber_client_verify_media_info (GtuberClient *self,
    GtuberMediaInfo *info, GError **error)
{
  const GPtrArray *streams, *adaptive_streams;

  streams = gtuber_media_info_get_streams (info);
  if (streams && streams->len)
    return;

  adaptive_streams = gtuber_media_info_get_adaptive_streams (info);
  if (adaptive_streams && adaptive_streams->len)
    return;

  g_debug ("No streams in media info");
  g_set_error (error, GTUBER_CLIENT_ERROR, GTUBER_CLIENT_ERROR_MISSING_INFO,
      "Plugin returned media info without any streams");
}

/**
 * gtuber_client_new:
 *
 * Creates a new #GtuberClient instance.
 *
 * Returns: (transfer full): a new #GtuberClient instance.
 */
GtuberClient *
gtuber_client_new (void)
{
  return g_object_new (GTUBER_TYPE_CLIENT, NULL);
}

/**
 * gtuber_client_fetch_media_info:
 * @client: a #GtuberClient
 * @uri: a media source URI
 * @cancellable: (nullable): optional #GCancellable object,
 *     %NULL to ignore
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Synchronously obtains media info for requested URI.
 *
 * Returns: (transfer full): a #GtuberMediaInfo or %NULL on error.
 */
GtuberMediaInfo *
gtuber_client_fetch_media_info (GtuberClient *self, const gchar *uri,
    GCancellable *cancellable, GError **error)
{
  GtuberMediaInfo *info = NULL;
  GtuberWebsite *website = NULL;
  GtuberWebsiteClass *website_class;
  GtuberFlow flow = GTUBER_FLOW_ERROR;

  GUri *guri = NULL;
  GModule *module = NULL;
  SoupMessage *msg = NULL;
  GError *my_error = NULL;

  g_return_val_if_fail (GTUBER_IS_CLIENT (self), NULL);

  g_debug ("Requested URI: %s", uri);

  guri = uri_to_guri (uri, &my_error);
  if (!guri) {
    gchar *yt_uri;

    if (strlen (uri) != 11)
      goto error;

    if (my_error) {
      g_error_free (my_error);
      my_error = NULL;
    }

    /* Exclusively assume YT video ID for non-uri 11 characters */
    yt_uri = g_strjoin (NULL, "https://www.youtube.com/watch?v=", uri, NULL);
    guri = uri_to_guri (yt_uri, &my_error);
    g_free (yt_uri);

    if (!guri)
      goto error;
  }

  if (self->module_name) {
    g_debug ("Trying to use last module again");
    website = gtuber_loader_get_website_from_module_name (self->module_name, guri, &module);
  }

  if (!website)
    website = gtuber_loader_get_website_for_uri (guri, &module);

  if (!website) {
    g_debug ("No plugin for URI: %s", uri);
    g_set_error (error, GTUBER_CLIENT_ERROR, GTUBER_CLIENT_ERROR_NO_PLUGIN,
        "None of the installed plugins could handle URI: %s", uri);

    return NULL;
  }

  gtuber_client_configure_website (self, website, uri);

  website_class = GTUBER_WEBSITE_GET_CLASS (website);
  info = g_object_new (GTUBER_TYPE_MEDIA_INFO, NULL);

beginning:
  flow = website_class->create_request (website, info, &msg, &my_error);
  if (flow != GTUBER_FLOW_OK)
    goto decide_flow;

  g_debug ("Request created");

  g_debug ("Plugin handles input stream: %s",
      website_class->handles_input_stream ? "yes" : "no");

  if (website_class->handles_input_stream) {
    GInputStream *stream;

    g_debug ("Sending request...");
    stream = soup_session_send (self->session, msg, cancellable, &my_error);
    g_debug ("Request send");

    flow = (my_error != NULL) ? GTUBER_FLOW_ERROR :
        website_class->parse_input_stream (website, stream, info, &my_error);

    if (stream) {
      if (g_input_stream_close (stream, NULL, NULL))
        g_debug ("Input stream closed");
      else
        g_warning ("Input stream could not be closed");
    }
    if (flow != GTUBER_FLOW_OK)
      goto decide_flow;
  } else {
    g_debug ("Sending request...");
    soup_session_send_message (self->session, msg);
    g_debug ("Request send");

    flow = website_class->parse_response (website, msg, info, &my_error);
    if (flow != GTUBER_FLOW_OK)
      goto decide_flow;
  }

  g_debug ("Parsed response");

error:
  if (guri)
    g_uri_unref (guri);
  if (msg)
    g_object_unref (msg);
  if (website)
    g_object_unref (website);
  if (module) {
    g_free (self->module_name);
    self->module_name = g_strdup (g_module_name (module));

    g_module_close (module);
    g_debug ("Closed plugin module");
  }

invalid_info:
  if (my_error) {
    g_propagate_error (error, my_error);

    if (info)
      g_object_unref (info);

    return NULL;
  }

  if (flow == GTUBER_FLOW_OK) {
    gtuber_client_verify_media_info (self, info, &my_error);
    if (my_error)
      goto invalid_info;
  }

  return info;

decide_flow:
  switch (flow) {
    case GTUBER_FLOW_RESTART:
      if (msg) {
        g_object_unref (msg);
        msg = NULL;
      }
      goto beginning;
    case GTUBER_FLOW_ERROR:
      if (!my_error) {
        g_set_error (&my_error, GTUBER_WEBSITE_ERROR,
            GTUBER_WEBSITE_ERROR_OTHER,
            "Plugin encountered an error");
      }
      goto error;
    default:
      g_assert_not_reached ();
      break;
  }
}

static void
fetch_media_info_async_thread (GTask *task, gpointer source, gpointer task_data,
    GCancellable *cancellable)
{
  GtuberClient *self = source;
  gchar *uri = task_data;
  GtuberMediaInfo *media_info;
  GError *error = NULL;

  media_info = gtuber_client_fetch_media_info (self, uri, cancellable, &error);

  if (media_info)
    g_task_return_pointer (task, media_info, g_object_unref);
  else
    g_task_return_error (task, error);
}

/**
 * gtuber_client_fetch_media_info_async:
 * @client: a #GtuberClient
 * @uri: a media source URI
 * @cancellable: (nullable): optional #GCancellable object,
 *     %NULL to ignore
 * @callback: (scope async): a #GAsyncReadyCallback to call
 *     when the request is satisfied
 * @user_data: (closure): the data to pass to callback function
 *
 * Asynchronously obtains media info for requested URI.
 *
 * When the operation is finished, @callback will be called.
 * You can then call gtuber_client_fetch_media_info_finish() to
 * get the result of the operation.
 */
void
gtuber_client_fetch_media_info_async (GtuberClient *self, const gchar *uri,
    GCancellable *cancellable, GAsyncReadyCallback callback, gpointer user_data)
{
  GTask *task;

  g_return_if_fail (GTUBER_IS_CLIENT (self));

  task = g_task_new (self, cancellable, callback, user_data);
  g_task_set_task_data (task, g_strdup (uri), (GDestroyNotify) g_free);
  g_task_run_in_thread (task, fetch_media_info_async_thread);

  g_object_unref (task);
}

/**
 * gtuber_client_fetch_media_info_finish:
 * @client: a #GtuberClient
 * @res: a #GAsyncResult
 * @error: (nullable): return location for a #GError, or %NULL
 *
 * Finishes an asynchronous obtain media info operation started with
 * gtuber_client_fetch_media_info_async().
 *
 * Returns: (transfer full): a #GtuberMediaInfo or %NULL on error.
 */
GtuberMediaInfo *
gtuber_client_fetch_media_info_finish (GtuberClient *self, GAsyncResult *res,
    GError **error)
{
  g_return_val_if_fail (GTUBER_IS_CLIENT (self), NULL);
  g_return_val_if_fail (G_IS_ASYNC_RESULT (res), NULL);

  return g_task_propagate_pointer (G_TASK (res), error);
}
