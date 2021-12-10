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

#ifndef __GTUBER_UTILS_COMMON_H__
#define __GTUBER_UTILS_COMMON_H__

#include <glib.h>
#include <gio/gio.h>
#include <libsoup/soup.h>
#include <gtuber/gtuber.h>

G_BEGIN_DECLS

gboolean             gtuber_utils_common_uri_matches_hosts              (GUri *uri, gint *match, const gchar *search_host, ...) G_GNUC_NULL_TERMINATED;

gboolean             gtuber_utils_common_uri_matches_hosts_array        (GUri *uri, gint *match, const gchar *const *hosts);

gchar *              gtuber_utils_common_obtain_uri_id_from_paths       (GUri *uri, gint *match, const gchar *search_path1, ...) G_GNUC_NULL_TERMINATED;

gchar *              gtuber_utils_common_obtain_uri_query_value         (GUri *uri, const gchar *key);

gchar *              gtuber_utils_common_obtain_uri_with_query_as_path  (const gchar *uri_str);

gchar *              gtuber_utils_common_obtain_uri_source              (GUri *uri);

gchar *              gtuber_utils_common_replace_uri_source             (const gchar *uri_str, const gchar *src_uri_str);

void                 gtuber_utils_common_msg_take_request               (SoupMessage *msg, const gchar *content_type, gchar *req_body);

GtuberStreamMimeType gtuber_utils_common_get_mime_type_from_string      (const gchar *string);

gboolean             gtuber_utils_common_parse_hls_input_stream         (GInputStream *stream, GtuberMediaInfo *info, GError **error);

G_END_DECLS

#endif /* __GTUBER_UTILS_COMMON_H__ */
