/*
 * Copyright (C) 2021 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "gstgtuberhlsdemux.h"
#include "gstgtuberelement.h"

#define GST_CAT_DEFAULT gst_gtuber_hls_demux_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/x-hls, source=(string)gtuber"));

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src_%u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

#define parent_class gst_gtuber_hls_demux_parent_class
G_DEFINE_TYPE_WITH_CODE (GstGtuberHlsDemux, gst_gtuber_hls_demux,
    GST_TYPE_GTUBER_ADAPTIVE_BIN, NULL);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (gtuberhlsdemux, "gtuberhlsdemux",
    GST_RANK_PRIMARY + 10, GST_TYPE_GTUBER_HLS_DEMUX, gst_gtuber_element_init (plugin));

static void
gst_gtuber_hls_demux_init (GstGtuberHlsDemux *self)
{
}

static void
gst_gtuber_hls_demux_class_init (GstGtuberHlsDemuxClass *klass)
{
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_gtuber_hls_demux_debug, "gtuberhlsdemux", 0,
      "Gtuber HLS demux");

  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);
  gst_element_class_add_static_pad_template (gstelement_class, &src_template);

  gst_element_class_set_static_metadata (gstelement_class, "Gtuber HLS demuxer",
      "Codec/Demuxer/Adaptive",
      "Demuxer for Gtuber HLS data",
      "Rafał Dzięgiel <rafostar.github@gmail.com>");
}
