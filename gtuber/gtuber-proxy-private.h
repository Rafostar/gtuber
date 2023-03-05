/*
 * Copyright (C) 2023 Rafał Dzięgiel <rafostar.github@gmail.com>
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

G_BEGIN_DECLS

typedef struct _GtuberProxyPrivate GtuberProxyPrivate;

G_GNUC_INTERNAL
void gtuber_proxy_configure (GtuberProxy *proxy, const gchar *media_id, GPtrArray *streams,
    GPtrArray *adaptive_streams, GHashTable *req_headers);

G_END_DECLS
