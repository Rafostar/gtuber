/*
 * Copyright (C) 2022 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#pragma once

#include <glib.h>
#include <libxml/tree.h>

G_BEGIN_DECLS

xmlDoc *          gtuber_utils_xml_load_html_from_data         (const gchar *data, GError **error);

const gchar *     gtuber_utils_xml_get_property_content        (xmlDoc *doc, const gchar *name);

gchar *           gtuber_utils_xml_obtain_json_in_node         (xmlDoc *doc, const gchar *json_name);

G_END_DECLS
