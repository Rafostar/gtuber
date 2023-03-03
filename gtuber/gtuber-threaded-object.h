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

G_BEGIN_DECLS

#define GTUBER_TYPE_THREADED_OBJECT            (gtuber_threaded_object_get_type ())
#define GTUBER_IS_THREADED_OBJECT(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTUBER_TYPE_THREADED_OBJECT))
#define GTUBER_IS_THREADED_OBJECT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTUBER_TYPE_THREADED_OBJECT))
#define GTUBER_THREADED_OBJECT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTUBER_TYPE_THREADED_OBJECT, GtuberThreadedObjectClass))
#define GTUBER_THREADED_OBJECT(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTUBER_TYPE_THREADED_OBJECT, GtuberThreadedObject))
#define GTUBER_THREADED_OBJECT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTUBER_TYPE_THREADED_OBJECT, GtuberThreadedObjectClass))

#define GTUBER_THREADED_OBJECT_GET_LOCK(obj)   (&((GtuberThreadedObject *)(obj))->lock)
#define GTUBER_THREADED_OBJECT_LOCK(obj)       (g_mutex_lock (GTUBER_THREADED_OBJECT_GET_LOCK (obj)))
#define GTUBER_THREADED_OBJECT_UNLOCK(obj)     (g_mutex_unlock (GTUBER_THREADED_OBJECT_GET_LOCK (obj)))

#define GTUBER_THREADED_OBJECT_ERROR           (gtuber_threaded_object_error_quark ())

/**
 * GtuberThreadedObject:
 *
 * Media info extra threaded object base class.
 */
typedef struct _GtuberThreadedObject GtuberThreadedObject;
typedef struct _GtuberThreadedObjectClass GtuberThreadedObjectClass;
typedef struct _GtuberThreadedObjectPrivate GtuberThreadedObjectPrivate;

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtuberThreadedObject, g_object_unref)
#endif

struct _GtuberThreadedObject
{
  GObject parent;

  GMutex lock;
};

/**
 * GtuberThreadedObjectClass:
 * @parent_class: The object class structure.
 * @thread_start: Thread is starting (before mainloop).
 * @thread_stop: Thread is stopping (after mainloop).
 */
struct _GtuberThreadedObjectClass
{
  GObjectClass parent_class;

  void (* thread_start) (GtuberThreadedObject *threaded_object);

  void (* thread_stop) (GtuberThreadedObject *threaded_object);
};

GType          gtuber_threaded_object_get_type              (void);

GThread *      gtuber_threaded_object_get_thread            (GtuberThreadedObject *threaded_object);

GMainContext * gtuber_threaded_object_get_context           (GtuberThreadedObject *threaded_object);

G_END_DECLS
