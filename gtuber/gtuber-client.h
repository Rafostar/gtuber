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

#ifndef __GTUBER_CLIENT_H__
#define __GTUBER_CLIENT_H__

#if !defined(__GTUBER_INSIDE__) && !defined(GTUBER_COMPILATION)
#error "Only <gtuber/gtuber.h> and <gtuber/gtuber-plugin-devel.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>

#include <gtuber/gtuber-media-info.h>

G_BEGIN_DECLS

#define GTUBER_TYPE_CLIENT            (gtuber_client_get_type ())
#define GTUBER_IS_CLIENT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTUBER_TYPE_CLIENT))
#define GTUBER_IS_CLIENT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTUBER_TYPE_CLIENT))
#define GTUBER_CLIENT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTUBER_TYPE_CLIENT, GtuberClientClass))
#define GTUBER_CLIENT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTUBER_TYPE_CLIENT, GtuberClient))
#define GTUBER_CLIENT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTUBER_TYPE_CLIENT, GtuberClientClass))

#define GTUBER_CLIENT_ERROR           (gtuber_client_error_quark ())

/**
 * GtuberClient:
 *
 * Gtuber web client used to obtain media info.
 */
typedef struct _GtuberClient GtuberClient;
typedef struct _GtuberClientClass GtuberClientClass;

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtuberClient, g_object_unref)
#endif

GType             gtuber_client_get_type                   (void);

GtuberClient *    gtuber_client_new                        (void);

GtuberMediaInfo * gtuber_client_fetch_media_info           (GtuberClient *client, const gchar *uri, GCancellable *cancellable, GError **error);

void              gtuber_client_fetch_media_info_async     (GtuberClient *client, const gchar *uri, GCancellable *cancellable,
                                                               GAsyncReadyCallback callback, gpointer user_data);

GtuberMediaInfo * gtuber_client_fetch_media_info_finish    (GtuberClient *client, GAsyncResult *res, GError **error);

GQuark            gtuber_client_error_quark                (void);

G_END_DECLS

#endif /* __GTUBER_CLIENT_H__ */
