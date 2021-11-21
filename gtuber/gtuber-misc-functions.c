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

#include <gmodule.h>

#include "gtuber-misc-functions.h"
#include "gtuber-loader-private.h"
#include "gtuber-website.h"

/**
 * gtuber_has_plugin_for_uri:
 * @uri: a media source URI
 * @plugin_name: (out) (optional) (transfer full): return location
 *   for plugin name that handles given URI, or %NULL
 *
 * Checks if any among installed plugins advertises support for given URI.
 *
 * You can use this to check plugin support without actually trying to download
 * any data, otherwise just use %GtuberClient to fetch media info directly.
 *
 * Returns: %TRUE when URI is supported with out parameters set, %FALSE otherwise.
 */
gboolean
gtuber_has_plugin_for_uri (const gchar *uri, gchar **plugin_name)
{
  GtuberWebsite *website;
  GUri *guri;
  GModule *module = NULL;
  gboolean res = FALSE;

  g_return_val_if_fail (uri != NULL, FALSE);

  g_debug ("Checking URI support: %s", uri);

  guri = g_uri_parse (uri, G_URI_FLAGS_ENCODED, NULL);
  if (!guri) {
    g_debug ("URI is invalid");
    return FALSE;
  }

  website = gtuber_loader_get_website_for_uri (guri, &module);
  if (website) {
    g_object_unref (website);

    if (plugin_name)
      *plugin_name = g_strdup (g_module_name (module));

    g_module_close (module);
    res = TRUE;
  }
  g_debug ("URI supported: %s", res ? "yes" : "no");

  return res;
}
