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

/**
 * SECTION:gtuber-misc-functions
 * @title: Gtuber
 * @short_description: misc functions
 */

#include <gmodule.h>

#include "gtuber-misc-functions.h"
#include "gtuber-loader-private.h"
#include "gtuber-cache-private.h"
#include "gtuber-website.h"

/**
 * gtuber_has_plugin_for_uri:
 * @uri: a media source URI
 * @filename: (out) (optional) (transfer full): return location
 *   for plugin filename that handles given URI, or %NULL
 *
 * Checks if any among installed plugins advertises support for given URI.
 *
 * You can use this to check plugin support without actually trying to download
 * any data, otherwise just use %GtuberClient to fetch media info directly.
 *
 * Returns: %TRUE when URI is supported with out parameters set, %FALSE otherwise.
 */
gboolean
gtuber_has_plugin_for_uri (const gchar *uri, gchar **filename)
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

    if (filename)
      *filename = g_strdup (g_module_name (module));

    gtuber_loader_close_module (module);
    res = TRUE;
  }
  g_debug ("URI supported: %s", res ? "yes" : "no");

  return res;
}

/**
 * gtuber_get_supported_schemes:
 *
 * Get the list of all supported URI schemes by currently available plugins.
 *
 * Returns: (transfer none): Supported URI schemes.
 */
const gchar *const *
gtuber_get_supported_schemes (void)
{
  return gtuber_cache_get_supported_schemes ();
}
