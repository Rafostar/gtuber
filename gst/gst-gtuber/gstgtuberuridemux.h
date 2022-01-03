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

#include <gst/gst.h>
#include <gst/base/gstadapter.h>

#include "gstgtuberbin.h"

G_BEGIN_DECLS

#define GST_TYPE_GTUBER_URI_DEMUX            (gst_gtuber_uri_demux_get_type ())
#define GST_IS_GTUBER_URI_DEMUX(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GST_TYPE_GTUBER_URI_DEMUX))
#define GST_IS_GTUBER_URI_DEMUX_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GST_TYPE_GTUBER_URI_DEMUX))
#define GST_GTUBER_URI_DEMUX_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GST_TYPE_GTUBER_URI_DEMUX, GstGtuberUriDemuxClass))
#define GST_GTUBER_URI_DEMUX(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GST_TYPE_GTUBER_URI_DEMUX, GstGtuberUriDemux))
#define GST_GTUBER_URI_DEMUX_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GST_TYPE_GTUBER_URI_DEMUX, GstGtuberUriDemuxClass))
#define GST_GTUBER_URI_DEMUX_CAST(obj)       ((GstGtuberUriDemux*)(obj))

typedef struct _GstGtuberUriDemux GstGtuberUriDemux;
typedef struct _GstGtuberUriDemuxClass GstGtuberUriDemuxClass;

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GstGtuberUriDemux, gst_object_unref)
#endif

struct _GstGtuberUriDemux
{
  GstGtuberBin parent;

  GstAdapter *input_adapter;

  GstElement *uri_handler;
  GstElement *typefind;

  GstPad *typefind_src;
};

struct _GstGtuberUriDemuxClass
{
  GstGtuberBinClass parent_class;
};

GType gst_gtuber_uri_demux_get_type (void);

G_END_DECLS
