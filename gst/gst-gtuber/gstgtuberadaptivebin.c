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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "gstgtuberadaptivebin.h"
#include "gstgtuberelement.h"

#define DEFAULT_INITIAL_BITRATE  1600
#define DEFAULT_TARGET_BITRATE   0

enum
{
  PROP_0,
  PROP_INITIAL_BITRATE,
  PROP_TARGET_BITRATE,
  PROP_LAST
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

GST_DEBUG_CATEGORY_STATIC (gst_gtuber_adaptive_bin_debug);
#define GST_CAT_DEFAULT gst_gtuber_adaptive_bin_debug

#define gst_gtuber_adaptive_bin_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGtuberAdaptiveBin,
    gst_gtuber_adaptive_bin, GST_TYPE_GTUBER_BIN, NULL);

/* GObject */
static void gst_gtuber_adaptive_bin_constructed (GObject* object);
static void gst_gtuber_adaptive_bin_set_property (GObject *object,
    guint prop_id, const GValue *value, GParamSpec *pspec);
static void gst_gtuber_adaptive_bin_get_property (GObject *object,
    guint prop_id, GValue *value, GParamSpec *pspec);

/* GstElement */
static GstStateChangeReturn gst_gtuber_adaptive_bin_change_state (
    GstElement *element, GstStateChange transition);
static void demuxer_pad_added_cb (GstElement *element,
    GstPad *pad, GstGtuberAdaptiveBin *self);

static void
gst_gtuber_adaptive_bin_class_init (GstGtuberAdaptiveBinClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_gtuber_adaptive_bin_debug, "gtuberadaptivebin", 0,
      "Gtuber Adaptive Bin");

  gobject_class->constructed = gst_gtuber_adaptive_bin_constructed;
  gobject_class->set_property = gst_gtuber_adaptive_bin_set_property;
  gobject_class->get_property = gst_gtuber_adaptive_bin_get_property;

  param_specs[PROP_INITIAL_BITRATE] = g_param_spec_uint ("initial-bitrate",
      "Initial Bitrate", "Initial startup bitrate in kbps (0 = same as target-bitrate)",
       0, G_MAXUINT, DEFAULT_INITIAL_BITRATE,
       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_TARGET_BITRATE] = g_param_spec_uint ("target-bitrate",
      "Target Bitrate", "Target playback bitrate in kbps (0 = auto, based on download speed)",
       0, G_MAXUINT, DEFAULT_TARGET_BITRATE,
       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  gstelement_class->change_state = gst_gtuber_adaptive_bin_change_state;
}

static void
gst_gtuber_adaptive_bin_init (GstGtuberAdaptiveBin *self)
{
  self->initial_bitrate = DEFAULT_INITIAL_BITRATE;
  self->target_bitrate = DEFAULT_TARGET_BITRATE;
}

static void
gst_gtuber_adaptive_bin_constructed (GObject* object)
{
  GstGtuberAdaptiveBin *self = GST_GTUBER_ADAPTIVE_BIN (object);
  GstPad *pad, *ghostpad;

  GST_STATE_LOCK (self);

  gst_element_set_locked_state (self->demuxer, TRUE);
  gst_bin_add (GST_BIN (self), self->demuxer);

  GST_STATE_UNLOCK (self);

  g_object_set (self->demuxer, "bitrate-limit", 1.0f, NULL);

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
gst_gtuber_adaptive_bin_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  GstGtuberAdaptiveBin *self = GST_GTUBER_ADAPTIVE_BIN (object);

  GST_GTUBER_BIN_PROP_LOCK (self);

  switch (prop_id) {
    case PROP_INITIAL_BITRATE:
      self->initial_bitrate = g_value_get_uint (value);
      break;
    case PROP_TARGET_BITRATE:
      self->target_bitrate = g_value_get_uint (value);
      g_object_set (self->demuxer,
          "connection-speed", self->target_bitrate, NULL);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_GTUBER_BIN_PROP_UNLOCK (self);
}

static void
gst_gtuber_adaptive_bin_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  GstGtuberAdaptiveBin *self = GST_GTUBER_ADAPTIVE_BIN (object);

  GST_GTUBER_BIN_PROP_LOCK (self);

  switch (prop_id) {
    case PROP_INITIAL_BITRATE:
      g_value_set_uint (value, self->initial_bitrate);
      break;
    case PROP_TARGET_BITRATE:
      g_value_set_uint (value, self->target_bitrate);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  GST_GTUBER_BIN_PROP_UNLOCK (self);
}

static gboolean
has_ghostpad_for_pad (GstGtuberAdaptiveBin *self, const gchar *pad_name, GstPad **ghostpad)
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
demuxer_pad_added_cb (GstElement *element, GstPad *pad, GstGtuberAdaptiveBin *self)
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
gst_gtuber_adaptive_bin_prepare (GstGtuberAdaptiveBin *self)
{
  GST_DEBUG ("Preparing");
  GST_STATE_LOCK (self);

  gst_element_set_locked_state (self->demuxer, FALSE);

  if (!gst_element_sync_state_with_parent (self->demuxer))
    goto error_sync_state;

  GST_DEBUG ("Prepared");
  GST_STATE_UNLOCK (self);

  return TRUE;

error_sync_state:
  GST_STATE_UNLOCK (self);
  GST_ELEMENT_ERROR (self, CORE, STATE_CHANGE,
      ("Failed to sync state"), (NULL));
  return FALSE;
}

static void
gst_gtuber_adaptive_bin_configure (GstGtuberAdaptiveBin *self)
{
  guint initial_bitrate;

  GST_DEBUG ("Configuring");

  GST_GTUBER_BIN_LOCK (self);
  self->needs_playback_config = TRUE;
  GST_GTUBER_BIN_UNLOCK (self);

  GST_GTUBER_BIN_PROP_LOCK (self);

  initial_bitrate = (self->initial_bitrate > 0)
      ? self->initial_bitrate
      : self->target_bitrate;

  GST_GTUBER_BIN_PROP_UNLOCK (self);

  g_object_set (self->demuxer,
      "connection-speed", initial_bitrate, NULL);

  GST_DEBUG ("Configured");
}

static void
gst_gtuber_adaptive_bin_playback_configure (GstGtuberAdaptiveBin *self)
{
  guint target_bitrate;

  GST_GTUBER_BIN_LOCK (self);

  if (!self->needs_playback_config) {
    GST_GTUBER_BIN_UNLOCK (self);
    return;
  }

  self->needs_playback_config = FALSE;
  GST_GTUBER_BIN_UNLOCK (self);

  GST_DEBUG ("Configuring playback");

  GST_GTUBER_BIN_PROP_LOCK (self);
  target_bitrate = self->target_bitrate;
  GST_GTUBER_BIN_PROP_UNLOCK (self);

  g_object_set (self->demuxer,
      "connection-speed", target_bitrate, NULL);

  GST_DEBUG ("Configured playback");
}

static GstStateChangeReturn
gst_gtuber_adaptive_bin_change_state (GstElement *element, GstStateChange transition)
{
  GstGtuberAdaptiveBin *self;
  GstStateChangeReturn ret;

  ret = GST_ELEMENT_CLASS (parent_class)->change_state (element, transition);
  if (ret == GST_STATE_CHANGE_FAILURE)
    return ret;

  self = GST_GTUBER_ADAPTIVE_BIN_CAST (element);

  switch (transition) {
    case GST_STATE_CHANGE_NULL_TO_READY:
      if (!gst_gtuber_adaptive_bin_prepare (self))
        return GST_STATE_CHANGE_FAILURE;
      break;
    case GST_STATE_CHANGE_READY_TO_PAUSED:
      gst_gtuber_adaptive_bin_configure (self);
      break;
    case GST_STATE_CHANGE_PAUSED_TO_PLAYING:
      gst_gtuber_adaptive_bin_playback_configure (self);
      break;
    default:
      break;
  }

  return ret;
}
