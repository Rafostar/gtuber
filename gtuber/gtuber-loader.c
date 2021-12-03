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
#include "gtuber-cache-private.h"

typedef GtuberWebsite* (* PluginQuery) (GUri *uri);
typedef const gchar *const * (* PluginHosts) (void);
typedef const gchar *const * (* PluginSchemes) (void);

static const gchar *const default_schemes[] = {
  "http", "https", NULL
};
static const gchar *const no_hosts[] = {
  NULL
};

const gchar *
gtuber_loader_get_plugin_dir_path_string (void)
{
  const gchar *env_path;

  env_path = g_getenv ("GTUBER_PLUGIN_PATH");

  return (env_path && env_path[0])
      ? env_path
      : GTUBER_PLUGIN_PATH;
}

gchar **
gtuber_loader_obtain_plugin_dir_paths (void)
{
  gchar **strv;
  const gchar *path_str;

  path_str = gtuber_loader_get_plugin_dir_path_string ();
  strv = g_strsplit (path_str, G_SEARCHPATH_SEPARATOR_S, 0);

  return strv;
}

static GModule *
gtuber_loader_open_module (const gchar *module_path)
{
  GModule *module;

  g_debug ("Opening module: %s", module_path);
  module = g_module_open (module_path, G_MODULE_BIND_LAZY);

  if (module == NULL) {
    g_warning ("Could not load plugin: %s, reason: %s",
        module_path, g_module_error ());
    return NULL;
  }
  g_debug ("Opened plugin module: %s", module_path);

  /* Make sure module stays loaded. This will speed up using
   * it next time as we do not have to read its file again */
  g_module_make_resident (module);

  return module;
}

void
gtuber_loader_close_module (GModule *module)
{
  if (G_LIKELY (g_module_close (module)))
    g_debug ("Closed plugin module");
  else
    g_warning ("Could not close module");
}

gboolean
gtuber_loader_check_plugin_compat (const gchar *module_path,
    const gchar *const **schemes, const gchar *const **hosts)
{
  PluginSchemes plugin_get_schemes;
  PluginHosts plugin_get_hosts;
  GModule *module;

  module = gtuber_loader_open_module (module_path);
  if (!module)
    return FALSE;

  if (g_module_symbol (module, "plugin_get_schemes", (gpointer *) &plugin_get_schemes)
      && plugin_get_schemes != NULL) {
    *schemes = plugin_get_schemes ();
  }

  /* Schemes are required */
  if (*schemes == NULL || (*schemes)[0] == NULL)
    *schemes = default_schemes;

  if (g_module_symbol (module, "plugin_get_hosts", (gpointer *) &plugin_get_hosts)
      && plugin_get_hosts != NULL) {
    *hosts = plugin_get_hosts ();
  }

  /* Hosts may be empty in case of plugins
   * that use some unusual scheme */
  if (*hosts == NULL)
    *hosts = no_hosts;

  gtuber_loader_close_module (module);

  return TRUE;
}

static GtuberWebsite *
gtuber_loader_get_website_internal (const gchar *module_path,
    GUri *guri, GModule **module)
{
  PluginQuery plugin_query;
  GtuberWebsite *website = NULL;

  *module = gtuber_loader_open_module (module_path);
  if (*module == NULL)
    goto finish;

  if (!g_module_symbol (*module, "plugin_query", (gpointer *) &plugin_query)
      || plugin_query == NULL) {
    g_warning ("Query function missing in module");
    goto fail;
  }

  website = plugin_query (guri);
  if (website)
    goto finish;

fail:
  gtuber_loader_close_module (*module);

finish:
  return website;
}

gboolean
gtuber_loader_name_is_plugin (const gchar *module_name)
{
  return g_str_has_suffix (module_name, G_MODULE_SUFFIX);
}

GtuberWebsite *
gtuber_loader_get_website_for_uri (GUri *guri, GModule **module)
{
  GtuberWebsite *website = NULL;
  GPtrArray *compatible;
  guint i;

  /* FIXME: pass cancellable and error */
  gtuber_cache_init (NULL, NULL);

  /* FIXME: if debug is enabled */
  {
    gchar *uri;

    uri = g_uri_to_string (guri);
    g_debug ("Searching for plugin that opens URI: %s", uri);
    g_free (uri);
  }

  compatible = gtuber_cache_find_plugins_for_uri (guri);

  for (i = 0; i < compatible->len; i++) {
    const gchar *module_path;

    module_path = g_ptr_array_index (compatible, i);
    website = gtuber_loader_get_website_internal (module_path, guri, module);

    if (website) {
      g_debug ("Found compatible plugin: %s", module_path);
      break;
    }
  }

  g_ptr_array_unref (compatible);

  return website;
}
