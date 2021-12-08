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

#ifndef __GTUBER_LOADER_PRIVATE_H__
#define __GTUBER_LOADER_PRIVATE_H__

#include <glib.h>
#include <gmodule.h>

#include <gtuber/gtuber-website.h>

G_BEGIN_DECLS

G_GNUC_INTERNAL
GtuberWebsite * gtuber_loader_get_website_for_uri (GUri *guri, GModule **module);

G_GNUC_INTERNAL
gboolean gtuber_loader_check_plugin_compat (const gchar *module_path,
    const gchar *const **schemes, const gchar *const **hosts);

G_GNUC_INTERNAL
const gchar * gtuber_loader_get_plugin_dir_path_string (void);

G_GNUC_INTERNAL
gchar ** gtuber_loader_obtain_plugin_dir_paths (void);

G_GNUC_INTERNAL
gboolean gtuber_loader_name_is_plugin (const gchar *module_name);

G_GNUC_INTERNAL
void gtuber_loader_close_module (GModule *module);

G_END_DECLS

#endif /* __GTUBER_LOADER_PRIVATE_H__ */
