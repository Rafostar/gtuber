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

#include "gstgtuberbin.h"
#include "gstgtuberelement.h"

#define GST_CAT_DEFAULT gst_gtuber_bin_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define parent_class gst_gtuber_bin_parent_class
G_DEFINE_TYPE_WITH_CODE (GstGtuberBin, gst_gtuber_bin, GST_TYPE_BIN, NULL);

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

  GST_TRACE ("Finalize");

  gst_clear_structure (&self->http_headers);

  if (self->tag_event)
    gst_event_unref (self->tag_event);
  if (self->toc_event)
    gst_event_unref (self->toc_event);

  g_mutex_clear (&self->bin_lock);
  g_mutex_clear (&self->prop_lock);

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
_set_property (GstObject *obj, const gchar *prop_name, gpointer value)
{
  g_object_set (G_OBJECT (obj), prop_name, value, NULL);

  if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) >= GST_LEVEL_DEBUG) {
    gchar *el_name;

    el_name = gst_object_get_name (obj);
    GST_DEBUG ("Set %s %s", el_name, prop_name);

    g_free (el_name);
  }
}

static gboolean
configure_deep_element (GQuark field_id, const GValue *value, GstElement *child)
{
  GObjectClass *gobject_class;
  const GstStructure *substructure;

  if (!GST_VALUE_HOLDS_STRUCTURE (value))
    return TRUE;

  substructure = gst_value_get_structure (value);

  if (!gst_structure_has_name (substructure, GST_GTUBER_REQ_HEADERS))
    return TRUE;

  gobject_class = G_OBJECT_GET_CLASS (child);

  if (g_object_class_find_property (gobject_class, "user-agent")) {
    const gchar *ua;

    if ((ua = gst_structure_get_string (substructure, GST_GTUBER_HEADER_UA)))
      _set_property (GST_OBJECT_CAST (child), "user-agent", (gchar *) ua);
  }

  if (g_object_class_find_property (gobject_class, "extra-headers")) {
    GstStructure *extra_headers;

    extra_headers = gst_structure_copy (substructure);
    gst_structure_set_name (extra_headers, "extra-headers");
    gst_structure_remove_field (extra_headers, GST_GTUBER_HEADER_UA);

    _set_property (GST_OBJECT_CAST (child), "extra-headers", extra_headers);

    gst_structure_free (extra_headers);
  }

  return TRUE;
}

static void
gst_gtuber_bin_deep_element_added (GstBin *bin, GstBin *sub_bin, GstElement *child)
{
  if (GST_OBJECT_FLAG_IS_SET (child, GST_ELEMENT_FLAG_SOURCE)) {
    GstGtuberBin *self = GST_GTUBER_BIN (bin);

    GST_GTUBER_BIN_LOCK (self);

    if (self->http_headers) {
      gst_structure_foreach (self->http_headers,
          (GstStructureForeachFunc) configure_deep_element, child);
    }

    GST_GTUBER_BIN_UNLOCK (self);
  }
}

static void
gst_gtuber_bin_push_event (GstGtuberBin *self, GstPad *pad, GstEvent *event)
{
  /* We might have started PLAYING at this point, store the event on the pad instead
   * force pushing right now to avoid blocking thread until next forward */
  if (gst_pad_store_sticky_event (pad, event) == GST_FLOW_OK)
    GST_DEBUG_OBJECT (self, "Stored sticky event %p on pad %" GST_PTR_FORMAT, event, pad);
  else
    GST_WARNING_OBJECT (self, "Could not store event %p on pad %" GST_PTR_FORMAT, event, pad);
}

static void
gst_gtuber_bin_push_all_events (GstGtuberBin *self, GstPad *pad)
{
  GST_GTUBER_BIN_LOCK (self);

  if (self->tag_event) {
    GstEvent *event;
    GstMessage *msg;
    GstTagList *tags = NULL;

    GST_DEBUG_OBJECT (self, "Pushing TAG event: %p", self->tag_event);

    gst_event_parse_tag (self->tag_event, &tags);

    msg = gst_message_new_tag (GST_OBJECT (self), gst_tag_list_copy (tags));
    event = gst_event_new_sink_message (GST_GTUBER_TAGS, msg);

    gst_gtuber_bin_push_event (self, pad, event);

    gst_message_unref (msg);
    gst_event_unref (event);
  }
  if (self->toc_event) {
    GstEvent *event;
    GstMessage *msg;
    GstToc *toc = NULL;
    gboolean updated = FALSE;

    GST_DEBUG_OBJECT (self, "Pushing TOC event: %p", self->toc_event);

    gst_event_parse_toc (self->toc_event, &toc, &updated);

    msg = gst_message_new_toc (GST_OBJECT (self), toc, updated);
    event = gst_event_new_sink_message (GST_GTUBER_TOC, msg);

    gst_gtuber_bin_push_event (self, pad, event);

    gst_toc_unref (toc);
    gst_message_unref (msg);
    gst_event_unref (event);
  }

  GST_GTUBER_BIN_UNLOCK (self);
}

static void
gst_gtuber_bin_push_all_events_on_all_pads (GstGtuberBin *self)
{
  GstIterator *iter;
  GValue value = G_VALUE_INIT;

  iter = gst_element_iterate_src_pads (GST_ELEMENT (self));

  while (gst_iterator_next (iter, &value) == GST_ITERATOR_OK) {
    GstPad *my_pad = g_value_get_object (&value);

    GST_GTUBER_BIN_LOCK (self);

    if (self->tag_event)
      gst_gtuber_bin_push_event (self, my_pad, self->tag_event);
    if (self->toc_event)
      gst_gtuber_bin_push_event (self, my_pad, self->toc_event);

    GST_GTUBER_BIN_UNLOCK (self);

    g_value_unset (&value);
  }
  gst_iterator_free (iter);
}

static gboolean
remove_sometimes_pad_cb (GstElement *element, GstPad *pad, gpointer user_data)
{
  GstPadTemplate *template = gst_pad_get_pad_template (pad);
  GstPadPresence presence = GST_PAD_TEMPLATE_PRESENCE (template);

  gst_object_unref (template);

  if (presence == GST_PAD_SOMETIMES) {
    if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) >= GST_LEVEL_DEBUG) {
      gchar *pad_name = gst_object_get_name (GST_OBJECT (pad));

      GST_DEBUG ("Removing pad \"%s\"", pad_name);
      g_free (pad_name);
    }

    gst_pad_set_active (pad, FALSE);

    if (G_UNLIKELY (!gst_element_remove_pad (element, pad)))
      g_critical ("Failed to remove pad from bin");
  }

  return TRUE;
}

static void
gst_gtuber_bin_reset (GstGtuberBin *self)
{
  GstElement *element = GST_ELEMENT_CAST (self);

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

  gst_element_foreach_pad (element, remove_sometimes_pad_cb, NULL);
}

static GstStateChangeReturn
gst_gtuber_bin_change_state (GstElement *element, GstStateChange transition)
{
  GstGtuberBin *self = GST_GTUBER_BIN_CAST (element);
  GstStateChangeReturn ret;

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  switch (transition) {
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      if (!self->started) {
        gst_gtuber_bin_push_all_events_on_all_pads (self);
        self->started = TRUE;
      }
      break;
    case GST_STATE_CHANGE_PAUSED_TO_READY:
      gst_gtuber_bin_reset (self);
      self->started = FALSE;
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

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_TAG:
      GST_DEBUG_OBJECT (self, "Received TAG event: %p", event);
      GST_GTUBER_BIN_LOCK (self);

      if (self->tag_event)
        gst_event_unref (self->tag_event);
      self->tag_event = gst_event_ref (event);

      GST_GTUBER_BIN_UNLOCK (self);
      break;
    case GST_EVENT_TOC:
      GST_DEBUG_OBJECT (self, "Received TOC event: %p", event);
      GST_GTUBER_BIN_LOCK (self);

      if (self->toc_event)
        gst_event_unref (self->toc_event);
      self->toc_event = gst_event_ref (event);

      GST_GTUBER_BIN_UNLOCK (self);
      break;
    case GST_EVENT_CUSTOM_DOWNSTREAM_STICKY:{
      const GstStructure *structure = gst_event_get_structure (event);

      if (gst_structure_has_name (structure, GST_GTUBER_HEADERS)) {
        GST_DEBUG_OBJECT (self, "Received " GST_GTUBER_HEADERS " event");
        GST_GTUBER_BIN_LOCK (self);

        gst_clear_structure (&self->http_headers);
        self->http_headers = gst_structure_copy (structure);

        GST_GTUBER_BIN_UNLOCK (self);
      }
      break;
    }
    default:
      break;
  }

  return gst_pad_event_default (pad, parent, event);
}

static void
gst_gtuber_bin_pad_added (GstElement *element, GstPad *pad)
{
  if (GST_PAD_IS_SRC (pad))
    gst_gtuber_bin_push_all_events (GST_GTUBER_BIN_CAST (element), pad);

  GST_CALL_PARENT (GST_ELEMENT_CLASS, pad_added, (element, pad));
}

static void
gst_gtuber_bin_class_init (GstGtuberBinClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBinClass *gstbin_class = (GstBinClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_gtuber_bin_debug, "gtuberbin", 0, "Gtuber Bin");

  gobject_class->finalize = gst_gtuber_bin_finalize;
  gstbin_class->deep_element_added = gst_gtuber_bin_deep_element_added;
  gstelement_class->pad_added = gst_gtuber_bin_pad_added;
  gstelement_class->change_state = gst_gtuber_bin_change_state;
}
