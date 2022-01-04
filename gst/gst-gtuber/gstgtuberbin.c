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

#include "gstgtuberbin.h"
#include "gstgtuberelement.h"

GST_DEBUG_CATEGORY_STATIC (gst_gtuber_bin_debug);
#define GST_CAT_DEFAULT gst_gtuber_bin_debug

#define gst_gtuber_bin_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGtuberBin, gst_gtuber_bin, GST_TYPE_BIN, NULL);

/* GObject */
static void gst_gtuber_bin_finalize (GObject *object);

/* GstElement */
static GstStateChangeReturn gst_gtuber_bin_change_state (
    GstElement *element, GstStateChange transition);

/* GstBin */
static void gst_gtuber_bin_deep_element_added (GstBin *bin,
    GstBin *sub_bin, GstElement *child);

static void
gst_gtuber_bin_class_init (GstGtuberBinClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBinClass *gstbin_class = (GstBinClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_gtuber_bin_debug, "gtuberbin", 0, "Gtuber Bin");

  gobject_class->finalize = gst_gtuber_bin_finalize;
  gstbin_class->deep_element_added = gst_gtuber_bin_deep_element_added;
  gstelement_class->change_state = gst_gtuber_bin_change_state;
}

static void
gst_gtuber_bin_init (GstGtuberBin *self)
{
  g_mutex_init (&self->bin_lock);
  g_mutex_init (&self->prop_lock);
}

static void
gst_gtuber_bin_finalize (GObject *object)
{
  GstGtuberBin *self = GST_GTUBER_BIN (object);

  GST_DEBUG ("Finalize");

  gst_clear_structure (&self->gtuber_config);

  if (self->tag_event)
    gst_event_unref (self->tag_event);
  if (self->toc_event)
    gst_event_unref (self->toc_event);

  g_mutex_clear (&self->bin_lock);
  g_mutex_clear (&self->prop_lock);

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
gst_gtuber_bin_deep_element_added (GstBin *bin, GstBin *sub_bin, GstElement *child)
{
  if (GST_OBJECT_FLAG_IS_SET (child, GST_ELEMENT_FLAG_SOURCE)) {
    GstGtuberBin *self = GST_GTUBER_BIN (bin);

    GST_GTUBER_BIN_LOCK (self);

    if (self->gtuber_config) {
      gst_structure_foreach (self->gtuber_config,
          (GstStructureForeachFunc) configure_deep_element, child);
    }

    GST_GTUBER_BIN_UNLOCK (self);
  }
}

static void
gst_gtuber_bin_push_event (GstGtuberBin *self, GstEvent *event)
{
  GstIterator *iter;
  GValue value = { 0, };

  iter = gst_element_iterate_src_pads (GST_ELEMENT (self));

  while (gst_iterator_next (iter, &value) == GST_ITERATOR_OK) {
    GstPad *my_pad;

    my_pad = g_value_get_object (&value);
    gst_pad_push_event (my_pad, gst_event_ref (event));

    g_value_unset (&value);
  }
  gst_iterator_free (iter);
  gst_event_unref (event);
}

static void
gst_gtuber_bin_push_all_events (GstGtuberBin *self)
{
  GST_GTUBER_BIN_LOCK (self);

  if (self->tag_event) {
    GST_DEBUG_OBJECT (self, "Pushing TAG event dowstream");

    gst_gtuber_bin_push_event (self, self->tag_event);
    self->tag_event = NULL;
  }
  if (self->toc_event) {
    GST_DEBUG_OBJECT (self, "Pushing TOC event dowstream");

    gst_gtuber_bin_push_event (self, self->toc_event);
    self->toc_event = NULL;
  }

  GST_GTUBER_BIN_UNLOCK (self);
}

static void
gst_gtuber_bin_clear_all_events (GstGtuberBin *self)
{
  GST_GTUBER_BIN_LOCK (self);

  if (self->tag_event) {
    gst_event_unref (self->tag_event);
    self->tag_event = NULL;
  }
  if (self->toc_event) {
    gst_event_unref (self->toc_event);
    self->toc_event = NULL;
  }

  GST_GTUBER_BIN_UNLOCK (self);
}

static GstStateChangeReturn
gst_gtuber_bin_change_state (GstElement *element, GstStateChange transition)
{
  GstStateChangeReturn ret;

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      gst_gtuber_bin_push_all_events (GST_GTUBER_BIN_CAST (element));
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_gtuber_bin_clear_all_events (GST_GTUBER_BIN_CAST (element));
      break;
    default:
      break;
  }

  return ret;
}

gboolean
gst_gtuber_bin_sink_event (GstPad *pad, GstObject *parent, GstEvent *event)
{
  GstGtuberBin *self = GST_GTUBER_BIN_CAST (parent);

  switch (event->type) {
    case GST_EVENT_TAG:
      GST_GTUBER_BIN_LOCK (self);

      if (self->tag_event)
        gst_event_unref (self->tag_event);
      self->tag_event = gst_event_ref (event);

      GST_GTUBER_BIN_UNLOCK (self);
      break;
    case GST_EVENT_TOC:
      GST_GTUBER_BIN_LOCK (self);

      if (self->toc_event)
        gst_event_unref (self->toc_event);
      self->toc_event = gst_event_ref (event);

      GST_GTUBER_BIN_UNLOCK (self);
      break;
    case GST_EVENT_CUSTOM_DOWNSTREAM_STICKY:{
      const GstStructure *structure = gst_event_get_structure (event);

      if (gst_structure_has_name (structure, GST_GTUBER_CONFIG)) {
        GST_DEBUG_OBJECT (self, "Received " GST_GTUBER_CONFIG " event");
        GST_GTUBER_BIN_LOCK (self);

        gst_clear_structure (&self->gtuber_config);
        self->gtuber_config = gst_structure_copy (structure);

        GST_GTUBER_BIN_UNLOCK (self);
      }
      break;
    }
    default:
      break;
  }

  return gst_pad_event_default (pad, parent, event);
}
