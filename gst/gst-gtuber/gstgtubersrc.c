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

#include <gtuber/gtuber.h>

#include "gstgtubersrc.h"
#include "gstgtuberelement.h"

GST_DEBUG_CATEGORY_STATIC (gst_gtuber_src_debug);
#define GST_CAT_DEFAULT gst_gtuber_src_debug

#define GST_GTUBER_HEADER_UA           "User-Agent"
#define GST_GTUBER_PROP_UA             "user-agent"
#define GST_GTUBER_PROP_EXTRA_HEADERS  "extra-headers"

#define DEFAULT_CODECS     GTUBER_CODEC_AVC | GTUBER_CODEC_MP4A
#define DEFAULT_MAX_HEIGHT 0
#define DEFAULT_MAX_FPS    0

enum
{
  PROP_0,
  PROP_LOCATION,
  PROP_CODECS,
  PROP_MAX_HEIGHT,
  PROP_MAX_FPS,
  PROP_ITAGS,
  PROP_LAST
};

static const gchar *hosts_blacklist[] = {
  "googlevideo.com",
  NULL
};

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

#define gst_gtuber_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGtuberSrc, gst_gtuber_src,
    GST_TYPE_PUSH_SRC, gst_gtuber_uri_handler_do_init (g_define_type_id));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (gtubersrc, "gtubersrc",
    GST_RANK_NONE, GST_TYPE_GTUBER_SRC, gst_gtuber_element_init (plugin));

/* GObject */
static void gst_gtuber_src_finalize (GObject *object);
static void gst_gtuber_src_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec);
static void gst_gtuber_src_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec);

/* GstBaseSrc */
static gboolean gst_gtuber_src_start (GstBaseSrc *base_src);
static gboolean gst_gtuber_src_stop (GstBaseSrc *base_src);
static gboolean gst_gtuber_src_get_size (GstBaseSrc *base_src,
    guint64 *size);
static gboolean gst_gtuber_src_is_seekable (GstBaseSrc *base_src);
static gboolean gst_gtuber_src_unlock (GstBaseSrc *base_src);
static gboolean gst_gtuber_src_unlock_stop (GstBaseSrc *base_src);
static gboolean gst_gtuber_src_query (GstBaseSrc *base_src,
    GstQuery *query);

/* GstPushSrc */
static GstFlowReturn gst_gtuber_src_create (GstPushSrc *push_src,
    GstBuffer **buf);

/* GstGtuberSrc */
static gboolean gst_gtuber_src_set_location (GstGtuberSrc *self,
    const gchar *uri, GError **error);
static void gst_gtuber_src_set_itags (GstGtuberSrc *self,
    const gchar *itags_str);

static void
gst_gtuber_src_class_init (GstGtuberSrcClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseSrcClass *gstbasesrc_class = (GstBaseSrcClass *) klass;
  GstPushSrcClass *gstpushsrc_class = (GstPushSrcClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_gtuber_src_debug, "gtuber_src", 0,
      "Gtuber source");

  gobject_class->finalize = gst_gtuber_src_finalize;
  gobject_class->set_property = gst_gtuber_src_set_property;
  gobject_class->get_property = gst_gtuber_src_get_property;

  param_specs[PROP_LOCATION] = g_param_spec_string ("location",
      "Location", "Media location", NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_CODECS] = g_param_spec_flags ("codecs",
      "Codecs", "Allowed media codecs (0 = all)",
      GTUBER_TYPE_CODEC_FLAGS, DEFAULT_CODECS,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_MAX_HEIGHT] = g_param_spec_uint64 ("max-height",
      "Maximal Height", "Maximal allowed video height in pixels (0 = unlimited)",
      0, G_MAXUINT64, DEFAULT_MAX_HEIGHT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_MAX_FPS] = g_param_spec_uint64 ("max-fps",
      "Maximal FPS", "Maximal allowed video framerate (0 = unlimited)",
       0, G_MAXUINT64, DEFAULT_MAX_FPS,
       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_ITAGS] = g_param_spec_string ("itags",
      "Itags", "A comma separated list of allowed itags", NULL,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);

  gst_element_class_add_static_pad_template (gstelement_class, &src_factory);

  gstbasesrc_class->start = GST_DEBUG_FUNCPTR (gst_gtuber_src_start);
  gstbasesrc_class->stop = GST_DEBUG_FUNCPTR (gst_gtuber_src_stop);
  gstbasesrc_class->get_size = GST_DEBUG_FUNCPTR (gst_gtuber_src_get_size);
  gstbasesrc_class->is_seekable = GST_DEBUG_FUNCPTR (gst_gtuber_src_is_seekable);
  gstbasesrc_class->unlock = GST_DEBUG_FUNCPTR (gst_gtuber_src_unlock);
  gstbasesrc_class->unlock_stop = GST_DEBUG_FUNCPTR (gst_gtuber_src_unlock_stop);
  gstbasesrc_class->query = GST_DEBUG_FUNCPTR (gst_gtuber_src_query);

  gstpushsrc_class->create = GST_DEBUG_FUNCPTR (gst_gtuber_src_create);

  gst_element_class_set_static_metadata (gstelement_class, "Gtuber source",
      "Source", "Source plugin that uses Gtuber API",
      "Rafał Dzięgiel <rafostar.github@gmail.com>");
}

static void
gst_gtuber_src_init (GstGtuberSrc *self)
{
  self->location = NULL;
  self->codecs = DEFAULT_CODECS;
  self->max_height = DEFAULT_MAX_HEIGHT;
  self->max_fps = DEFAULT_MAX_FPS;
  self->itags_str = NULL;

  self->itags = g_array_new (FALSE, FALSE, sizeof (guint));

  self->cancellable = g_cancellable_new ();
  self->buf_size = 0;
}

static void
gst_gtuber_src_finalize (GObject *object)
{
  GstGtuberSrc *self = GST_GTUBER_SRC (object);

  GST_DEBUG ("Finalize");

  g_free (self->location);
  g_free (self->itags_str);

  g_array_unref (self->itags);

  g_clear_object (&self->cancellable);

  GST_CALL_PARENT (G_OBJECT_CLASS, finalize, (object));
}

static void
gst_gtuber_src_set_property (GObject *object, guint prop_id,
    const GValue *value, GParamSpec *pspec)
{
  GstGtuberSrc *self = GST_GTUBER_SRC (object);

  switch (prop_id) {
    case PROP_LOCATION:{
      GError *error = NULL;

      if (!gst_gtuber_src_set_location (self, g_value_get_string (value), &error)) {
        GST_ERROR_OBJECT (self, "%s", error->message);
        g_clear_error (&error);
      }
      break;
    }
    case PROP_CODECS:
      self->codecs = g_value_get_flags (value);
      break;
    case PROP_MAX_HEIGHT:
      self->max_height = g_value_get_uint64 (value);
      break;
    case PROP_MAX_FPS:
      self->max_fps = g_value_get_uint64 (value);
      break;
    case PROP_ITAGS:
      gst_gtuber_src_set_itags (self, g_value_get_string (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_gtuber_src_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  GstGtuberSrc *self = GST_GTUBER_SRC (object);

  switch (prop_id) {
    case PROP_LOCATION:
      g_value_set_string (value, self->location);
      break;
    case PROP_CODECS:
      g_value_set_flags (value, self->codecs);
      break;
    case PROP_MAX_HEIGHT:
      g_value_set_uint64 (value, self->max_height);
      break;
    case PROP_MAX_FPS:
      g_value_set_uint64 (value, self->max_fps);
      break;
    case PROP_ITAGS:
      g_value_set_string (value, self->itags_str);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static gboolean
gst_gtuber_src_set_location (GstGtuberSrc *self, const gchar *uri,
    GError **error)
{
  GstElement *element = GST_ELEMENT (self);
  GstUri *gst_uri;
  const gchar *const *protocols;
  const gchar *host;
  gboolean supported = FALSE, blacklisted = FALSE;
  guint i;

  g_return_val_if_fail (GST_IS_ELEMENT (element), FALSE);

  GST_DEBUG_OBJECT (self, "Changing location to: %s", uri);

  g_free (self->location);
  self->location = NULL;

  if (!uri) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        "Location property cannot be NULL");
    return FALSE;
  }
  if (GST_STATE (element) == GST_STATE_PLAYING ||
      GST_STATE (element) == GST_STATE_PAUSED) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_STATE,
        "Cannot change location property while element is running");
    return FALSE;
  }

  protocols = gst_uri_handler_get_protocols (GST_URI_HANDLER (self));
  for (i = 0; protocols[i]; i++) {
    if ((supported = gst_uri_has_protocol (uri, protocols[i])))
      break;
  }
  if (!supported) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_UNSUPPORTED_PROTOCOL,
        "Location URI protocol is not supported");
    return FALSE;
  }

  gst_uri = gst_uri_from_string (uri);
  if (!gst_uri) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        "URI could not be parsed");
    return FALSE;
  }

  /* This is faster for URIs that we know are unsupported instead
   * of letting gtuber query every available plugin. We need this
   * to quickly fallback to lower ranked httpsrc plugins downstream */
  host = gst_uri_get_host (gst_uri);
  for (i = 0; hosts_blacklist[i]; i++) {
    if ((blacklisted = g_str_has_suffix (host, hosts_blacklist[i])))
      break;
  }
  gst_uri_unref (gst_uri);

  if (blacklisted) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        "This URI is not meant for Gtuber");
    return FALSE;
  }
  if (!gtuber_has_plugin_for_uri (uri, NULL)) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        "Gtuber does not have a plugin for this URI");
    return FALSE;
  }

  self->location = g_strdup (uri);

  GST_DEBUG_OBJECT (self, "Location changed to: %s", self->location);

  return TRUE;
}

static void
gst_gtuber_src_set_itags (GstGtuberSrc *self, const gchar *itags_str)
{
  gchar **itags;
  guint index = 0;

  g_free (self->itags_str);
  self->itags_str = g_strdup (itags_str);

  g_array_remove_range (self->itags, 0, self->itags->len);

  itags = g_strsplit (itags_str, ",", 0);
  while (itags[index]) {
    guint itag;

    g_strstrip (itags[index]);
    itag = g_ascii_strtoull (itags[index], NULL, 10);
    if (itag > 0) {
      g_array_append_val (self->itags, itag);
      GST_DEBUG_OBJECT (self, "Added allowed itag: %u", itag);
    }
    index++;
  }
  g_strfreev (itags);
}

static gboolean
gst_gtuber_src_start (GstBaseSrc *base_src)
{
  GstGtuberSrc *self = GST_GTUBER_SRC (base_src);

  GST_DEBUG_OBJECT (self, "Start");

  if (G_UNLIKELY (!self->location)) {
    GST_ELEMENT_ERROR (self, RESOURCE, OPEN_READ, (NULL),
        ("No media location"));
    return FALSE;
  }

  return TRUE;
}

static gboolean
gst_gtuber_src_stop (GstBaseSrc *base_src)
{
  GstGtuberSrc *self = GST_GTUBER_SRC (base_src);

  GST_DEBUG_OBJECT (self, "Stop");

  self->buf_size = 0;

  return TRUE;
}

static gboolean
gst_gtuber_src_get_size (GstBaseSrc *base_src, guint64 *size)
{
  GstGtuberSrc *self = GST_GTUBER_SRC (base_src);

  if (self->buf_size > 0) {
    *size = self->buf_size;
    return TRUE;
  }

  return FALSE;
}

static gboolean
gst_gtuber_src_is_seekable (GstBaseSrc *base_src)
{
  return FALSE;
}

static gboolean
gst_gtuber_src_unlock (GstBaseSrc *base_src)
{
  GstGtuberSrc *self = GST_GTUBER_SRC (base_src);

  GST_LOG_OBJECT (self, "Cancel triggered");
  g_cancellable_cancel (self->cancellable);

  return TRUE;
}

static gboolean
gst_gtuber_src_unlock_stop (GstBaseSrc *base_src)
{
  GstGtuberSrc *self = GST_GTUBER_SRC (base_src);

  GST_LOG_OBJECT (self, "Resetting cancellable");

  g_object_unref (self->cancellable);
  self->cancellable = g_cancellable_new ();

  return TRUE;
}

static void
insert_header_cb (const gchar *name, const gchar *value, GstStructure *structure)
{
  if (strcmp (name, GST_GTUBER_HEADER_UA))
    gst_structure_set (structure, name, G_TYPE_STRING, value, NULL);
}

static void
gst_gtuber_src_push_config_event (GstGtuberSrc *self, GtuberMediaInfo *info)
{
  GHashTable *gtuber_headers;

  gtuber_headers = gtuber_media_info_get_request_headers (info);

  if (gtuber_headers && g_hash_table_size (gtuber_headers) > 0) {
    GstStructure *config, *req_headers;
    GstEvent *event;
    const gchar *ua;

    ua = g_hash_table_lookup (gtuber_headers, GST_GTUBER_HEADER_UA);
    req_headers = gst_structure_new_empty ("request-headers");

    g_hash_table_foreach (gtuber_headers,
        (GHFunc) insert_header_cb, req_headers);

    config = gst_structure_new (GST_GTUBER_CONFIG,
        GST_GTUBER_PROP_UA, G_TYPE_STRING, ua,
        GST_GTUBER_PROP_EXTRA_HEADERS, GST_TYPE_STRUCTURE, req_headers,
        NULL);
    gst_structure_free (req_headers);

    event = gst_event_new_custom (GST_EVENT_CUSTOM_DOWNSTREAM_STICKY, config);
    gst_pad_push_event (GST_BASE_SRC_PAD (self), event);
  }
}

static gboolean
stream_filter_func (GtuberAdaptiveStream *astream, GstGtuberSrc *self)
{
  GtuberStream *stream = (GtuberStream *) astream;

  if (self->codecs > 0) {
    GtuberCodecFlags flags = gtuber_stream_get_codec_flags (stream);

    if ((self->codecs & flags) != flags)
      return FALSE;
  }

  if (gtuber_stream_get_video_codec (stream) != NULL) {
    if (self->max_height > 0) {
      guint height = gtuber_stream_get_height (stream);

      if (height == 0 || height > self->max_height)
        return FALSE;
    }
    if (self->max_fps > 0) {
      guint fps = gtuber_stream_get_fps (stream);

      if (fps == 0 || fps > self->max_fps)
        return FALSE;
    }
  }

  if (self->itags->len > 0) {
    guint i, itag = gtuber_stream_get_itag (stream);
    gboolean found = FALSE;

    for (i = 0; i < self->itags->len; i++) {
      if ((found = itag == g_array_index (self->itags, guint, i)))
        break;
    }

    if (!found)
      return FALSE;
  }

  return TRUE;
}

static gchar *
gst_gtuber_generate_manifest (GstGtuberSrc *self, GtuberMediaInfo *info,
    GtuberAdaptiveStreamManifest *manifest_type)
{
  GtuberManifestGenerator *gen;
  GtuberAdaptiveStreamManifest type;
  gchar *data;

  gen = gtuber_manifest_generator_new ();
  gtuber_manifest_generator_set_media_info (gen, info);

  gtuber_manifest_generator_set_filter_func (gen,
      (GtuberAdaptiveStreamFilter) stream_filter_func, self, NULL);

  for (type = GTUBER_ADAPTIVE_STREAM_MANIFEST_DASH;
      type <= GTUBER_ADAPTIVE_STREAM_MANIFEST_HLS; type++) {
    gtuber_manifest_generator_set_manifest_type (gen, type);

    if ((data = gtuber_manifest_generator_to_data (gen)))
      break;
  }

  g_object_unref (gen);

  if (manifest_type) {
    if (!data)
      type = GTUBER_ADAPTIVE_STREAM_MANIFEST_UNKNOWN;

    *manifest_type = type;
  }

  return data;
}

static GstBuffer *
gst_gtuber_media_info_to_buffer (GstGtuberSrc *self, GtuberMediaInfo *info,
    GError **error)
{
  GtuberAdaptiveStreamManifest manifest_type;
  GstBuffer *buffer;
  GstCaps *caps = NULL;
  gchar *data;

  data = gst_gtuber_generate_manifest (self, info, &manifest_type);

  /* TODO: Fallback to best combined URI */
  if (!data)
    GST_FIXME_OBJECT (self, "Implement fallback to best combined URI");

  if (!data) {
    g_set_error (error, GTUBER_MANIFEST_GENERATOR_ERROR,
        GTUBER_MANIFEST_GENERATOR_ERROR_NO_DATA,
        "No manifest data was generated");
    return FALSE;
  }

  switch (manifest_type) {
    case GTUBER_ADAPTIVE_STREAM_MANIFEST_DASH:
      caps = gst_caps_new_empty_simple ("application/dash+xml");
      break;
    case GTUBER_ADAPTIVE_STREAM_MANIFEST_HLS:
      caps = gst_caps_new_empty_simple ("application/x-hls");
      break;
    default:
      GST_WARNING_OBJECT (self, "Unsupported gtuber manifest type");
      break;
  }

  if (caps) {
    gst_caps_set_simple (caps,
        "source", G_TYPE_STRING, "gtuber",
        NULL);
    if (gst_base_src_set_caps (GST_BASE_SRC (self), caps))
      GST_INFO_OBJECT (self, "Using caps: %" GST_PTR_FORMAT, caps);

    gst_caps_unref (caps);
  }

  self->buf_size = strlen (data);
  buffer = gst_buffer_new_wrapped (data, self->buf_size);

  return buffer;
}

static gboolean
gst_gtuber_fetch_into_buffer (GstGtuberSrc *self, GstBuffer **outbuf,
    GError **error)
{
  GtuberClient *client;
  GtuberMediaInfo *info;
  GMainContext *ctx;
  gchar *uri;

  /* Gtuber is not guaranteed to handle "gtuber" URI scheme */
  uri = (g_str_has_prefix (self->location, "gtuber"))
      ? g_strconcat ("https", self->location + 6, NULL)
      : g_strdup (self->location);

  GST_DEBUG_OBJECT (self, "Fetching media info for URI: %s", uri);

  ctx = g_main_context_new ();
  g_main_context_push_thread_default (ctx);

  client = gtuber_client_new ();
  info = gtuber_client_fetch_media_info (client, uri,
      self->cancellable, error);
  g_object_unref (client);

  g_main_context_pop_thread_default (ctx);
  g_main_context_unref (ctx);

  g_free (uri);

  if (!info)
    return FALSE;

  GST_DEBUG_OBJECT (self, "Fetched media info");

  *outbuf = gst_gtuber_media_info_to_buffer (self, info, error);

  if (*outbuf)
    gst_gtuber_src_push_config_event (self, info);

  g_object_unref (info);

  return (*outbuf) ? TRUE : FALSE;
}

static GstFlowReturn
gst_gtuber_src_create (GstPushSrc *push_src, GstBuffer **outbuf)
{
  GstGtuberSrc *self = GST_GTUBER_SRC (push_src);
  GError *error = NULL;

  /* When non-zero, we already returned complete data */
  if (self->buf_size > 0)
    return GST_FLOW_EOS;

  if (!gst_gtuber_fetch_into_buffer (self, outbuf, &error)) {
    GST_ERROR_OBJECT (self, "%s", error->message);
    g_clear_error (&error);

    return GST_FLOW_ERROR;
  }

  return GST_FLOW_OK;
}

static gboolean
gst_gtuber_src_query (GstBaseSrc *base_src, GstQuery *query)
{
  GstGtuberSrc *self = GST_GTUBER_SRC (base_src);
  gboolean ret = FALSE;

  switch (GST_QUERY_TYPE (query)) {
    case GST_QUERY_URI:
      if (GST_IS_URI_HANDLER (self)) {
        gchar *uri;

        uri = gst_uri_handler_get_uri (GST_URI_HANDLER (self));
        gst_query_set_uri (query, uri);

        g_free (uri);
        ret = TRUE;
      }
      break;
    default:
      ret = FALSE;
      break;
  }

  if (!ret)
    ret = GST_BASE_SRC_CLASS (parent_class)->query (base_src, query);

  return ret;
}

/**
 * GstURIHandlerInterface
 */
static GstURIType
gst_gtuber_uri_handler_get_type_src (GType type)
{
  return GST_URI_SRC;
}

static const gchar *const *
gst_gtuber_uri_handler_get_protocols (GType type)
{
  static const gchar *protocols[] = {
    "https", "gtuber", NULL
  };

  return protocols;
}

static gchar *
gst_gtuber_uri_handler_get_uri (GstURIHandler *handler)
{
  GstElement *element = GST_ELEMENT (handler);
  gchar *uri;

  g_return_val_if_fail (GST_IS_ELEMENT (element), NULL);

  g_object_get (G_OBJECT (element), "location", &uri, NULL);

  return uri;
}

static gboolean
gst_gtuber_uri_handler_set_uri (GstURIHandler *handler, const gchar *uri, GError **error)
{
  GstGtuberSrc *self = GST_GTUBER_SRC (handler);

  return gst_gtuber_src_set_location (self, uri, error);
}

static void
gst_gtuber_uri_handler_init (gpointer g_iface, gpointer iface_data)
{
  GstURIHandlerInterface *iface = (GstURIHandlerInterface *) g_iface;

  iface->get_type = gst_gtuber_uri_handler_get_type_src;
  iface->get_protocols = gst_gtuber_uri_handler_get_protocols;
  iface->get_uri = gst_gtuber_uri_handler_get_uri;
  iface->set_uri = gst_gtuber_uri_handler_set_uri;
}

void
gst_gtuber_uri_handler_do_init (GType type)
{
  GInterfaceInfo uri_handler_info = {
    gst_gtuber_uri_handler_init,
    NULL,
    NULL
  };

  g_type_add_interface_static (type, GST_TYPE_URI_HANDLER, &uri_handler_info);
}
