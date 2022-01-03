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

#pragma once

#include <gst/gst.h>
#include <gst/gstbin.h>

G_BEGIN_DECLS

#define GST_TYPE_GTUBER_BIN               (gst_gtuber_bin_get_type ())
#define GST_IS_GTUBER_BIN(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_GTUBER_BIN))
#define GST_IS_GTUBER_BIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_GTUBER_BIN))
#define GST_GTUBER_BIN_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_GTUBER_BIN, GstGtuberBinClass))
#define GST_GTUBER_BIN(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_GTUBER_BIN, GstGtuberBin))
#define GST_GTUBER_BIN_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_GTUBER_BIN, GstGtuberBinClass))
#define GST_GTUBER_BIN_CAST(obj)          ((GstGtuberBin*)(obj))

#define GST_GTUBER_BIN_GET_LOCK(obj)      (&GST_GTUBER_BIN_CAST(obj)->bin_lock)
#define GST_GTUBER_BIN_LOCK(obj)          g_mutex_lock (GST_GTUBER_BIN_GET_LOCK(obj))
#define GST_GTUBER_BIN_UNLOCK(obj)        g_mutex_unlock (GST_GTUBER_BIN_GET_LOCK(obj))

#define GST_GTUBER_BIN_GET_PROP_LOCK(obj) (&GST_GTUBER_BIN_CAST(obj)->prop_lock)
#define GST_GTUBER_BIN_PROP_LOCK(obj)     g_mutex_lock (GST_GTUBER_BIN_GET_PROP_LOCK(obj))
#define GST_GTUBER_BIN_PROP_UNLOCK(obj)   g_mutex_unlock (GST_GTUBER_BIN_GET_PROP_LOCK(obj))

typedef struct _GstGtuberBin GstGtuberBin;
typedef struct _GstGtuberBinClass GstGtuberBinClass;

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GstGtuberBin, gst_object_unref)
#endif

struct _GstGtuberBin
{
  GstBin parent;

  GMutex bin_lock;
  GMutex prop_lock;

  GstStructure *gtuber_config;

  GstEvent *tag_event;
  GstEvent *toc_event;
};

struct _GstGtuberBinClass
{
  GstBinClass parent_class;
};

GType gst_gtuber_bin_get_type (void);

gboolean gst_gtuber_bin_sink_event (GstPad *pad, GstObject *parent, GstEvent *event);

G_END_DECLS
