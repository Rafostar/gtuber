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
 * SECTION:gtuber-cache
 * @title: Gtuber Cache
 * @short_description: a cache for storing some values locally
 */

#include "config.h"

#include <stdio.h>

#include "gtuber-cache.h"
#include "gtuber-cache-private.h"
#include "gtuber-loader-private.h"
#include "gtuber-version.h"

#define GTUBER_CACHE_BASENAME "gtuber_cache.bin"

/* CACHE CONTENTS:
 * GtuberCacheHeader;
 *
 * multiple dir data
 */

/* DIR DATA:
 * guint len;
 * gchar *dir_path;
 *
 * gint64 mod_time;
 * guint n_plugins;
 *
 * multiple plugins data
 */

/* PLUGIN DATA:
 * guint len;
 * gchar *module_name;
 *
 * guint n_strings;
 * guint len;
 * gchar *string;
 * ...
 */

/* PLUGIN CACHE CONTENTS:
 * GtuberCacheHeader;
 *
 * guint key_len;
 * gchar *key;
 * guint val_len;
 * gchar *val;
 */

typedef struct
{
  gchar name[7];
  guint version_hex;
} GtuberCacheHeader;

typedef struct
{
  gchar *module_name;
  GPtrArray *schemes;
  GPtrArray *hosts;
} GtuberCachePluginCompatData;

typedef struct
{
  gchar *dir_path;
  GPtrArray *plugins;
} GtuberCachePluginDirData;

/* Plugins cache data and mutex protecting it */
static GMutex cache_lock;
static GPtrArray *plugins_cache = NULL;

static GtuberCachePluginCompatData *
gtuber_cache_plugin_compat_data_new_take (gchar *module_name)
{
  GtuberCachePluginCompatData *data;

  data = g_new (GtuberCachePluginCompatData, 1);
  data->module_name = module_name;
  data->schemes = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);
  data->hosts = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);

  return data;
}

static GtuberCachePluginCompatData *
gtuber_cache_plugin_compat_data_new (const gchar *module_name)
{
  return gtuber_cache_plugin_compat_data_new_take (g_strdup (module_name));
}

static void
gtuber_cache_plugin_compat_data_free (GtuberCachePluginCompatData *data)
{
  g_free (data->module_name);
  g_ptr_array_unref (data->schemes);
  g_ptr_array_unref (data->hosts);

  g_free (data);
}

static GtuberCachePluginDirData *
gtuber_cache_plugin_dir_data_new_take (gchar *dir_path)
{
  GtuberCachePluginDirData *data;

  data = g_new (GtuberCachePluginDirData, 1);
  data->dir_path = dir_path;
  data->plugins = g_ptr_array_new_with_free_func (
      (GDestroyNotify) gtuber_cache_plugin_compat_data_free);

  return data;
}

static GtuberCachePluginDirData *
gtuber_cache_plugin_dir_data_new (const gchar *dir_path)
{
  return gtuber_cache_plugin_dir_data_new_take (g_strdup (dir_path));
}

static void
gtuber_cache_plugin_dir_data_free (GtuberCachePluginDirData *data)
{
  g_free (data->dir_path);
  g_ptr_array_unref (data->plugins);

  g_free (data);
}

static void
gtuber_cache_take_dir_data (GtuberCachePluginDirData *data)
{
  g_ptr_array_add (plugins_cache, data);
}

static void
gtuber_cache_clear (void)
{
  while (plugins_cache->len > 0)
    g_ptr_array_remove_index (plugins_cache, plugins_cache->len - 1);
}

static void
gtuber_cache_plugin_dir_data_take_plugin_data (GtuberCachePluginDirData *data,
    GtuberCachePluginCompatData *plugin_data)
{
  g_ptr_array_add (data->plugins, plugin_data);
}

static void
gtuber_cache_plugin_compat_data_add_scheme (GtuberCachePluginCompatData *data,
    const gchar *scheme)
{
  g_ptr_array_add (data->schemes, g_strdup (scheme));
}

static void
gtuber_cache_plugin_compat_data_take_scheme (GtuberCachePluginCompatData *data,
    gchar *scheme)
{
  g_ptr_array_add (data->schemes, scheme);
}

static void
gtuber_cache_plugin_compat_data_add_host (GtuberCachePluginCompatData *data,
    const gchar *host)
{
  g_ptr_array_add (data->hosts, g_strdup (host));
}

static void
gtuber_cache_plugin_compat_data_take_host (GtuberCachePluginCompatData *data,
    gchar *host)
{
  g_ptr_array_add (data->hosts, host);
}

static gchar *
gtuber_cache_obtain_cache_path (const gchar *basename)
{
  return g_build_filename (g_get_user_cache_dir (),
      GTUBER_API_NAME, basename, NULL);
}

static gboolean
write_ptr_to_file (FILE *file, gconstpointer ptr, gsize size)
{
  return fwrite (ptr, size, 1, file) == 1;
}

static gboolean
read_file_to_ptr (FILE *file, gpointer ptr, gsize size)
{
  return fread (ptr, size, 1, file) == 1;
}

static void
write_string (FILE *file, const gchar *str)
{
  guint len = strlen (str) + 1;

  write_ptr_to_file (file, &len, sizeof (guint));
  write_ptr_to_file (file, str, len);
}

static gchar *
read_next_string (FILE *file)
{
  guint len;
  gchar *str = NULL;

  read_file_to_ptr (file, &len, sizeof (guint));
  if (len > 0) {
    str = g_new (gchar, len);
    read_file_to_ptr (file, str, len);
  }

  return str;
}

static gboolean
write_n_elems (FILE *file, const gchar *const *arr)
{
  guint n_elems = 0;

  while (arr && arr[n_elems])
    n_elems++;

  write_ptr_to_file (file, &n_elems, sizeof (guint));

  return n_elems > 0;
}

static gint64
gtuber_cache_get_plugin_mod_time (GFileInfo *info)
{
  GDateTime *date_time;
  gint64 unix_time = 0;

  date_time = g_file_info_get_modification_date_time (info);
  if (date_time) {
    unix_time = g_date_time_to_unix (date_time);
    g_date_time_unref (date_time);
  }

  return unix_time;
}

static gboolean
gtuber_cache_prepare (GCancellable *cancellable, GError **error)
{
  GFile *db_dir;
  gchar *db_path;
  gboolean res = FALSE;

  db_path = gtuber_cache_obtain_cache_path (NULL);
  db_dir = g_file_new_for_path (db_path);
  g_free (db_path);

  if (!g_file_query_exists (db_dir, cancellable)) {
    if (g_cancellable_is_cancelled (cancellable)) {
      g_set_error (error, G_IO_ERROR,
          G_IO_ERROR_CANCELLED,
          "Operation was cancelled");
      goto finish;
    }
    if (!g_file_make_directory (db_dir, cancellable, error))
      goto finish;
  }
  res = TRUE;

finish:
  g_object_unref (db_dir);

  return res;
}

static FILE *
gtuber_cache_open_write (const gchar *basename)
{
  FILE *file = NULL;
  GtuberCacheHeader *header;
  gchar *filepath;

  filepath = gtuber_cache_obtain_cache_path (basename);
  g_debug ("Opening cache file for writing: %s", filepath);

  file = fopen (filepath, "wb");
  if (!file) {
    g_warning ("Could not open file: %s", filepath);
    goto finish;
  }

  header = g_new (GtuberCacheHeader, 1);
  g_strlcpy (header->name, "GTUBER", 7);
  header->version_hex = GTUBER_VERSION_HEX;

  write_ptr_to_file (file, header, sizeof (GtuberCacheHeader));
  g_free (header);

finish:
  g_free (filepath);

  return file;
}

static FILE *
gtuber_cache_open_read (const gchar *basename)
{
  FILE *file;
  GtuberCacheHeader *header;
  gchar *filepath;
  gboolean success = FALSE;

  filepath = gtuber_cache_obtain_cache_path (basename);
  g_debug ("Opening cache file for reading: %s", filepath);

  file = fopen (filepath, "rb");
  g_free (filepath);

  if (!file) {
    g_debug ("Could not open file: %s", basename);
    return NULL;
  }

  header = g_new (GtuberCacheHeader, 1);
  read_file_to_ptr (file, header, sizeof (GtuberCacheHeader));

  g_debug ("Cache header, name: %s, version_hex: %u",
      header->name, header->version_hex);

  if (g_strcmp0 (header->name, "GTUBER")
      || header->version_hex != GTUBER_VERSION_HEX) {
    g_debug ("Cache header mismatch");
    goto finish;
  }

  g_debug ("Cache opened successfully");
  success = TRUE;

finish:
  g_free (header);

  if (success)
    return file;

  fclose (file);
  return NULL;
}

static void
gtuber_cache_enumerate_plugins (GFile *dir, GPtrArray *modules,
    gint64 *mod_time, guint *n_plugins, GCancellable *cancellable,
    GError **error)
{
  GFileEnumerator *dir_enum;

  dir_enum = g_file_enumerate_children (dir,
      G_FILE_ATTRIBUTE_STANDARD_NAME ","
      G_FILE_ATTRIBUTE_TIME_MODIFIED,
      G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS, cancellable, error);
  if (!dir_enum)
    return;

  while (TRUE) {
    GFileInfo *info = NULL;
    gint64 plugin_mod_time;
    const gchar *module_name;

    if (!g_file_enumerator_iterate (dir_enum, &info,
        NULL, cancellable, error) || !info)
      break;

    module_name = g_file_info_get_name (info);

    if (!gtuber_loader_name_is_plugin (module_name))
      continue;

    if (modules)
      g_ptr_array_add (modules, g_strdup (module_name));
    if (n_plugins)
      *n_plugins += 1;

    plugin_mod_time = gtuber_cache_get_plugin_mod_time (info);

    if (mod_time && *mod_time < plugin_mod_time)
      *mod_time = plugin_mod_time;
  }
  g_object_unref (dir_enum);
}

static gboolean
gtuber_cache_read_plugins_compat (FILE *file,
    const gchar *dir_path, GCancellable *cancellable, GError **error)
{
  GFile *dir;
  gint64 cache_mod_time, latest_time = 0;
  guint cache_n_plugins, n_plugins = 0;
  gchar *cache_dir_path = NULL;
  gboolean changed;

  cache_dir_path = read_next_string (file);
  g_debug ("Read cache dir path: %s", cache_dir_path);

  /* Make sure the order in GTUBER_PLUGIN_PATH have not changed */
  changed = g_strcmp0 (dir_path, cache_dir_path) != 0;

  if (changed) {
    g_debug ("Plugin path has changed");
    goto fail;
  }

  dir = g_file_new_for_path (dir_path);
  if (!dir) {
    g_warning ("Could not parse dir path: %s", dir_path);
    goto fail;
  }

  read_file_to_ptr (file, &cache_mod_time, sizeof (gint64));
  read_file_to_ptr (file, &cache_n_plugins, sizeof (guint));

  gtuber_cache_enumerate_plugins (dir, NULL, &latest_time, &n_plugins,
      cancellable, error);
  g_object_unref (dir);

  changed = (cache_mod_time != latest_time
      || cache_n_plugins != n_plugins);

  g_debug ("Cache compared, mod_time: %li %s %li, "
      "n_plugins: %u %s %u",
      cache_mod_time, (cache_mod_time != latest_time) ? "!=" : "==", latest_time,
      cache_n_plugins, (cache_n_plugins != n_plugins) ? "!=" : "==", n_plugins);

  if (changed) {
    g_debug ("Plugins have changed");
  } else {
    GtuberCachePluginDirData *dir_data;
    guint i;

    g_debug ("Reading plugins compat data for dir: %s", cache_dir_path);
    dir_data = gtuber_cache_plugin_dir_data_new_take (cache_dir_path);

    for (i = 0; i < cache_n_plugins; i++) {
      GtuberCachePluginCompatData *data;
      gchar *module_name;
      guint n_schemes, n_hosts, j;

      module_name = read_next_string (file);
      data = gtuber_cache_plugin_compat_data_new_take (module_name);

      read_file_to_ptr (file, &n_schemes, sizeof (guint));
      for (j = 0; j < n_schemes; j++) {
        gchar *scheme;

        scheme = read_next_string (file);
        gtuber_cache_plugin_compat_data_take_scheme (data, scheme);
      }

      read_file_to_ptr (file, &n_hosts, sizeof (guint));
      for (j = 0; j < n_hosts; j++) {
        gchar *host;

        host = read_next_string (file);
        gtuber_cache_plugin_compat_data_take_host (data, host);
      }

      gtuber_cache_plugin_dir_data_take_plugin_data (dir_data, data);
    }

    gtuber_cache_take_dir_data (dir_data);
    g_debug ("Read compat data for %u plugins", n_plugins);

    return TRUE;
  }

fail:
  g_free (cache_dir_path);

  return FALSE;
}

static gboolean
gtuber_cache_write_plugin_compat (FILE *file,
    const gchar *dir_path, GCancellable *cancellable, GError **error)
{
  GFile *dir;
  GPtrArray *module_names;
  GtuberCachePluginDirData *dir_data;
  gint64 mod_time = 0;
  guint i, n_plugins = 0;

  dir = g_file_new_for_path (dir_path);
  if (!dir) {
    g_warning ("Malformed path in \"GTUBER_PLUGIN_PATH\" env: %s", dir_path);
    goto fail;
  }

  write_string (file, dir_path);

  module_names = g_ptr_array_new_with_free_func ((GDestroyNotify) g_free);
  gtuber_cache_enumerate_plugins (dir, module_names, &mod_time, &n_plugins,
      cancellable, error);

  write_ptr_to_file (file, &mod_time, sizeof (gint64));
  write_ptr_to_file (file, &n_plugins, sizeof (guint));

  dir_data = gtuber_cache_plugin_dir_data_new (dir_path);

  for (i = 0; i < module_names->len; i++) {
    GtuberCachePluginCompatData *data;
    gchar *module_path;
    gboolean success = FALSE;
    guint j;

    const gchar *module_name;
    const gchar *const *plugin_schemes = NULL;
    const gchar *const *plugin_hosts = NULL;

    module_name = g_ptr_array_index (module_names, i);

    module_path = g_module_build_path (dir_path, module_name);
    g_debug ("Checking support: %s", module_path);

    success = gtuber_loader_check_plugin_compat (module_path,
        &plugin_schemes, &plugin_hosts);
    g_free (module_path);

    if (!success) {
      g_warning ("No exported hosts support in plugin: %s", module_name);
      continue;
    }

    write_string (file, module_name);
    data = gtuber_cache_plugin_compat_data_new (module_name);

    if (write_n_elems (file, plugin_schemes)) {
      for (j = 0; plugin_schemes[j]; j++) {
        g_debug ("Supported scheme: %s", plugin_schemes[j]);

        write_string (file, plugin_schemes[j]);
        gtuber_cache_plugin_compat_data_add_scheme (data, plugin_schemes[j]);
      }
    }
    if (write_n_elems (file, plugin_hosts)) {
      for (j = 0; plugin_hosts[j]; j++) {
        g_debug ("Supported host: %s", plugin_hosts[j]);

        write_string (file, plugin_hosts[j]);
        gtuber_cache_plugin_compat_data_add_host (data, plugin_hosts[j]);
      }
    }

    gtuber_cache_plugin_dir_data_take_plugin_data (dir_data, data);
  }

  g_ptr_array_unref (module_names);
  gtuber_cache_take_dir_data (dir_data);

  return TRUE;

fail:
  return FALSE;
}

void
gtuber_cache_init (GCancellable *cancellable, GError **error)
{
  FILE *file;
  gboolean success = FALSE;
  guint i;

  g_mutex_lock (&cache_lock);

  if (plugins_cache) {
    g_mutex_unlock (&cache_lock);
    return;
  }

  g_debug ("Initializing cache");
  plugins_cache = g_ptr_array_new_with_free_func (
      (GDestroyNotify) gtuber_cache_plugin_dir_data_free);

  if (!gtuber_cache_prepare (cancellable, error))
    return;

  file = gtuber_cache_open_read (GTUBER_CACHE_BASENAME);
  if (file) {
    gchar **dir_paths;

    dir_paths = gtuber_loader_obtain_plugin_dir_paths ();

    for (i = 0; dir_paths[i]; i++) {
      if (!(success = gtuber_cache_read_plugins_compat (file, dir_paths[i],
          cancellable, error))) {
        gtuber_cache_clear ();
        break;
      }
    }

    g_strfreev (dir_paths);
    fclose (file);
  }

  if (!success) {
    gchar **dir_paths;

    g_debug ("Plugin cache needs rewriting");

    file = gtuber_cache_open_write (GTUBER_CACHE_BASENAME);
    if (!file) {
      /* FIXME: Can we recover somehow? */
      g_assert_not_reached ();
    }

    dir_paths = gtuber_loader_obtain_plugin_dir_paths ();

    for (i = 0; dir_paths[i]; i++) {
      if (!(success = gtuber_cache_write_plugin_compat (file, dir_paths[i],
          cancellable, error))) {
        gtuber_cache_clear ();
        break;
      }
    }

    fclose (file);
    g_debug ("Plugin cache %srewritten", success ? "" : "could not be ");
  }

  if (success) {
    g_debug ("Initialized cache");
  } else {
    g_ptr_array_unref (plugins_cache);
    plugins_cache = NULL;

    g_debug ("Could not initialize cache");
  }

  g_mutex_unlock (&cache_lock);
}

static gpointer
_obtain_supported_schemes (G_GNUC_UNUSED gpointer data)
{
  GPtrArray *arr;
  gchar **schemes;
  guint i;

  gtuber_cache_init (NULL, NULL);

  if (!plugins_cache)
    return NULL;

  arr = g_ptr_array_new ();

  for (i = 0; i < plugins_cache->len; i++) {
    GtuberCachePluginDirData *dir_data;
    guint j;

    dir_data = g_ptr_array_index (plugins_cache, i);

    for (j = 0; j < dir_data->plugins->len; j++) {
      GtuberCachePluginCompatData *data;
      guint k;

      data = g_ptr_array_index (dir_data->plugins, j);

      for (k = 0; k < data->schemes->len; k++) {
        const gchar *plugin_scheme;
        gboolean present = FALSE;
        guint l;

        plugin_scheme = g_ptr_array_index (data->schemes, k);

        for (l = 0; l < arr->len; l++) {
          if ((present = strcmp (g_ptr_array_index (arr, l), plugin_scheme) == 0))
            break;
        }

        if (!present)
          g_ptr_array_add (arr, (gchar *) plugin_scheme);
      }
    }
  }

  schemes = g_new0 (gchar *, arr->len + 1);

  for (i = 0; i < arr->len; i++)
    schemes[i] = g_ptr_array_index (arr, i);

  g_ptr_array_unref (arr);

  return schemes;
}

const gchar *const *
gtuber_cache_get_supported_schemes (void)
{
  static GOnce schemes_once = G_ONCE_INIT;

  g_once (&schemes_once, _obtain_supported_schemes, NULL);
  return (const gchar *const *) schemes_once.retval;
}

GPtrArray *
gtuber_cache_find_plugins_for_uri (GUri *guri)
{
  GPtrArray *compatible;
  const gchar *scheme, *host;
  guint i, offset = 0;

  compatible = g_ptr_array_new_with_free_func (
      (GDestroyNotify) g_free);

  scheme = g_uri_get_scheme (guri);
  host = g_uri_get_host (guri);

  /* Skip common host prefixes */
  if (g_str_has_prefix (host, "www."))
    offset = 4;
  else if (g_str_has_prefix (host, "m."))
    offset = 2;

  g_debug ("Cache query, scheme: \"%s\", host: \"%s\"",
      scheme, host + offset);

  for (i = 0; i < plugins_cache->len; i++) {
    GtuberCachePluginDirData *dir_data;
    guint j;

    dir_data = g_ptr_array_index (plugins_cache, i);
    g_debug ("Searching in cached dir: %s", dir_data->dir_path);

    for (j = 0; j < dir_data->plugins->len; j++) {
      GtuberCachePluginCompatData *data;
      gboolean plausible = FALSE;
      guint k;

      data = g_ptr_array_index (dir_data->plugins, j);

      for (k = 0; k < data->schemes->len; k++) {
        const gchar *plugin_scheme;

        plugin_scheme = g_ptr_array_index (data->schemes, k);
        if ((plausible = strcmp (plugin_scheme, scheme) == 0))
          break;
      }

      /* Wrong scheme */
      if (!plausible)
        continue;

      /* For common http(s) scheme, also check host */
      if (g_str_has_prefix (scheme, "http")) {
        /* Disallow http(s) with no hosts */
        plausible = FALSE;

        for (k = 0; k < data->hosts->len; k++) {
          const gchar *plugin_host;

          plugin_host = g_ptr_array_index (data->hosts, k);
          if ((plausible = strcmp (plugin_host, host + offset) == 0))
            break;
        }
      }

      if (plausible) {
        gchar *module_path;

        module_path = g_module_build_path (dir_data->dir_path,
            data->module_name);

        g_debug ("Found plausible plugin: %s", module_path);
        g_ptr_array_add (compatible, module_path);
      }
    }
  }

  return compatible;
}

static gchar *
gtuber_cache_plugin_encode_name (const gchar *plugin_name, const gchar *key)
{
  gchar *name, *encoded;

  name = g_strjoin (".", plugin_name, key, NULL);
  encoded = g_base64_encode ((guchar*) name, strlen (name));
  g_free (name);

  return encoded;
}

/**
 * gtuber_cache_plugin_read:
 * @plugin_name: short and unique name of plugin.
 * @key: name of the key this value is associated with.
 *
 * Reads the value of a given plugin name with key.
 *
 * This is mainly useful for plugin development.
 *
 * Returns: (transfer full): cached value or %NULL if unavailable or expired.
 */
gchar *
gtuber_cache_plugin_read (const gchar *plugin_name, const gchar *key)
{
  FILE *file;
  gchar *encoded, *str = NULL;
  gboolean success = FALSE;

  g_return_val_if_fail (plugin_name != NULL, FALSE);
  g_return_val_if_fail (key != NULL, FALSE);

  g_debug ("Reading from \"%s\" cache \"%s\" data",
      plugin_name, key);

  encoded = gtuber_cache_plugin_encode_name (plugin_name, key);

  g_mutex_lock (&cache_lock);

  file = gtuber_cache_open_read (encoded);
  g_free (encoded);

  if (file) {
    GDateTime *date_time;
    gint64 curr_time, exp_time;

    date_time = g_date_time_new_now_utc ();
    curr_time = g_date_time_to_unix (date_time);
    g_date_time_unref (date_time);

    read_file_to_ptr (file, &exp_time, sizeof (gint64));

    if ((success = exp_time > curr_time)) {
      str = read_next_string (file);
      g_debug ("Read cached value: %s", str);
    } else {
      g_debug ("Cache expired");
    }

    fclose (file);
  }

  g_mutex_unlock (&cache_lock);

  return str;
}

/**
 * gtuber_cache_plugin_write:
 * @plugin_name: short and unique name of plugin.
 * @key: name of the key this value is associated with.
 * @val: value to store in cache file.
 * @exp: expire time in seconds from now.
 *
 * Writes the value of a given plugin name with key. This function
 * uses time in seconds to set how long cached value will stay valid.
 *
 * This is mainly useful for plugin development.
 */
void
gtuber_cache_plugin_write (const gchar *plugin_name,
    const gchar *key, const gchar *val, gint64 exp)
{
  GDateTime *date_time;
  gint64 epoch;

  g_return_if_fail (exp > 0);

  date_time = g_date_time_new_now_utc ();
  epoch = g_date_time_to_unix (date_time);
  g_date_time_unref (date_time);

  epoch += exp;

  gtuber_cache_plugin_write_epoch (plugin_name,
      key, val, epoch);
}

/**
 * gtuber_cache_plugin_write_epoch:
 * @plugin_name: short and unique name of plugin.
 * @key: name of the key this value is associated with.
 * @val: value to store in cache file.
 * @epoch: expire date in epoch time.
 *
 * Writes the value of a given plugin name with key. This function
 * uses epoch time to set date when cached value will expire.
 *
 * This is mainly useful for plugin development.
 */
void
gtuber_cache_plugin_write_epoch (const gchar *plugin_name,
    const gchar *key, const gchar *val, gint64 epoch)
{
  FILE *file;
  gchar *encoded;

  g_return_if_fail (plugin_name != NULL);
  g_return_if_fail (key != NULL);
  g_return_if_fail (val != NULL);
  g_return_if_fail (epoch > 0);

  g_debug ("Writing into \"%s\" cache \"%s\" data",
      plugin_name, key);

  encoded = gtuber_cache_plugin_encode_name (plugin_name, key);

  g_mutex_lock (&cache_lock);

  file = gtuber_cache_open_write (encoded);
  g_free (encoded);

  if (file) {
    write_ptr_to_file (file, &epoch, sizeof (gint64));
    write_string (file, val);
    g_debug ("Written cache value: %s, expires: %li",
        val, epoch);

    fclose (file);
  }

  g_mutex_unlock (&cache_lock);
}
