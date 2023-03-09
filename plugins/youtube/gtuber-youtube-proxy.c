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

#include <gtuber/gtuber-plugin-devel.h>

#include "gtuber-youtube-proxy.h"

struct _GtuberYoutubeProxy
{
  GtuberProxy parent;
};

#define parent_class gtuber_youtube_proxy_parent_class
G_DEFINE_TYPE (GtuberYoutubeProxy, gtuber_youtube_proxy,
    GTUBER_TYPE_PROXY)

static void
gtuber_youtube_proxy_init (GtuberYoutubeProxy *self)
{
}

static GtuberFlow
gtuber_youtube_proxy_forward_request (GtuberProxy *proxy, SoupServerMessage *srv_msg,
    const char *org_uri, SoupMessage **msg, GError **error)
{
  SoupMessageHeaders *headers = soup_server_message_get_request_headers (srv_msg);
  SoupRange *ranges = NULL;
  gint length = 0;

  /* Append "range" param to URI */
  if (soup_message_headers_get_ranges (headers, 0, &ranges, &length)) {
    gchar *mod_uri, *tmp;

    tmp = g_strdup_printf ("/range/%li-%li", ranges->start, ranges->end);
    mod_uri = g_build_path ("/", org_uri, tmp, NULL);

    g_free (tmp);

    //mod_uri = g_strdup_printf ("%s/range/%li-%li", org_uri, ranges->start, ranges->end);
    soup_message_headers_free_ranges (headers, ranges);

    //soup_message_headers_remove (headers, "Range");

    g_debug ("Redirect to: %s", mod_uri);

    *msg = soup_message_new (soup_server_message_get_method (srv_msg), mod_uri);
    g_free (mod_uri);
  } else {
    *msg = soup_message_new (soup_server_message_get_method (srv_msg), org_uri);
    g_debug ("Redirect to: %s", org_uri);
  }

  return GTUBER_FLOW_OK;

//fail:
  //g_set_error (error, GTUBER_WEBSITE_ERROR,
  //    GTUBER_HEARTBEAT_ERROR_PING_FAILED,
  //    "%s", err_msg);
  //return GTUBER_FLOW_ERROR;
}

static void
gtuber_youtube_proxy_class_init (GtuberYoutubeProxyClass *klass)
{
  GtuberProxyClass *proxy_class = (GtuberProxyClass *) klass;

  proxy_class->forward_request = gtuber_youtube_proxy_forward_request;
}

GtuberProxy *
gtuber_youtube_proxy_new (void)
{
  return g_object_new (GTUBER_TYPE_YOUTUBE_PROXY, NULL);
}
