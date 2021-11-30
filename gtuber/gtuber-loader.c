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

#include "config.h"
#include "gtuber-loader-private.h"

typedef GtuberWebsite* (* PluginQuery) (GUri *uri);

static gchar *
gtuber_loader_get_plugin_dir_path (void)
{
  const gchar *env_path;
  gchar *parsed_path;

  env_path = g_getenv ("GTUBER_PLUGIN_PATH");
  parsed_path = g_filename_display_name (
      (env_path && env_path[0]) ? env_path : GTUBER_PLUGIN_PATH);
  g_debug ("Plugin dir path: %s", parsed_path);

  return parsed_path;
}

static GtuberWebsite *
gtuber_loader_get_website_internal (gchar *module_path,
    GUri *guri, GModule **module)
{
  PluginQuery plugin_query;
  GtuberWebsite *website = NULL;

  g_debug ("Opening module: %s", module_path);
  *module = g_module_open (module_path, G_MODULE_BIND_LAZY);

  if (*module == NULL) {
    g_warning ("Could not load plugin: %s, reason: %s",
        module_path, g_module_error ());
    goto finish;
  }
  g_debug ("Opened plugin module: %s", module_path);

  if (!g_module_symbol (*module, "plugin_query", (gpointer *) &plugin_query)
      || plugin_query == NULL) {
    g_warning ("Query function missing in module");
    goto fail;
  }

  /* Make sure module stays loaded. This will speed up using
   * it next time as we do not have to read its file again */
  g_module_make_resident (*module);

  website = plugin_query (guri);
  if (website)
    goto finish;

fail:
  if (g_module_close (*module))
    g_debug ("Closed plugin module");
  else
    g_warning ("Could not close module: %s", module_path);

finish:
  g_free (module_path);
  return website;
}

GtuberWebsite *
gtuber_loader_get_website_for_uri (GUri *guri, GModule **module)
{
  GDir *dir;
  GModule *my_module;
  GtuberWebsite *website = NULL;
  gchar *dir_path, *uri;
  const gchar *module_name;

  if (!g_module_supported ()) {
    g_warning ("No module loading support on current platform");
    return NULL;
  }

  dir_path = gtuber_loader_get_plugin_dir_path ();
  dir = g_dir_open (dir_path, 0, NULL);
  if (!dir) {
    g_debug ("Could not open plugin dir: %s", dir_path);
    goto no_dir;
  }

  uri = g_uri_to_string (guri);
  g_debug ("Searching for plugin that opens URI: %s", uri);
  g_free (uri);

  while ((module_name = g_dir_read_name (dir))) {
    gchar *module_path;

    if (!g_str_has_suffix (module_name, G_MODULE_SUFFIX))
      continue;

    module_path = g_module_build_path (dir_path, module_name);
    website = gtuber_loader_get_website_internal (module_path, guri, &my_module);
    if (website) {
      g_debug ("Found compatible plugin: %s", module_name);
      *module = my_module;
      break;
    }
  }

  if (dir) {
    g_dir_close (dir);
    g_debug ("Plugins dir closed");
  }

no_dir:
  g_free (dir_path);

  return website;
}

GtuberWebsite *
gtuber_loader_get_website_from_module_name (const gchar *module_name,
    GUri *guri, GModule **module)
{
  GtuberWebsite *website;
  GModule *my_module;
  gchar *module_path;

  g_debug ("Loading plugin from module name: %s", module_name);

  if (g_path_is_absolute (module_name))
    module_path = g_strdup (module_name);
  else {
    gchar *dir_path;

    dir_path = gtuber_loader_get_plugin_dir_path ();
    module_path = g_module_build_path (dir_path, module_name);
    g_free (dir_path);
  }

  website = gtuber_loader_get_website_internal (module_path, guri, &my_module);
  if (website) {
    g_debug ("Plugin loaded successfully");

    if (module)
      *module = my_module;
  }

  return website;
}
