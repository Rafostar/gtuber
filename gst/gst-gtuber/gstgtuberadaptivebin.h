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

#include "gstgtuberbin.h"

G_BEGIN_DECLS

#define GST_TYPE_GTUBER_ADAPTIVE_BIN            (gst_gtuber_adaptive_bin_get_type ())
#define GST_IS_GTUBER_ADAPTIVE_BIN(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_GTUBER_ADAPTIVE_BIN))
#define GST_IS_GTUBER_ADAPTIVE_BIN_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_GTUBER_ADAPTIVE_BIN))
#define GST_GTUBER_ADAPTIVE_BIN_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_GTUBER_ADAPTIVE_BIN, GstGtuberAdaptiveBinClass))
#define GST_GTUBER_ADAPTIVE_BIN(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_GTUBER_ADAPTIVE_BIN, GstGtuberAdaptiveBin))
#define GST_GTUBER_ADAPTIVE_BIN_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_GTUBER_ADAPTIVE_BIN, GstGtuberAdaptiveBinClass))
#define GST_GTUBER_ADAPTIVE_BIN_CAST(obj)       ((GstGtuberAdaptiveBin*)(obj))

typedef struct _GstGtuberAdaptiveBin GstGtuberAdaptiveBin;
typedef struct _GstGtuberAdaptiveBinClass GstGtuberAdaptiveBinClass;

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GstGtuberAdaptiveBin, gst_object_unref)
#endif

struct _GstGtuberAdaptiveBin
{
  GstGtuberBin parent;

  guint initial_bitrate;
  guint target_bitrate;

  gboolean needs_playback_config;

  GstElement *demuxer;
};

struct _GstGtuberAdaptiveBinClass
{
  GstGtuberBinClass parent_class;
};

GType gst_gtuber_adaptive_bin_get_type (void);

G_END_DECLS
