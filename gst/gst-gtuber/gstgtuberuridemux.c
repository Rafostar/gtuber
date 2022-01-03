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

#include "gstgtuberuridemux.h"
#include "gstgtuberelement.h"

GST_DEBUG_CATEGORY_STATIC (gst_gtuber_uri_demux_debug);
#define GST_CAT_DEFAULT gst_gtuber_uri_demux_debug

static GstStaticPadTemplate sink_template = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("text/uri-list, source=(string)gtuber"));

static GstStaticPadTemplate src_template = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_SOMETIMES,
    GST_STATIC_CAPS_ANY);

#define gst_gtuber_uri_demux_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGtuberUriDemux, gst_gtuber_uri_demux,
    GST_TYPE_GTUBER_BIN, NULL);
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (gtuberuridemux, "gtuberuridemux",
    GST_RANK_PRIMARY + 1, GST_TYPE_GTUBER_URI_DEMUX, gst_gtuber_element_init (plugin));

/* GObject */
static void gst_gtuber_uri_demux_finalize (GObject *object);

/* GstPad */
static gboolean gst_gtuber_uri_demux_sink_event (GstPad *pad,
    GstObject *parent, GstEvent *event);
static GstFlowReturn gst_gtuber_uri_demux_sink_chain (GstPad *pad,
    GstObject *parent, GstBuffer *buffer);

static void
gst_gtuber_uri_demux_class_init (GstGtuberUriDemuxClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_gtuber_uri_demux_debug, "gtuberuridemux", 0,
      "Gtuber URI demux");

  gobject_class->finalize = gst_gtuber_uri_demux_finalize;

  gst_element_class_add_static_pad_template (gstelement_class, &sink_template);
  gst_element_class_add_static_pad_template (gstelement_class, &src_template);

  gst_element_class_set_static_metadata (gstelement_class, "Gtuber URI demuxer",
      "Codec/Demuxer",
      "Demuxer for Gtuber stream URI",
      "Rafał Dzięgiel <rafostar.github@gmail.com>");
}

static void
gst_gtuber_uri_demux_init (GstGtuberUriDemux *self)
{
  GstPad *sink_pad;

  self->input_adapter = gst_adapter_new ();

  sink_pad = gst_pad_new_from_template (gst_element_class_get_pad_template (
      GST_ELEMENT_GET_CLASS (self), "sink"), "sink");
  gst_pad_set_event_function (sink_pad,
      GST_DEBUG_FUNCPTR (gst_gtuber_uri_demux_sink_event));
  gst_pad_set_chain_function (sink_pad,
      GST_DEBUG_FUNCPTR (gst_gtuber_uri_demux_sink_chain));

  gst_pad_set_active (sink_pad, TRUE);

  if (!gst_element_add_pad (GST_ELEMENT (self), sink_pad))
    g_critical ("Failed to add sink pad to bin");
}

static void
gst_gtuber_uri_demux_finalize (GObject *object)
{
  GstGtuberUriDemux *self = GST_GTUBER_URI_DEMUX (object);

  GST_DEBUG ("Finalize");

  g_object_unref (self->input_adapter);

  if (self->typefind_src)
    g_object_unref (self->typefind_src);

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static gboolean
gst_gtuber_uri_demux_process_buffer (GstGtuberUriDemux *self, GstBuffer *buffer)
{
  GstMemory *mem;
  GstMapInfo info;

  mem = gst_buffer_peek_memory (buffer, 0);

  if (mem && gst_memory_map (mem, &info, GST_MAP_READ)) {
    GstPad *uri_handler_src, *typefind_sink, *src_ghostpad;
    GstPadLinkReturn pad_link_ret;

    GST_DEBUG ("Stream URI: %s", info.data);

    if (self->uri_handler) {
      GST_DEBUG ("Trying to reuse existing URI handler");

      if (gst_uri_handler_set_uri (GST_URI_HANDLER (self->uri_handler),
          (gchar *) info.data, NULL)) {
        GST_DEBUG ("Reused existing URI handler");
      } else {
        GST_DEBUG ("Could not reuse existing URI handler");

        if (self->typefind_src) {
          gst_element_remove_pad (GST_ELEMENT (self), self->typefind_src);
          g_clear_object (&self->typefind_src);
        }

        gst_bin_remove (GST_BIN_CAST (self), self->uri_handler);
        gst_bin_remove (GST_BIN_CAST (self), self->typefind);

        self->uri_handler = NULL;
        self->typefind = NULL;
      }
    }

    if (!self->uri_handler) {
      GST_DEBUG ("Creating new URI handler element");

      self->uri_handler = gst_element_make_from_uri (GST_URI_SRC,
          (gchar *) info.data, NULL, NULL);

      if (G_UNLIKELY (!self->uri_handler)) {
        GST_ERROR ("Could not create URI handler element");

        GST_ELEMENT_ERROR (self, CORE, MISSING_PLUGIN,
            ("Missing plugin to handle URI: %s", info.data), (NULL));
        gst_memory_unmap (mem, &info);

        return FALSE;
      }

      gst_uri_handler_set_uri (GST_URI_HANDLER (self->uri_handler),
          (gchar *) info.data, NULL);
      gst_bin_add (GST_BIN_CAST (self), self->uri_handler);

      self->typefind = gst_element_factory_make ("typefind", NULL);
      gst_bin_add (GST_BIN_CAST (self), self->typefind);

      uri_handler_src = gst_element_get_static_pad (self->uri_handler, "src");
      typefind_sink = gst_element_get_static_pad (self->typefind, "sink");

      pad_link_ret = gst_pad_link_full (uri_handler_src, typefind_sink,
          GST_PAD_LINK_CHECK_NOTHING);

      if (pad_link_ret != GST_PAD_LINK_OK)
        g_critical ("Failed to link bin elements");

      g_object_unref (uri_handler_src);
      g_object_unref (typefind_sink);

      self->typefind_src = gst_element_get_static_pad (self->typefind, "src");

      src_ghostpad = gst_ghost_pad_new_from_template ("src", self->typefind_src,
          gst_element_class_get_pad_template (GST_ELEMENT_GET_CLASS (self), "src"));

      gst_pad_set_active (src_ghostpad, TRUE);

      if (!gst_element_add_pad (GST_ELEMENT (self), src_ghostpad))
        g_critical ("Failed to add source pad to bin");
    }

    gst_memory_unmap (mem, &info);

    gst_element_sync_state_with_parent (self->typefind);
    gst_element_sync_state_with_parent (self->uri_handler);
  }

  return TRUE;
}

static gboolean
gst_gtuber_uri_demux_sink_event (GstPad *pad, GstObject *parent, GstEvent *event)
{
  GstGtuberUriDemux *self = GST_GTUBER_URI_DEMUX_CAST (parent);

  switch (event->type) {
    case GST_EVENT_EOS:{
      GstBuffer *buffer;
      gsize available;
      gboolean success;

      available = gst_adapter_available (self->input_adapter);

      if (available == 0) {
        GST_WARNING_OBJECT (self, "Received EOS without URI data");
        break;
      }

      buffer = gst_adapter_take_buffer (self->input_adapter, available);
      success = gst_gtuber_uri_demux_process_buffer (self, buffer);
      gst_buffer_unref (buffer);

      if (success) {
        gst_event_unref (event);
        return TRUE;
      }
      break;
    }
    default:
      break;
  }

  return gst_gtuber_bin_sink_event (pad, parent, event);
}

static GstFlowReturn
gst_gtuber_uri_demux_sink_chain (GstPad *pad, GstObject *parent, GstBuffer *buffer)
{
  GstGtuberUriDemux *self = GST_GTUBER_URI_DEMUX_CAST (parent);

  gst_adapter_push (self->input_adapter, buffer);
  GST_DEBUG_OBJECT (self, "Received data buffer, total size is %i bytes",
      (gint) gst_adapter_available (self->input_adapter));

  return GST_FLOW_OK;
}
