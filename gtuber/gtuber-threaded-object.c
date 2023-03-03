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

/**
 * SECTION:gtuber-threaded-object
 * @title: GtuberThreadedObject
 * @short_description: a base class for creating extra media info objects
 *     that run their code in separate thread.
 */

#include "gtuber-threaded-object.h"

struct _GtuberThreadedObjectPrivate
{
  GCond cond;
  GThread *thread;
  GMainContext *context;
  GMainLoop *loop;
};

#define parent_class gtuber_threaded_object_parent_class
G_DEFINE_TYPE_WITH_CODE (GtuberThreadedObject, gtuber_threaded_object, G_TYPE_OBJECT,
    G_ADD_PRIVATE (GtuberThreadedObject))

static gboolean
main_loop_running_cb (GtuberThreadedObject *self)
{
  GtuberThreadedObjectPrivate *priv = gtuber_threaded_object_get_instance_private (self);

  g_debug ("ThreadedObject main loop is running");

  GTUBER_THREADED_OBJECT_LOCK (self);
  g_cond_signal (&priv->cond);
  GTUBER_THREADED_OBJECT_UNLOCK (self);

  return G_SOURCE_REMOVE;
}

static gpointer
gtuber_threaded_object_main (GtuberThreadedObject *self)
{
  GtuberThreadedObjectPrivate *priv = gtuber_threaded_object_get_instance_private (self);
  GtuberThreadedObjectClass *threaded_object_class = GTUBER_THREADED_OBJECT_GET_CLASS (self);
  GSource *idle_source;

  g_debug ("ThreadedObject thread: %p", g_thread_self ());

  priv->context = g_main_context_new ();
  priv->loop = g_main_loop_new (priv->context, FALSE);

  g_main_context_push_thread_default (priv->context);

  threaded_object_class->thread_start (self);

  idle_source = g_idle_source_new ();
  g_source_set_callback (idle_source, (GSourceFunc) main_loop_running_cb, self, NULL);
  g_source_attach (idle_source, priv->context);
  g_source_unref (idle_source);

  g_main_loop_run (priv->loop);
  g_debug ("ThreadedObject main loop stopped");

  threaded_object_class->thread_stop (self);

  g_main_context_pop_thread_default (priv->context);
  g_main_context_unref (priv->context);

  return NULL;
}

static void
gtuber_threaded_object_init (GtuberThreadedObject *self)
{
  GtuberThreadedObjectPrivate *priv = gtuber_threaded_object_get_instance_private (self);

  g_mutex_init (&self->lock);
  g_cond_init (&priv->cond);
}

static void
gtuber_threaded_object_constructed (GObject *object)
{
  GtuberThreadedObject *self = GTUBER_THREADED_OBJECT (object);
  GtuberThreadedObjectPrivate *priv = gtuber_threaded_object_get_instance_private (self);

  g_debug ("ThreadedObject constructed from thread: %p", g_thread_self ());

  GTUBER_THREADED_OBJECT_LOCK (self);

  priv->thread = g_thread_new ("GtuberThreadedObject",
      (GThreadFunc) gtuber_threaded_object_main, self);
  while (!priv->loop || !g_main_loop_is_running (priv->loop))
    g_cond_wait (&priv->cond, &self->lock);

  GTUBER_THREADED_OBJECT_UNLOCK (self);

  G_OBJECT_CLASS (parent_class)->constructed (object);
}

static void
gtuber_threaded_object_dispose (GObject *object)
{
  GtuberThreadedObject *self = GTUBER_THREADED_OBJECT (object);
  GtuberThreadedObjectPrivate *priv = gtuber_threaded_object_get_instance_private (self);

  GTUBER_THREADED_OBJECT_LOCK (self);

  if (priv->loop) {
    g_main_loop_quit (priv->loop);

    if (priv->thread != g_thread_self ())
      g_thread_join (priv->thread);
    else
      g_thread_unref (priv->thread);

    g_main_loop_unref (priv->loop);
    priv->loop = NULL;
  }

  GTUBER_THREADED_OBJECT_UNLOCK (self);

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gtuber_threaded_object_finalize (GObject *object)
{
  GtuberThreadedObject *self = GTUBER_THREADED_OBJECT (object);
  GtuberThreadedObjectPrivate *priv = gtuber_threaded_object_get_instance_private (self);

  g_debug ("ThreadedObject finalize");

  g_mutex_clear (&self->lock);
  g_cond_clear (&priv->cond);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static void
gtuberthreaded_object_thread_start (GtuberThreadedObject *self)
{
}

static void
gtuberthreaded_object_thread_stop (GtuberThreadedObject *self)
{
}

static void
gtuber_threaded_object_class_init (GtuberThreadedObjectClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GtuberThreadedObjectClass *threaded_object = (GtuberThreadedObjectClass *) klass;

  gobject_class->constructed = gtuber_threaded_object_constructed;
  gobject_class->dispose = gtuber_threaded_object_dispose;
  gobject_class->finalize = gtuber_threaded_object_finalize;

  threaded_object->thread_start = gtuberthreaded_object_thread_start;
  threaded_object->thread_stop = gtuberthreaded_object_thread_stop;
}

GThread *
gtuber_threaded_object_get_thread (GtuberThreadedObject *self)
{
  GtuberThreadedObjectPrivate *priv;

  g_return_val_if_fail (GTUBER_IS_THREADED_OBJECT (self), NULL);

  priv = gtuber_threaded_object_get_instance_private (self);

  return priv->thread;
}

GMainContext *
gtuber_threaded_object_get_context (GtuberThreadedObject *self)
{
  GtuberThreadedObjectPrivate *priv;

  g_return_val_if_fail (GTUBER_IS_THREADED_OBJECT (self), NULL);

  priv = gtuber_threaded_object_get_instance_private (self);

  return priv->context;
}
