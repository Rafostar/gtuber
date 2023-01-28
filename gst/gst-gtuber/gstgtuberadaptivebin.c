/*
 * Copyright (C) 2022 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#define GST_CAT_DEFAULT gst_gtuber_adaptive_bin_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

#define parent_class gst_gtuber_adaptive_bin_parent_class
G_DEFINE_TYPE_WITH_CODE (GstGtuberAdaptiveBin,
    gst_gtuber_adaptive_bin, GST_TYPE_GTUBER_BIN, NULL);

static void
gst_gtuber_adaptive_bin_init (GstGtuberAdaptiveBin *self)
{
  self->initial_bitrate = DEFAULT_INITIAL_BITRATE;
  self->target_bitrate = DEFAULT_TARGET_BITRATE;

  GST_OBJECT_FLAG_SET (self, GST_BIN_FLAG_STREAMS_AWARE);
}

static void
gst_gtuber_adaptive_bin_constructed (GObject* object)
{
  GstGtuberAdaptiveBin *self = GST_GTUBER_ADAPTIVE_BIN (object);

  self->sink_ghostpad = gst_ghost_pad_new_no_target_from_template ("sink",
      gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (self), "sink"));
  gst_pad_set_event_function (self->sink_ghostpad,
      GST_DEBUG_FUNCPTR (gst_gtuber_bin_sink_event));

  if (G_UNLIKELY (!gst_element_add_pad (GST_ELEMENT (self), self->sink_ghostpad)))
    g_critical ("Failed to add sink pad to bin");

  GST_CALL_PARENT (G_OBJECT_CLASS, constructed, (object));
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
has_ghostpad_for_pad (GstGtuberAdaptiveBin *self, GstPad *src_pad,
    const gchar *pad_name, GstPad **ghostpad)
{
  GstIterator *iter;
  GValue value = G_VALUE_INIT;
  gboolean has_ghostpad = FALSE;

  iter = gst_element_iterate_src_pads (GST_ELEMENT (self));

  while (gst_iterator_next (iter, &value) == GST_ITERATOR_OK) {
    GstPad *my_pad;
    gchar *name, **strv;

    my_pad = g_value_get_object (&value);
    name = gst_object_get_name (GST_OBJECT (my_pad));
    strv = g_strsplit (name, "_", 2);

    /* On similiar name, check caps compatibility */
    if (g_str_has_prefix (pad_name, strv[0])) {
      GstCaps *my_caps, *his_caps;

      my_caps = gst_pad_get_current_caps (my_pad);
      his_caps = gst_pad_get_current_caps (src_pad);

      if ((has_ghostpad = (my_caps != NULL
          && his_caps != NULL
          && gst_caps_is_always_compatible (my_caps, his_caps))))
        GST_DEBUG ("Found ghostpad \"%s\" for pad \"%s\"", name, pad_name);

      gst_clear_caps (&my_caps);
      gst_clear_caps (&his_caps);
    }

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
demuxer_pad_link_with_ghostpad (GstPad *pad, GstGtuberAdaptiveBin *self)
{
  GstPad *ghostpad = NULL;
  gchar *pad_name;

  pad_name = gst_object_get_name (GST_OBJECT (pad));
  GST_DEBUG ("Demuxer has source pad \"%s\"", pad_name);

  if (has_ghostpad_for_pad (self, pad, pad_name, &ghostpad)) {
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

    if (G_UNLIKELY (!gst_element_add_pad (GST_ELEMENT (self), ghostpad)))
      g_critical ("Failed to add source pad to bin");
  }

  g_free (pad_name);
}

static void
link_demuxer_pads (GstGtuberAdaptiveBin *self)
{
  GstIterator *iter;
  GValue value = G_VALUE_INIT;

  iter = gst_element_iterate_src_pads (self->demuxer);
  GST_DEBUG_OBJECT (self, "Linking demuxer pads with ghostpads");

  while (gst_iterator_next (iter, &value) == GST_ITERATOR_OK) {
    GstPad *demuxer_pad;

    demuxer_pad = g_value_get_object (&value);
    if (!gst_pad_is_linked (demuxer_pad))
      demuxer_pad_link_with_ghostpad (demuxer_pad, self);

    g_value_unset (&value);
  }
  gst_iterator_free (iter);
}

static void
demuxer_no_more_pads_cb (GstElement *element, GstGtuberAdaptiveBin *self)
{
  link_demuxer_pads (self);

  GST_DEBUG_OBJECT (self, "Signalling \"no more pads\"");
  gst_element_no_more_pads (GST_ELEMENT (self));
}

static gboolean
is_parent_streams_aware (GstGtuberAdaptiveBin *self)
{
  gboolean ret = FALSE;
  GstObject *parent = gst_object_get_parent (GST_OBJECT_CAST (self));

  if (parent) {
    ret = GST_OBJECT_FLAG_IS_SET (parent, GST_BIN_FLAG_STREAMS_AWARE);
    gst_object_unref (parent);
  }

  return ret;
}

static gboolean
caps_handle_media_type (GstCaps *pad_caps, const gchar *media_type)
{
  GstCaps *search_caps = gst_caps_new_empty_simple (media_type);
  gboolean ret = gst_caps_is_always_compatible (pad_caps, search_caps);

  gst_caps_unref (search_caps);

  return ret;
}

static GstElement *
make_compatible_demuxer (GstGtuberAdaptiveBin *self)
{
  GstPad *pad;
  GstCaps *pad_caps;
  GstElement *demuxer = NULL;
  const gchar *demuxer_name;

  pad = gst_element_get_static_pad (GST_ELEMENT (self), "sink");
  pad_caps = gst_pad_get_allowed_caps (pad);
  gst_object_unref (pad);

  if (G_UNLIKELY (!pad_caps))
    return NULL;

  demuxer_name = (caps_handle_media_type (pad_caps, "application/dash+xml"))
      ? "dashdemux"
      : (caps_handle_media_type (pad_caps, "application/x-hls"))
      ? "hlsdemux"
      : NULL;

  gst_caps_unref (pad_caps);

  if (G_UNLIKELY (!demuxer_name))
    return NULL;

  if (is_parent_streams_aware (self)) {
    gchar *ad2_name;

    ad2_name = g_strjoin (NULL, demuxer_name, "2", NULL);
    demuxer = gst_element_factory_make (ad2_name, NULL);

    g_free (ad2_name);
  }

  if (!demuxer) {
    demuxer = gst_element_factory_make (demuxer_name, NULL);
    GST_OBJECT_FLAG_UNSET (self, GST_BIN_FLAG_STREAMS_AWARE);
  }

  if (demuxer)
    GST_INFO_OBJECT (self, "Made internal demuxer: %s", GST_ELEMENT_NAME (demuxer));

  return demuxer;
}

static gboolean
gst_gtuber_adaptive_bin_prepare (GstGtuberAdaptiveBin *self)
{
  GST_GTUBER_BIN_LOCK (self);

  if (self->prepared) {
    GST_GTUBER_BIN_UNLOCK (self);
    return TRUE;
  }

  self->prepared = TRUE;
  GST_GTUBER_BIN_UNLOCK (self);

  if ((self->demuxer = make_compatible_demuxer (self))) {
    GObjectClass *gobject_class = G_OBJECT_GET_CLASS (self->demuxer);
    GstPad *pad;

    if (g_object_class_find_property (gobject_class, "low-watermark-time"))
      g_object_set (self->demuxer, "low-watermark-time", 3 * GST_SECOND, NULL);

    gst_bin_add (GST_BIN (self), self->demuxer);

    if (!is_parent_streams_aware (self)) {
      g_signal_connect (self->demuxer, "no-more-pads",
          (GCallback) demuxer_no_more_pads_cb, self);
    }

    /* Link with sink ghost pad */
    pad = gst_element_get_static_pad (self->demuxer, "sink");
    if (!gst_ghost_pad_set_target (GST_GHOST_PAD (self->sink_ghostpad), pad))
      GST_ERROR_OBJECT (self, "Could not set sink ghostpad target");
    gst_object_unref (pad);

    gst_pad_set_active (self->sink_ghostpad, TRUE);
  }

  return (self->demuxer != NULL);
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

static void
gst_gtuber_adaptive_bin_handle_message (GstBin *bin, GstMessage *message)
{
  switch (message->type) {
    case GST_MESSAGE_STREAMS_SELECTED:
      link_demuxer_pads (GST_GTUBER_ADAPTIVE_BIN_CAST (bin));
      break;
    default:
      break;
  }

  GST_CALL_PARENT (GST_BIN_CLASS, handle_message, (bin, message));
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
        ret = GST_STATE_CHANGE_FAILURE;
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

static void
gst_gtuber_adaptive_bin_class_init (GstGtuberAdaptiveBinClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstBinClass *gstbin_class = (GstBinClass *) klass;
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

  gstbin_class->handle_message = gst_gtuber_adaptive_bin_handle_message;

  gstelement_class->change_state = gst_gtuber_adaptive_bin_change_state;
}
