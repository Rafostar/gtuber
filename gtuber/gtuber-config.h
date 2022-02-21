/*
 * Copyright (C) 2022 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#pragma once

#if !defined(__GTUBER_INSIDE__) && !defined(GTUBER_COMPILATION)
#error "Only <gtuber/gtuber.h> and <gtuber/gtuber-plugin-devel.h> can be included directly."
#endif

#include <glib.h>
#include <gio/gio.h>

G_BEGIN_DECLS

gchar *     gtuber_config_obtain_config_dir_path              (void);

gchar *     gtuber_config_obtain_config_file_path             (const gchar *file_name);

GFile *     gtuber_config_obtain_config_dir                   (void);

GFile *     gtuber_config_obtain_config_dir_file              (const gchar *file_name);

gchar **    gtuber_config_read_plugin_hosts_file              (const gchar *file_name);

gchar **    gtuber_config_read_plugin_hosts_file_with_prepend (const gchar *file_name, ...) G_GNUC_NULL_TERMINATED;

G_END_DECLS
