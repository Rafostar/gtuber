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

#ifndef __GTUBER_UTILS_H__
#define __GTUBER_UTILS_H__

#include <glib.h>
#include <gio/gio.h>
#include <gtuber/gtuber.h>

G_BEGIN_DECLS

#define GTUBER_UTILS_ERROR (gtuber_utils_error_quark ())
GQuark               gtuber_utils_error_quark                                (void);

GtuberStreamMimeType gtuber_utils_get_mime_type_from_string                  (const gchar *string);

gboolean             gtuber_utils_add_adaptive_streams_from_hls_input_stream (GInputStream *stream,
                                                                                 GtuberMediaInfo *info, GError **error);

G_END_DECLS

#endif /* __GTUBER_UTILS_H__ */
