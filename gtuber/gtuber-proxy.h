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

#if !defined(__GTUBER_INSIDE__) && !defined(GTUBER_COMPILATION)
#error "Only <gtuber/gtuber.h> and <gtuber/gtuber-plugin-devel.h> can be included directly."
#endif

#include <glib-object.h>
#include <gio/gio.h>
#include <libsoup/soup.h>

#include <gtuber/gtuber-enums.h>
#include <gtuber/gtuber-threaded-object.h>
#include <gtuber/gtuber-stream.h>

G_BEGIN_DECLS

#define GTUBER_TYPE_PROXY            (gtuber_proxy_get_type ())
#define GTUBER_IS_PROXY(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTUBER_TYPE_PROXY))
#define GTUBER_IS_PROXY_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTUBER_TYPE_PROXY))
#define GTUBER_PROXY_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTUBER_TYPE_PROXY, GtuberProxyClass))
#define GTUBER_PROXY(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTUBER_TYPE_PROXY, GtuberProxy))
#define GTUBER_PROXY_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTUBER_TYPE_PROXY, GtuberProxyClass))

#define GTUBER_PROXY_ERROR           (gtuber_proxy_error_quark ())

/**
 * GtuberProxy:
 *
 * Media info proxy base class.
 */
typedef struct _GtuberProxy GtuberProxy;
typedef struct _GtuberProxyClass GtuberProxyClass;

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtuberProxy, g_object_unref)
#endif

struct _GtuberProxy
{
  GtuberThreadedObject parent;
};

/**
 * GtuberProxyClass:
 * @parent_class: The object class structure.
 * @fetch_stream: Proxy request to download data for #GtuberStream.
 */
struct _GtuberProxyClass
{
  GtuberThreadedObjectClass parent_class;

  GtuberFlow (* fetch_stream) (GtuberProxy       *proxy,
                               SoupServerMessage *msg,
                               GtuberStream      *stream,
                               GError           **error);
};

GType         gtuber_proxy_get_type              (void);

GQuark        gtuber_proxy_error_quark           (void);

G_END_DECLS
