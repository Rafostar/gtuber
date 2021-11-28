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
    GST_TYPE_BIN, NULL);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (gtuberdashdemux, "gtuberdashdemux",
    GST_RANK_PRIMARY + 1, GST_TYPE_GTUBER_DASH_DEMUX, gst_gtuber_element_init (plugin));

/* GObject */
static void gst_gtuber_dash_demux_finalize (GObject *object);

/* GstPad */
static gboolean gst_gtuber_dash_demux_sink_event (GstPad *pad, GstObject *parent,
    GstEvent *event);

/* GstElement */
static GstStateChangeReturn gst_gtuber_dash_demux_change_state (
    GstElement *element, GstStateChange transition);
static void dashdemux_pad_added_cb (GstElement *element,
    GstPad *pad, GstGtuberDashDemux *self);

/* GstBin */
static void gst_gtuber_dash_demux_deep_element_added (GstBin *bin,
    GstBin *sub_bin, GstElement *child);

static void
gst_gtuber_dash_demux_class_init (GstGtuberDashDemuxClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBinClass *gstbin_class = (GstBinClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_gtuber_dash_demux_debug, "gtuber_dash_demux", 0,
      "Gtuber DASH demux");

  gobject_class->finalize = gst_gtuber_dash_demux_finalize;
  gstbin_class->deep_element_added = gst_gtuber_dash_demux_deep_element_added;
  gstelement_class->change_state = gst_gtuber_dash_demux_change_state;

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
  GstPad *pad, *ghostpad;

  self->gtuber_config = NULL;
  self->dashdemux = gst_element_factory_make ("dashdemux", NULL);
  gst_bin_add (GST_BIN (self), self->dashdemux);

  gst_element_set_locked_state (self->dashdemux, TRUE);

  /* Create sink ghost pad */
  pad = gst_element_get_static_pad (self->dashdemux, "sink");
  ghostpad = gst_ghost_pad_new_from_template ("sink", pad,
      gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (self), "sink"));
  gst_object_unref (pad);
  gst_pad_set_event_function (ghostpad,
      GST_DEBUG_FUNCPTR (gst_gtuber_dash_demux_sink_event));

  gst_pad_set_active (ghostpad, TRUE);

  if (!gst_element_add_pad (GST_ELEMENT (self), ghostpad))
    g_critical ("Failed to add sink pad to bin");

  g_signal_connect (self->dashdemux, "pad-added",
      (GCallback) dashdemux_pad_added_cb, self);
}

static void
gst_gtuber_dash_demux_finalize (GObject *object)
{
  GstGtuberDashDemux *self = GST_GTUBER_DASH_DEMUX (object);

  GST_DEBUG ("Finalize");

  gst_clear_structure (&self->gtuber_config);

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static gboolean
configure_deep_element (GQuark field_id, const GValue *value, GstElement *child)
{
  GObjectClass *gobject_class = G_OBJECT_GET_CLASS (child);
  const gchar *prop_name = g_quark_to_string (field_id);

  if (g_object_class_find_property (gobject_class, prop_name)) {
    g_object_set_property (G_OBJECT (child), prop_name, value);

    if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) >= GST_LEVEL_DEBUG) {
      gchar *el_name;

      el_name = gst_object_get_name (GST_OBJECT (child));
      GST_DEBUG ("Set %s %s", el_name, prop_name);

      g_free (el_name);
    }
  }

  return TRUE;
}

static void
gst_gtuber_dash_demux_deep_element_added (GstBin *bin,
    GstBin *sub_bin, GstElement *child)
{
  GST_BIN_CLASS (parent_class)->deep_element_added (bin, sub_bin, child);

  if (GST_OBJECT_FLAG_IS_SET (child, GST_ELEMENT_FLAG_SOURCE)) {
    GstGtuberDashDemux *self = GST_GTUBER_DASH_DEMUX (bin);

    if (self->gtuber_config) {
      gst_structure_foreach (self->gtuber_config,
          (GstStructureForeachFunc) configure_deep_element, child);
    }
  }
}

static void
dashdemux_pad_added_cb (GstElement *element, GstPad *pad, GstGtuberDashDemux *self)
{
  GstPad *ghostpad;
  GstPadTemplate *template;
  gchar *pad_name;

  if (gst_pad_get_direction (pad) != GST_PAD_SRC)
    return;

  template = gst_pad_get_pad_template (pad);
  pad_name = gst_object_get_name (GST_OBJECT (pad));
  ghostpad = gst_ghost_pad_new_from_template (pad_name, pad,
      gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (self),
          GST_PAD_TEMPLATE_NAME_TEMPLATE (template)));
  gst_object_unref (template);

  GST_DEBUG ("Adding src ghostpad: %s", pad_name);
  g_free (pad_name);

  gst_pad_set_active (ghostpad, TRUE);

  if (!gst_element_add_pad (GST_ELEMENT (self), ghostpad))
    g_critical ("Failed to add source pad to bin");
}

static gboolean
gst_gtuber_dash_demux_configure (GstGtuberDashDemux *self)
{
  GST_DEBUG ("Configuring");

  gst_element_set_locked_state (self->dashdemux, FALSE);
  if (!gst_element_sync_state_with_parent (self->dashdemux))
    goto error_sync_state;

  GST_DEBUG ("Configured");

  return TRUE;

error_sync_state:
  GST_ELEMENT_ERROR (self, CORE, STATE_CHANGE,
      ("Failed to sync state"), (NULL));
  return FALSE;
}

static GstStateChangeReturn
gst_gtuber_dash_demux_change_state (GstElement *element, GstStateChange transition)
{
  GstGtuberDashDemux *self;
  GstStateChangeReturn ret;

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  self = GST_GTUBER_DASH_DEMUX (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_gtuber_dash_demux_configure (self))
        return GST_STATE_CHANGE_FAILURE;
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
gst_gtuber_dash_demux_sink_event (GstPad *pad, GstObject *parent, GstEvent *event)
{
  gboolean res = FALSE;

  switch (event->type) {
    case GST_EVENT_CUSTOM_DOWNSTREAM_STICKY:{
      const GstStructure *structure = gst_event_get_structure (event);

      if (gst_structure_has_name (structure, GST_GTUBER_CONFIG)) {
        GstGtuberDashDemux *self = GST_GTUBER_DASH_DEMUX (parent);

        GST_DEBUG_OBJECT (self, "Received " GST_GTUBER_CONFIG " event");
        GST_OBJECT_LOCK (self);

        gst_clear_structure (&self->gtuber_config);
        self->gtuber_config = gst_structure_copy (structure);

        GST_OBJECT_UNLOCK (self);
      }
    }
    /* Fall through */
    default:
      res = gst_pad_event_default (pad, parent, event);
      break;
  }

  return res;
}
