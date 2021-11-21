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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <gtuber/gtuber.h>

#include "gstgtuberelement.h"

GST_DEBUG_CATEGORY_STATIC (gst_gtuber_debug);
#define GST_CAT_DEFAULT gst_gtuber_debug

static const gchar *hosts_blacklist[] = {
  "googlevideo.com",
  NULL
};

static GstURIType
gst_gtuber_uri_handler_get_type_src (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
gst_gtuber_uri_handler_get_protocols (GType type)
{
  static const gchar *protocols[] = {
    "https", "gtuber", NULL
  };

  return protocols;
}

static gchar *
gst_gtuber_uri_handler_get_uri (GstURIHandler *handler)
{
  GstElement *element = GST_ELEMENT (handler);
  gchar *uri;

  g_return_val_if_fail (GST_IS_ELEMENT (element), NULL);

  g_object_get (G_OBJECT (element), "location", &uri, NULL);

  return uri;
}

static gboolean
gst_gtuber_uri_handler_set_uri (GstURIHandler *handler, const gchar *uri, GError **error)
{
  GstElement *element = GST_ELEMENT (handler);
  GstUri *gst_uri;
  const gchar *host;
  gboolean blacklisted = FALSE;
  guint index = 0;

  g_return_val_if_fail (GST_IS_ELEMENT (element), FALSE);

  gst_uri = gst_uri_from_string (uri);
  if (!gst_uri) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        "URI could not be parsed");
    return FALSE;
  }

  /* This is faster for URIs that we know are unsupported instead
   * of letting gtuber query every available plugin. We need this
   * to quickly fallback to lower ranked httpsrc plugins downstream */
  host = gst_uri_get_host (gst_uri);
  while (hosts_blacklist[index]) {
    if ((blacklisted = g_str_has_suffix (host, hosts_blacklist[index])))
      break;

    index++;
  }
  gst_uri_unref (gst_uri);

  if (blacklisted) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        "This URI is not meant for Gtuber");
    return FALSE;
  }

  if (!gtuber_has_plugin_for_uri (uri, NULL)) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        "Gtuber does not have a plugin for this URI");
    return FALSE;
  }
  if (GST_STATE (element) == GST_STATE_PLAYING ||
      GST_STATE (element) == GST_STATE_PAUSED) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_STATE,
        "Changing the 'location' property while the element is running is "
        "not supported");
    return FALSE;
  }

  g_object_set (G_OBJECT (element), "location", uri, NULL);

  return TRUE;
}

static void
gst_gtuber_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_gtuber_uri_handler_get_type_src;
  iface->get_protocols = gst_gtuber_uri_handler_get_protocols;
  iface->get_uri = gst_gtuber_uri_handler_get_uri;
  iface->set_uri = gst_gtuber_uri_handler_set_uri;
}

void
gst_gtuber_uri_handler_do_init (GType type)
{
  GInterfaceInfo uri_handler_info = {
    gst_gtuber_uri_handler_init,
    NULL,
    NULL
  };

  g_type_add_interface_static (type, GST_TYPE_URI_HANDLER, &uri_handler_info);
}

void
gst_gtuber_element_init (GstPlugin *plugin)
{
  static gsize res = FALSE;

  if (g_once_init_enter (&res)) {
    GST_DEBUG_CATEGORY_INIT (gst_gtuber_debug, "gtuber", 0, "Gtuber elements");
    g_once_init_leave (&res, TRUE);
  }
}
