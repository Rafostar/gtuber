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

#ifndef __GTUBER_UTILS_JSON_H__
#define __GTUBER_UTILS_JSON_H__

#include <glib.h>
#include <json-glib/json-glib.h>
#include <gtuber/gtuber.h>

G_BEGIN_DECLS

const gchar *        gtuber_utils_json_get_string           (JsonReader *reader, const gchar *key, ...) G_GNUC_NULL_TERMINATED;

gint64               gtuber_utils_json_get_int              (JsonReader *reader, const gchar *key, ...) G_GNUC_NULL_TERMINATED;

gboolean             gtuber_utils_json_get_boolean          (JsonReader *reader, const gchar *key, ...) G_GNUC_NULL_TERMINATED;

gboolean             gtuber_utils_json_go_to                (JsonReader *reader, const gchar *key, ...) G_GNUC_NULL_TERMINATED;

void                 gtuber_utils_json_go_back              (JsonReader *reader, guint count);

gboolean             gtuber_utils_json_array_foreach        (JsonReader *reader, GtuberMediaInfo *info, GtuberFunc func, gpointer user_data);

void                 gtuber_utils_json_parser_debug         (JsonParser *parser);

G_END_DECLS

#endif /* __GTUBER_UTILS_JSON_H__ */
