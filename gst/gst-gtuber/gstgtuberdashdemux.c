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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstgtuberdashdemux.h"
#include "gstgtuberelement.h"

GST_DEBUG_CATEGORY_STATIC (gst_gtuber_dash_demux_debug);
#define GST_CAT_DEFAULT gst_gtuber_dash_demux_debug

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("application/dash+xml, source=(string)gtuber"));

static GstStaticPadTemplate videosrc_template = GST_STATIC_PAD_TEMPLATE ("video_%02u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);
static GstStaticPadTemplate audiosrc_template = GST_STATIC_PAD_TEMPLATE ("audio_%02u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);
static GstStaticPadTemplate subtitlesrc_template = GST_STATIC_PAD_TEMPLATE ("subtitle_%02u",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

#define gst_gtuber_dash_demux_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGtuberDashDemux, gst_gtuber_dash_demux,
    GST_TYPE_GTUBER_ADAPTIVE_BIN, NULL);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (gtuberdashdemux, "gtuberdashdemux",
    GST_RANK_PRIMARY + 1, GST_TYPE_GTUBER_DASH_DEMUX, gst_gtuber_element_init (plugin));

/* GObject */
static void gst_gtuber_dash_demux_constructed (GObject* object);

static void
gst_gtuber_dash_demux_class_init (GstGtuberDashDemuxClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_gtuber_dash_demux_debug, "gtuberdashdemux", 0,
      "Gtuber DASH demux");

  gobject_class->constructed = gst_gtuber_dash_demux_constructed;

  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);
  gst_element_class_add_static_pad_template (gstelement_class, &videosrc_template);
  gst_element_class_add_static_pad_template (gstelement_class, &audiosrc_template);
  gst_element_class_add_static_pad_template (gstelement_class, &subtitlesrc_template);

  gst_element_class_set_static_metadata (gstelement_class, "Gtuber DASH demuxer",
      "Codec/Demuxer/Adaptive",
      "Demuxer for Gtuber DASH data",
      "Rafał Dzięgiel <rafostar.github@gmail.com>");
}

static void
gst_gtuber_dash_demux_init (GstGtuberDashDemux *self)
{
}

static void
gst_gtuber_dash_demux_constructed (GObject* object)
{
  GstGtuberAdaptiveBin *adaptive_bin = GST_GTUBER_ADAPTIVE_BIN_CAST (object);

  adaptive_bin->demuxer = gst_element_factory_make ("dashdemux", NULL);

  GST_CALL_PARENT (G_OBJECT_CLASS, constructed, (object));
}
