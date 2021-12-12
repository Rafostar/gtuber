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

#define DEFAULT_CONNECTION_SPEED 0
#define DEFAULT_INITIAL_BITRATE  1600

enum
{
  PROP_0,
  PROP_CONNECTION_SPEED,
  PROP_INITIAL_BITRATE,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

GST_DEBUG_CATEGORY_STATIC (gst_gtuber_bin_debug);
#define GST_CAT_DEFAULT gst_gtuber_bin_debug

#define gst_gtuber_bin_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGtuberBin, gst_gtuber_bin, GST_TYPE_BIN, NULL);

/* GObject */
static void gst_gtuber_bin_constructed (GObject* object);
static void gst_gtuber_bin_finalize (GObject *object);
static void gst_gtuber_bin_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec);
static void gst_gtuber_bin_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec);

/* GstPad */
static gboolean gst_gtuber_bin_sink_event (GstPad *pad, GstObject *parent,
    GstEvent *event);

/* GstElement */
static GstStateChangeReturn gst_gtuber_bin_change_state (
    GstElement *element, GstStateChange transition);
static void demuxer_pad_added_cb (GstElement *element,
    GstPad *pad, GstGtuberBin *self);

/* GstBin */
static void gst_gtuber_bin_deep_element_added (GstBin *bin,
    GstBin *sub_bin, GstElement *child);

static void
gst_gtuber_bin_class_init (GstGtuberBinClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBinClass *gstbin_class = (GstBinClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_gtuber_bin_debug, "gtuber_bin", 0, "Gtuber Bin");

  gobject_class->constructed = gst_gtuber_bin_constructed;
  gobject_class->finalize = gst_gtuber_bin_finalize;
  gobject_class->set_property = gst_gtuber_bin_set_property;
  gobject_class->get_property = gst_gtuber_bin_get_property;

  param_specs[PROP_CONNECTION_SPEED] = g_param_spec_uint ("connection-speed",
      "Connection Speed", "Network connection speed in kbps (0 = auto)",
       0, G_MAXUINT, DEFAULT_CONNECTION_SPEED,
       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_INITIAL_BITRATE] = g_param_spec_uint ("initial-bitrate",
      "Initial Bitrate", "Initial startup bitrate in kbps (0 = same as connection-speed)",
       0, G_MAXUINT, DEFAULT_INITIAL_BITRATE,
       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  gstbin_class->deep_element_added = gst_gtuber_bin_deep_element_added;
  gstelement_class->change_state = gst_gtuber_bin_change_state;
}

static void
gst_gtuber_bin_init (GstGtuberBin *self)
{
  g_mutex_init (&self->prop_lock);

  self->connection_speed = DEFAULT_CONNECTION_SPEED;
  self->initial_bitrate = DEFAULT_INITIAL_BITRATE;
}

static void
gst_gtuber_bin_constructed (GObject* object)
{
  GstGtuberBin *self = GST_GTUBER_BIN (object);
  GstPad *pad, *ghostpad;

  gst_bin_add (GST_BIN (self), self->demuxer);
  gst_element_set_locked_state (self->demuxer, TRUE);

  /* Create sink ghost pad */
  pad = gst_element_get_static_pad (self->demuxer, "sink");
  ghostpad = gst_ghost_pad_new_from_template ("sink", pad,
      gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (self), "sink"));
  gst_object_unref (pad);
  gst_pad_set_event_function (ghostpad,
      GST_DEBUG_FUNCPTR (gst_gtuber_bin_sink_event));

  gst_pad_set_active (ghostpad, TRUE);

  if (!gst_element_add_pad (GST_ELEMENT (self), ghostpad))
    g_critical ("Failed to add sink pad to bin");

  g_signal_connect (self->demuxer, "pad-added",
      (GCallback) demuxer_pad_added_cb, self);
}

static void
gst_gtuber_bin_finalize (GObject *object)
{
  GstGtuberBin *self = GST_GTUBER_BIN (object);

  GST_DEBUG ("Finalize");

  gst_clear_structure (&self->gtuber_config);
  g_mutex_clear (&self->prop_lock);

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
gst_gtuber_bin_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  GstGtuberBin *self = GST_GTUBER_BIN (object);

  g_mutex_lock (&self->prop_lock);

  switch (prop_id) {
    case PROP_CONNECTION_SPEED:
      self->connection_speed = g_value_get_uint (value);
      g_object_set (self->demuxer,
          "connection-speed", self->connection_speed, NULL);
      break;
    case PROP_INITIAL_BITRATE:
      self->initial_bitrate = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  g_mutex_unlock (&self->prop_lock);
}

static void
gst_gtuber_bin_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  GstGtuberBin *self = GST_GTUBER_BIN (object);

  g_mutex_lock (&self->prop_lock);

  switch (prop_id) {
    case PROP_CONNECTION_SPEED:
      g_value_set_uint (value, self->connection_speed);
      break;
    case PROP_INITIAL_BITRATE:
      g_value_set_uint (value, self->initial_bitrate);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  g_mutex_unlock (&self->prop_lock);
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
  /* First call parent to let user do configuration, as we have to
   * overwrite some of the props it may set to what website needs */
  GST_BIN_CLASS (parent_class)->deep_element_added (bin, sub_bin, child);

  if (GST_OBJECT_FLAG_IS_SET (child, GST_ELEMENT_FLAG_SOURCE)) {
    GstGtuberBin *self = GST_GTUBER_BIN (bin);

    if (self->gtuber_config) {
      gst_structure_foreach (self->gtuber_config,
          (GstStructureForeachFunc) configure_deep_element, child);
    }
  }
}

static gboolean
has_ghostpad_for_pad (GstGtuberBin *self, const gchar *pad_name, GstPad **ghostpad)
{
  GstIterator *iter;
  GValue value = { 0, };
  gboolean has_ghostpad = FALSE;

  iter = gst_element_iterate_src_pads (GST_ELEMENT (self));

  while (gst_iterator_next (iter, &value) == GST_ITERATOR_OK) {
    GstPad *my_pad;
    gchar *name, **strv;

    my_pad = g_value_get_object (&value);
    name = gst_object_get_name (GST_OBJECT (my_pad));
    strv = g_strsplit (name, "_", 2);

    has_ghostpad = g_str_has_prefix (pad_name, strv[0]);

    g_value_unset (&value);
    g_free (name);
    g_strfreev (strv);

    if (has_ghostpad) {
      if (ghostpad)
        *ghostpad = my_pad;
      break;
    }
  }
  gst_iterator_free (iter);

  return has_ghostpad;
}

static void
demuxer_pad_added_cb (GstElement *element, GstPad *pad, GstGtuberBin *self)
{
  GstPad *ghostpad = NULL;
  gchar *pad_name;

  if (gst_pad_get_direction (pad) != GST_PAD_SRC)
    return;

  pad_name = gst_object_get_name (GST_OBJECT (pad));

  if (has_ghostpad_for_pad (self, pad_name, &ghostpad)) {
    GST_DEBUG ("Changing ghostpad target to \"%s\"", pad_name);

    gst_pad_set_active (ghostpad, FALSE);
    gst_ghost_pad_set_target (GST_GHOST_PAD (ghostpad), pad);
    gst_pad_set_active (ghostpad, TRUE);
  } else {
    GstPadTemplate *template;

    GST_DEBUG ("Adding src ghostpad for \"%s\"", pad_name);

    template = gst_pad_get_pad_template (pad);
    ghostpad = gst_ghost_pad_new_from_template (pad_name, pad,
        gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (self),
            GST_PAD_TEMPLATE_NAME_TEMPLATE (template)));
    gst_object_unref (template);

    gst_pad_set_active (ghostpad, TRUE);

    if (!gst_element_add_pad (GST_ELEMENT (self), ghostpad))
      g_critical ("Failed to add source pad to bin");
  }

  g_free (pad_name);
}

static gboolean
gst_gtuber_bin_prepare (GstGtuberBin *self)
{
  GST_DEBUG ("Preparing");

  gst_element_set_locked_state (self->demuxer, FALSE);
  if (!gst_element_sync_state_with_parent (self->demuxer))
    goto error_sync_state;

  GST_DEBUG ("Prepared");

  return TRUE;

error_sync_state:
  GST_ELEMENT_ERROR (self, CORE, STATE_CHANGE,
      ("Failed to sync state"), (NULL));
  return FALSE;
}

static void
gst_gtuber_bin_configure (GstGtuberBin *self)
{
  guint initial_speed;

  GST_DEBUG ("Configuring");

  g_mutex_lock (&self->prop_lock);

  initial_speed = (self->initial_bitrate > 0)
      ? self->initial_bitrate
      : self->connection_speed;
  self->needs_playback_config = TRUE;

  g_mutex_unlock (&self->prop_lock);

  g_object_set (self->demuxer, "connection-speed", initial_speed, NULL);

  GST_DEBUG ("Configured");
}

static void
gst_gtuber_bin_playback_configure (GstGtuberBin *self)
{
  guint connection_speed;

  g_mutex_lock (&self->prop_lock);

  if (!self->needs_playback_config) {
    g_mutex_unlock (&self->prop_lock);
    return;
  }

  GST_DEBUG ("Configuring playback");

  connection_speed = self->connection_speed;
  self->needs_playback_config = FALSE;

  g_mutex_unlock (&self->prop_lock);

  g_object_set (self->demuxer, "connection-speed", connection_speed, NULL);

  GST_DEBUG ("Configured playback");
}

static GstStateChangeReturn
gst_gtuber_bin_change_state (GstElement *element, GstStateChange transition)
{
  GstGtuberBin *self;
  GstStateChangeReturn ret;

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  self = GST_GTUBER_BIN (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_gtuber_bin_prepare (self))
        return GST_STATE_CHANGE_FAILURE;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_gtuber_bin_configure (self);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      gst_gtuber_bin_playback_configure (self);
      break;
    default:
      break;
  }

  return ret;
}

static gboolean
gst_gtuber_bin_sink_event (GstPad *pad, GstObject *parent, GstEvent *event)
{
  gboolean res = FALSE;

  switch (event->type) {
    case GST_EVENT_CUSTOM_DOWNSTREAM_STICKY:{
      const GstStructure *structure = gst_event_get_structure (event);

      if (gst_structure_has_name (structure, GST_GTUBER_CONFIG)) {
        GstGtuberBin *self = GST_GTUBER_BIN (parent);

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
