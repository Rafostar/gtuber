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

#include <glib-object.h>
#include <gio/gio.h>
#include <libsoup/soup.h>

#include <gtuber/gtuber-enums.h>

G_BEGIN_DECLS

#define GTUBER_TYPE_HEARTBEAT            (gtuber_heartbeat_get_type ())
#define GTUBER_IS_HEARTBEAT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTUBER_TYPE_HEARTBEAT))
#define GTUBER_IS_HEARTBEAT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTUBER_TYPE_HEARTBEAT))
#define GTUBER_HEARTBEAT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTUBER_TYPE_HEARTBEAT, GtuberHeartbeatClass))
#define GTUBER_HEARTBEAT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTUBER_TYPE_HEARTBEAT, GtuberHeartbeat))
#define GTUBER_HEARTBEAT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTUBER_TYPE_HEARTBEAT, GtuberHeartbeatClass))

#define GTUBER_HEARTBEAT_ERROR           (gtuber_heartbeat_error_quark ())

/**
 * GtuberHeartbeat:
 *
 * Media info heartbeat base class.
 */
typedef struct _GtuberHeartbeat GtuberHeartbeat;
typedef struct _GtuberHeartbeatClass GtuberHeartbeatClass;

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtuberHeartbeat, g_object_unref)
#endif

struct _GtuberHeartbeat
{
  GObject parent;
};

/**
 * GtuberHeartbeatClass:
 * @parent_class: The object class structure.
 * @ping: Create and pass #SoupMessage to send.
 * @pong: Read ping response.
 */
struct _GtuberHeartbeatClass
{
  GObjectClass parent_class;

  GtuberFlow (* ping) (GtuberHeartbeat *heartbeat,
                       SoupMessage    **msg,
                       GError         **error);

  GtuberFlow (* pong) (GtuberHeartbeat *heartbeat,
                       SoupMessage     *msg,
                       GInputStream    *stream,
                       GError         **error);
};

GType         gtuber_heartbeat_get_type              (void);

void          gtuber_heartbeat_set_interval          (GtuberHeartbeat *heartbeat, guint interval);

GQuark        gtuber_heartbeat_error_quark           (void);

G_END_DECLS
