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

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS_ANY);

#define gst_gtuber_src_parent_class parent_class
G_DEFINE_TYPE_WITH_CODE (GstGtuberSrc, gst_gtuber_src,
    GST_TYPE_PUSH_SRC, gst_gtuber_uri_handler_do_init (g_define_type_id));
GST_ELEMENT_REGISTER_DEFINE_WITH_CODE (gtubersrc, "gtubersrc",
    GST_RANK_PRIMARY + 10, GST_TYPE_GTUBER_SRC, gst_gtuber_element_init (plugin));

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
    const gchar *location, GError **error);
static void gst_gtuber_src_set_itags (GstGtuberSrc *self,
    const gchar *itags_str);

static void
gst_gtuber_src_class_init (GstGtuberSrcClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;
  GstElementClass *gstelement_class = (GstElementClass *) klass;
  GstBaseSrcClass *gstbasesrc_class = (GstBaseSrcClass *) klass;
  GstPushSrcClass *gstpushsrc_class = (GstPushSrcClass *) klass;

  GST_DEBUG_CATEGORY_INIT (gst_gtuber_src_debug, "gtubersrc", 0,
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

  param_specs[PROP_MAX_HEIGHT] = g_param_spec_uint ("max-height",
      "Maximal Height", "Maximal allowed video height in pixels (0 = unlimited)",
      0, G_MAXUINT, DEFAULT_MAX_HEIGHT,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_MAX_FPS] = g_param_spec_uint ("max-fps",
      "Maximal FPS", "Maximal allowed video framerate (0 = unlimited)",
      0, G_MAXUINT, DEFAULT_MAX_FPS,
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
  g_mutex_init (&self->prop_lock);

  g_mutex_init (&self->client_lock);
  g_cond_init (&self->client_finished);

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

  GST_TRACE ("Finalize");

  g_mutex_lock (&self->client_lock);

  if (self->client_thread)
    g_thread_unref (self->client_thread);

  g_mutex_unlock (&self->client_lock);

  g_free (self->location);
  g_free (self->itags_str);

  g_array_unref (self->itags);

  g_clear_object (&self->cancellable);

  g_mutex_clear (&self->client_lock);
  g_cond_clear (&self->client_finished);

  g_mutex_clear (&self->prop_lock);

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
      g_mutex_lock (&self->prop_lock);
      self->codecs = g_value_get_flags (value);
      g_mutex_unlock (&self->prop_lock);
      break;
    case PROP_MAX_HEIGHT:
      g_mutex_lock (&self->prop_lock);
      self->max_height = g_value_get_uint (value);
      g_mutex_unlock (&self->prop_lock);
      break;
    case PROP_MAX_FPS:
      g_mutex_lock (&self->prop_lock);
      self->max_fps = g_value_get_uint (value);
      g_mutex_unlock (&self->prop_lock);
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

  g_mutex_lock (&self->prop_lock);

  switch (prop_id) {
    case PROP_LOCATION:
      g_value_set_string (value, self->location);
      break;
    case PROP_CODECS:
      g_value_set_flags (value, self->codecs);
      break;
    case PROP_MAX_HEIGHT:
      g_value_set_uint (value, self->max_height);
      break;
    case PROP_MAX_FPS:
      g_value_set_uint (value, self->max_fps);
      break;
    case PROP_ITAGS:
      g_value_set_string (value, self->itags_str);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }

  g_mutex_unlock (&self->prop_lock);
}

static gchar *
location_to_uri (const gchar *location)
{
  /* Gtuber is not guaranteed to handle "gtuber" URI scheme */
  return (g_str_has_prefix (location, "gtuber"))
      ? g_strconcat ("https", location + 6, NULL)
      : g_strdup (location);
}

static gboolean
gst_gtuber_src_set_location (GstGtuberSrc *self, const gchar *location,
    GError **error)
{
  GstElement *element = GST_ELEMENT (self);
  const gchar *const *protocols;
  gchar *uri;
  gboolean supported = FALSE;
  guint i;

  g_return_val_if_fail (GST_IS_ELEMENT (element), FALSE);

  GST_DEBUG_OBJECT (self, "Changing location to: %s", location);

  g_mutex_lock (&self->prop_lock);

  g_free (self->location);
  self->location = NULL;

  g_mutex_unlock (&self->prop_lock);

  if (!location) {
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
    if ((supported = gst_uri_has_protocol (location, protocols[i])))
      break;
  }
  if (!supported) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_UNSUPPORTED_PROTOCOL,
        "Location URI protocol is not supported");
    return FALSE;
  }

  uri = location_to_uri (location);
  supported = gtuber_has_plugin_for_uri (uri, NULL);
  g_free (uri);

  if (!supported) {
    g_set_error (error, GST_URI_ERROR, GST_URI_ERROR_BAD_URI,
        "Gtuber does not have a plugin for this URI");
    return FALSE;
  }

  g_mutex_lock (&self->prop_lock);

  self->location = g_strdup (location);
  GST_DEBUG_OBJECT (self, "Location changed to: %s", self->location);

  g_mutex_unlock (&self->prop_lock);

  return TRUE;
}

static void
gst_gtuber_src_set_itags (GstGtuberSrc *self, const gchar *itags_str)
{
  gchar **itags;
  guint index = 0;

  g_mutex_lock (&self->prop_lock);

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

  g_mutex_unlock (&self->prop_lock);
  g_strfreev (itags);
}

static gboolean
gst_gtuber_src_start (GstBaseSrc *base_src)
{
  GstGtuberSrc *self = GST_GTUBER_SRC (base_src);
  gboolean can_start;

  GST_DEBUG_OBJECT (self, "Start");

  g_mutex_lock (&self->prop_lock);
  can_start = (self->location != NULL);
  g_mutex_unlock (&self->prop_lock);

  if (G_UNLIKELY (!can_start)) {
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
insert_chapter_cb (guint64 time, const gchar *name, GstTocEntry *entry)
{
  GstTocEntry *subentry;
  GstClockTime clock_time;
  gchar *id;

  clock_time = time * GST_MSECOND;
  GST_DEBUG ("Inserting TOC chapter, time: %lu, name: %s", clock_time, name);

  id = g_strdup_printf ("chap.%lu", time);
  subentry = gst_toc_entry_new (GST_TOC_ENTRY_TYPE_CHAPTER, id);
  g_free (id);

  gst_toc_entry_set_tags (subentry,
      gst_tag_list_new (GST_TAG_TITLE, name, NULL));
  gst_toc_entry_set_start_stop_times (subentry, clock_time, GST_CLOCK_TIME_NONE);

  gst_toc_entry_append_sub_entry (entry, subentry);
}

static void
gst_gtuber_src_push_events (GstGtuberSrc *self, GtuberMediaInfo *info)
{
  GHashTable *gtuber_headers, *gtuber_chapters;
  GstTagList *tags;
  const gchar *tag;

  gtuber_headers = gtuber_media_info_get_request_headers (info);

  if (gtuber_headers && g_hash_table_size (gtuber_headers) > 0) {
    GstStructure *config, *req_headers;
    GstEvent *event;
    const gchar *ua;

    GST_DEBUG_OBJECT (self, "Creating " GST_GTUBER_CONFIG " event");

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

    GST_DEBUG_OBJECT (self, "Pushing " GST_GTUBER_CONFIG " event");
    gst_pad_push_event (GST_BASE_SRC_PAD (self), event);
  }

  GST_DEBUG_OBJECT (self, "Creating TAG event");
  tags = gst_tag_list_new_empty ();

  if ((tag = gtuber_media_info_get_title (info)))
    gst_tag_list_add (tags, GST_TAG_MERGE_APPEND, GST_TAG_TITLE, tag, NULL);
  if ((tag = gtuber_media_info_get_description (info)))
    gst_tag_list_add (tags, GST_TAG_MERGE_APPEND, GST_TAG_DESCRIPTION, tag, NULL);

  if (gst_tag_list_is_empty (tags)) {
    GST_DEBUG_OBJECT (self, "No tags to push");
    gst_tag_list_unref (tags);
  } else {
    GstEvent *event;

    gst_tag_list_set_scope (tags, GST_TAG_SCOPE_GLOBAL);
    event = gst_event_new_tag (gst_tag_list_copy (tags));

    GST_DEBUG_OBJECT (self, "Pushing TAG event: %p", event);

    gst_pad_push_event (GST_BASE_SRC_PAD (self), event);
    gst_element_post_message (GST_ELEMENT (self),
        gst_message_new_tag (GST_OBJECT (self), tags));
  }

  gtuber_chapters = gtuber_media_info_get_chapters (info);

  if (gtuber_chapters && g_hash_table_size (gtuber_chapters) > 0) {
    GstToc *toc;
    GstTocEntry *toc_entry;
    GstEvent *event;

    toc = gst_toc_new (GST_TOC_SCOPE_GLOBAL);
    toc_entry = gst_toc_entry_new (GST_TOC_ENTRY_TYPE_EDITION, "00");

    gst_toc_entry_set_start_stop_times (toc_entry, 0,
        gtuber_media_info_get_duration (info) * GST_SECOND);

    g_hash_table_foreach (gtuber_chapters,
        (GHFunc) insert_chapter_cb, toc_entry);

    gst_toc_append_entry (toc, toc_entry);
    event = gst_event_new_toc (toc, FALSE);

    GST_DEBUG_OBJECT (self, "Pushing TOC event: %p", event);

    gst_pad_push_event (GST_BASE_SRC_PAD (self), event);
    gst_element_post_message (GST_ELEMENT (self),
        gst_message_new_toc (GST_OBJECT (self), toc, FALSE));

    gst_toc_unref (toc);
  }

  GST_DEBUG_OBJECT (self, "Pushed all events");
}

/* Called with a lock on props */
static gboolean
get_is_stream_allowed (GtuberStream *stream, GstGtuberSrc *self)
{
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

static gboolean
astream_filter_func (GtuberAdaptiveStream *astream, GstGtuberSrc *self)
{
  return get_is_stream_allowed ((GtuberStream *) astream, self);
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
      (GtuberAdaptiveStreamFilter) astream_filter_func, self, NULL);

  for (type = GTUBER_ADAPTIVE_STREAM_MANIFEST_DASH;
      type <= GTUBER_ADAPTIVE_STREAM_MANIFEST_HLS; type++) {
    gtuber_manifest_generator_set_manifest_type (gen, type);

    /* Props are accessed in filter callback */
    g_mutex_lock (&self->prop_lock);
    data = gtuber_manifest_generator_to_data (gen);
    g_mutex_unlock (&self->prop_lock);

    if (data)
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

static void
_rate_streams_vals (guint a_num, guint b_num, guint weight,
    guint *a_pts, guint *b_pts)
{
  if (a_num > b_num)
    *a_pts += weight;
  else if (a_num < b_num)
    *b_pts += weight;
}

static gchar *
gst_gtuber_generate_best_uri_data (GstGtuberSrc *self, GtuberMediaInfo *info)
{
  GPtrArray *streams;
  GtuberStream *best_stream = NULL;
  gchar *data = NULL;
  guint i;

  streams = gtuber_media_info_get_streams (info);

  for (i = 0; i < streams->len; i++) {
    GtuberStream *stream;

    stream = g_ptr_array_index (streams, i);

    if (get_is_stream_allowed (stream, self)) {
      guint best_pts = 0, curr_pts = 0;

      if (best_stream) {
        _rate_streams_vals (
            gtuber_stream_get_height (best_stream),
            gtuber_stream_get_height (stream),
            8, &best_pts, &curr_pts);
        _rate_streams_vals (
            gtuber_stream_get_width (best_stream),
            gtuber_stream_get_width (stream),
            4, &best_pts, &curr_pts);
        _rate_streams_vals (
            gtuber_stream_get_bitrate (best_stream),
            gtuber_stream_get_bitrate (stream),
            2, &best_pts, &curr_pts);
        _rate_streams_vals (
            gtuber_stream_get_fps (best_stream),
            gtuber_stream_get_fps (stream),
            1, &best_pts, &curr_pts);
      }

      if (!best_stream || curr_pts > best_pts) {
        best_stream = stream;
        GST_DEBUG ("Current best stream itag: %u",
            gtuber_stream_get_itag (best_stream));
      }
    }
  }

  if (best_stream)
    data = g_strdup (gtuber_stream_get_uri (best_stream));

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

  if ((data = gst_gtuber_generate_manifest (self, info, &manifest_type))) {
    GST_INFO ("Using adaptive streaming");

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
  } else if ((data = gst_gtuber_generate_best_uri_data (self, info))) {
    GST_INFO ("Using direct stream");
    caps = gst_caps_new_empty_simple ("text/uri-list");
  } else {
    g_set_error (error, GTUBER_MANIFEST_GENERATOR_ERROR,
        GTUBER_MANIFEST_GENERATOR_ERROR_NO_DATA,
        "No manifest data was generated");
    return FALSE;
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

typedef struct
{
  GstGtuberSrc *src;
  GtuberMediaInfo *info;
  GError *error;
} GstGtuberThreadData;

static GstGtuberThreadData *
gst_gtuber_thread_data_new (GstGtuberSrc *self)
{
  GstGtuberThreadData *data;

  data = g_new (GstGtuberThreadData, 1);
  data->src = gst_object_ref (self);
  data->info = NULL;
  data->error = NULL;

  return data;
}

static void
gst_gtuber_thread_data_free (GstGtuberThreadData *data)
{
  gst_object_unref (data->src);
  g_clear_object (&data->info);
  g_clear_error (&data->error);

  g_free (data);
}

static gpointer
client_thread_func (GstGtuberThreadData *data)
{
  GstGtuberSrc *self = data->src;
  GtuberClient *client;
  GMainContext *ctx;
  gchar *uri;

  g_mutex_lock (&self->client_lock);
  GST_DEBUG ("Entered new GtuberClient thread");

  g_mutex_lock (&self->prop_lock);
  uri = location_to_uri (self->location);
  g_mutex_unlock (&self->prop_lock);

  ctx = g_main_context_new ();
  g_main_context_push_thread_default (ctx);

  GST_INFO ("Fetching media info for URI: %s", uri);

  client = gtuber_client_new ();
  data->info = gtuber_client_fetch_media_info (client, uri,
      self->cancellable, &data->error);
  g_object_unref (client);

  g_main_context_pop_thread_default (ctx);
  g_main_context_unref (ctx);

  g_free (uri);

  GST_DEBUG ("Leaving GtuberClient thread");

  g_cond_signal (&self->client_finished);
  g_mutex_unlock (&self->client_lock);

  return NULL;
}

static gboolean
gst_gtuber_fetch_into_buffer (GstGtuberSrc *self, GstBuffer **outbuf,
    GError **error)
{
  GstGtuberThreadData *data;

  GST_DEBUG_OBJECT (self, "Fetching media info");

  g_mutex_lock (&self->client_lock);

  data = gst_gtuber_thread_data_new (self);
  self->client_thread = g_thread_new ("GstGtuberClientThread",
      (GThreadFunc) client_thread_func, data);

  g_cond_wait (&self->client_finished, &self->client_lock);

  g_thread_unref (self->client_thread);
  self->client_thread = NULL;

  g_mutex_unlock (&self->client_lock);

  if (!data->info) {
    *error = g_error_copy (data->error);
    gst_gtuber_thread_data_free (data);

    return FALSE;
  }

  GST_DEBUG_OBJECT (self, "Fetched media info");

  *outbuf = gst_gtuber_media_info_to_buffer (self, data->info, error);

  if (*outbuf)
    gst_gtuber_src_push_events (self, data->info);

  gst_gtuber_thread_data_free (data);

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

static gpointer
_obtain_available_protocols (G_GNUC_UNUSED gpointer data)
{
  const gchar *const *gtuber_schemes;
  gchar **my_schemes;
  guint n_schemes, i;

  GST_DEBUG ("Checking supported by gtuber URI schemes");

  gtuber_schemes = gtuber_get_supported_schemes ();

  n_schemes = (gtuber_schemes != NULL)
      ? g_strv_length ((gchar **) gtuber_schemes)
      : 0;

  /* Supported schemes are NULL terminated, but we need to add
   * one more special "gtuber" scheme, so reserve space */
  my_schemes = g_new0 (gchar *, n_schemes + 2);

  for (i = 0; i <= n_schemes; i++) {
    my_schemes[i] = (i < n_schemes)
        ? g_strdup (gtuber_schemes[i])
        : g_strdup ("gtuber");

    GST_INFO ("Added supported URI scheme: %s", my_schemes[i]);
  }

  return my_schemes;
}

static const gchar *const *
gst_gtuber_uri_handler_get_protocols (GType type)
{
  static GOnce once = G_ONCE_INIT;

  g_once (&once, _obtain_available_protocols, NULL);
  return (const gchar *const *) once.retval;
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
