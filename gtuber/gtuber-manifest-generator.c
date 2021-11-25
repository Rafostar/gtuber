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

/**
 * SECTION:gtuber-manifest-generator
 * @title: GtuberManifestGenerator
 * @short_description: generates adaptive streams manifest
 *   from media info
 */

#include "gtuber-enums.h"
#include "gtuber-manifest-generator.h"
#include "gtuber-stream.h"

enum
{
  PROP_0,
  PROP_PRETTY,
  PROP_INDENT,
  PROP_MANIFEST_TYPE,
  PROP_LAST
};

struct _GtuberManifestGenerator
{
  GObject parent;

  gboolean pretty;
  guint indent;

  GtuberAdaptiveStreamManifest manifest_type;
  GtuberMediaInfo *media_info;

  GtuberAdaptiveStreamFilter filter_func;
  gpointer filter_data;
  GDestroyNotify filter_destroy;
};

struct _GtuberManifestGeneratorClass
{
  GObjectClass parent_class;
};

typedef enum
{
  DASH_CODEC_UNKNOWN,
  DASH_CODEC_AVC,
  DASH_CODEC_HEVC,
  DASH_CODEC_VP9,
  DASH_CODEC_AV1,
  DASH_CODEC_MP4A,
  DASH_CODEC_OPUS,
} DashCodec;

#define parent_class gtuber_manifest_generator_parent_class
G_DEFINE_TYPE (GtuberManifestGenerator, gtuber_manifest_generator, G_TYPE_OBJECT)
G_DEFINE_QUARK (gtubermanifestgenerator-error-quark, gtuber_manifest_generator_error)

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void gtuber_manifest_generator_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec);
static void gtuber_manifest_generator_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec);
static void gtuber_manifest_generator_dispose (GObject *object);
static void gtuber_manifest_generator_finalize (GObject *object);

static void
gtuber_manifest_generator_init (GtuberManifestGenerator *self)
{
  self->pretty = FALSE;
  self->indent = 2;

  self->manifest_type = GTUBER_ADAPTIVE_STREAM_MANIFEST_UNKNOWN;
  self->media_info = NULL;

  self->filter_func = NULL;
  self->filter_destroy = NULL;
}

static void
gtuber_manifest_generator_class_init (GtuberManifestGeneratorClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->set_property = gtuber_manifest_generator_set_property;
  gobject_class->get_property = gtuber_manifest_generator_get_property;
  gobject_class->dispose = gtuber_manifest_generator_dispose;
  gobject_class->finalize = gtuber_manifest_generator_finalize;

  param_specs[PROP_PRETTY] =
      g_param_spec_boolean ("pretty", "Pretty",
      "Apply new lines and indentation to manifests that support them",
      FALSE, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_INDENT] = g_param_spec_uint ("indent",
      "Indent", "Amount of spaces used for indent", 0, G_MAXUINT, 0,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_MANIFEST_TYPE] = g_param_spec_enum ("manifest-type",
      "Adaptive Stream Manifest Type", "The manifest type to generate",
      GTUBER_TYPE_ADAPTIVE_STREAM_MANIFEST, GTUBER_ADAPTIVE_STREAM_MANIFEST_UNKNOWN,
      G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}

static void
gtuber_manifest_generator_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GtuberManifestGenerator *self = GTUBER_MANIFEST_GENERATOR (object);

  switch (prop_id) {
    case PROP_PRETTY:
      gtuber_manifest_generator_set_pretty (self, g_value_get_boolean (value));
      break;
    case PROP_INDENT:
      gtuber_manifest_generator_set_indent (self, g_value_get_uint (value));
      break;
    case PROP_MANIFEST_TYPE:
      gtuber_manifest_generator_set_manifest_type (self, g_value_get_enum (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gtuber_manifest_generator_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  GtuberManifestGenerator *self = GTUBER_MANIFEST_GENERATOR (object);

  switch (prop_id) {
    case PROP_PRETTY:
      g_value_set_boolean (value, gtuber_manifest_generator_get_pretty (self));
      break;
    case PROP_INDENT:
      g_value_set_uint (value, gtuber_manifest_generator_get_indent (self));
      break;
    case PROP_MANIFEST_TYPE:
      g_value_set_enum (value, gtuber_manifest_generator_get_manifest_type (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gtuber_manifest_generator_dispose (GObject *object)
{
  GtuberManifestGenerator *self = GTUBER_MANIFEST_GENERATOR (object);

  if (self->filter_destroy)
    self->filter_destroy (self->filter_data);

  self->filter_func = NULL;
  self->filter_destroy = NULL;

  G_OBJECT_CLASS (parent_class)->dispose (object);
}

static void
gtuber_manifest_generator_finalize (GObject *object)
{
  GtuberManifestGenerator *self = GTUBER_MANIFEST_GENERATOR (object);

  g_debug ("ManifestGenerator finalize");

  if (self->media_info)
    g_object_unref (self->media_info);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

static DashCodec
get_dash_video_codec (const gchar *codec)
{
  DashCodec dash_codec = DASH_CODEC_UNKNOWN;

  if (codec) {
    if (g_str_has_prefix (codec, "avc"))
      dash_codec = DASH_CODEC_AVC;
    else if (g_str_has_prefix (codec, "vp9"))
      dash_codec = DASH_CODEC_VP9;
    else if (g_str_has_prefix (codec, "hev"))
      dash_codec = DASH_CODEC_HEVC;
    else if (g_str_has_prefix (codec, "av01"))
      dash_codec = DASH_CODEC_AV1;
  }

  return dash_codec;
}

static DashCodec
get_dash_audio_codec (const gchar *codec)
{
  DashCodec dash_codec = DASH_CODEC_UNKNOWN;

  if (codec) {
    if (g_str_has_prefix (codec, "mp4a"))
      dash_codec = DASH_CODEC_MP4A;
    else if (g_str_has_prefix (codec, "opus"))
      dash_codec = DASH_CODEC_OPUS;
  }

  return dash_codec;
}

static guint
_get_gcd (guint width, guint height)
{
  return (height > 0)
      ? _get_gcd (height, width % height)
      : width;
}

static gchar *
obtain_par_from_res (guint width, guint height)
{
  guint gcd;

  if (!width || !height)
    return g_strdup ("1:1");

  gcd = _get_gcd (width, height);

  width /= gcd;
  height /= gcd;

  return g_strdup_printf ("%i:%i", width, height);
}

typedef struct
{
  GtuberStreamMimeType mime_type;
  DashCodec dash_codec;
  guint max_width;
  guint max_height;
  guint max_fps;
  GPtrArray *adaptive_streams;
} DashAdaptationData;

static DashAdaptationData *
dash_adaptation_data_new (GtuberStreamMimeType mime_type, DashCodec dash_codec)
{
  DashAdaptationData *data;

  data = g_new (DashAdaptationData, 1);
  data->mime_type = mime_type;
  data->dash_codec = dash_codec;
  data->max_width = 0;
  data->max_height = 0;
  data->max_fps = 0;
  data->adaptive_streams =
      g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);

  return data;
}

static void
dash_adaptation_data_free (DashAdaptationData *data)
{
  g_ptr_array_unref (data->adaptive_streams);
  g_free (data);
}

static DashAdaptationData *
get_adaptation_data_for_stream (GtuberStream *stream, GPtrArray *adaptations)
{
  GtuberStreamMimeType mime_type;
  DashCodec dash_codec = DASH_CODEC_UNKNOWN;
  const gchar *codec = NULL;
  guint i;

  mime_type = gtuber_stream_get_mime_type (stream);

  switch (mime_type) {
    case GTUBER_STREAM_MIME_TYPE_VIDEO_MP4:
    case GTUBER_STREAM_MIME_TYPE_VIDEO_WEBM:
      codec = gtuber_stream_get_video_codec (stream);
      dash_codec = get_dash_video_codec (codec);
      break;
    case GTUBER_STREAM_MIME_TYPE_AUDIO_MP4:
    case GTUBER_STREAM_MIME_TYPE_AUDIO_WEBM:
      codec = gtuber_stream_get_audio_codec (stream);
      dash_codec = get_dash_audio_codec (codec);
      break;
    default:
      break;
  }

  if (codec && dash_codec != DASH_CODEC_UNKNOWN) {
    DashAdaptationData *data = NULL;

    for (i = 0; i < adaptations->len; i++) {
      data = g_ptr_array_index (adaptations, i);
      if (data->mime_type == mime_type && data->dash_codec == dash_codec)
        return data;
    }

    /* No such adaptation yet in array, so create
     * a new one, add it to array and return it */
    data = dash_adaptation_data_new (mime_type, dash_codec);
    g_ptr_array_add (adaptations, data);

    return data;
  }

  g_debug ("Cannot create adaptation data for unknown codec: %s", codec);

  return NULL;
}

static void
add_line_no_newline (GtuberManifestGenerator *self, GString *string,
    guint depth, const gchar *line)
{
  guint indent = depth * self->indent;

  if (self->pretty) {
    while (indent--)
      g_string_append (string, " ");
  }

  g_string_append (string, line);
}

static void
finish_line (GtuberManifestGenerator *self, GString *string, const gchar *suffix)
{
  g_string_append_printf (string, "%s%s",
      suffix ? suffix : "", self->pretty ? "\n" : "");
}

static void
add_line (GtuberManifestGenerator *self, GString *string,
    guint depth, const gchar *line)
{
  add_line_no_newline (self, string, depth, line);
  finish_line (self, string, NULL);
}

static void
add_option_string (GString *string, const gchar *key, const gchar *value)
{
  g_string_append_printf (string, " %s=\"%s\"", key, value);
}

static void
add_option_int (GString *string, const gchar *key, guint64 value)
{
  g_string_append_printf (string, " %s=\"%lu\"", key, value);
}

static void
add_option_range (GString *string, const gchar *key, guint64 start, guint64 end)
{
  g_string_append_printf (string, " %s=\"%lu-%lu\"", key, start, end);
}

static void
add_option_boolean (GString *string, const gchar *key, gboolean value)
{
  const gchar *boolean_str;

  boolean_str = (value) ? "true" : "false";
  g_string_append_printf (string, " %s=\"%s\"", key, boolean_str);
}

static void
parse_content_and_mime_type (GtuberStreamMimeType mime_type,
    gchar **content_str, gchar **mime_str)
{
  switch (mime_type) {
    case GTUBER_STREAM_MIME_TYPE_VIDEO_MP4:
    case GTUBER_STREAM_MIME_TYPE_VIDEO_WEBM:
      *content_str = g_strdup ("video");
      break;
    case GTUBER_STREAM_MIME_TYPE_AUDIO_MP4:
    case GTUBER_STREAM_MIME_TYPE_AUDIO_WEBM:
      *content_str = g_strdup ("audio");
      break;
    default:
      *content_str = NULL;
      break;
  }

  switch (mime_type) {
    case GTUBER_STREAM_MIME_TYPE_VIDEO_MP4:
    case GTUBER_STREAM_MIME_TYPE_AUDIO_MP4:
      *mime_str = g_strdup_printf ("%s/%s", *content_str, "mp4");
      break;
    case GTUBER_STREAM_MIME_TYPE_VIDEO_WEBM:
    case GTUBER_STREAM_MIME_TYPE_AUDIO_WEBM:
      *mime_str = g_strdup_printf ("%s/%s", *content_str, "webm");
      break;
    default:
      *mime_str = NULL;
      break;
  }
}

typedef struct
{
  GtuberManifestGenerator *gen;
  GString *string;
} DumpStringData;

static DumpStringData *
dump_string_data_new (GtuberManifestGenerator *gen, GString *string)
{
  DumpStringData *data;

  data = g_new (DumpStringData, 1);
  data->gen = g_object_ref (gen);
  data->string = string;

  return data;
}

static void
dump_string_data_free (DumpStringData *data)
{
  g_object_unref (data->gen);
  g_free (data);
}

static gboolean
get_should_add_adaptive_stream (GtuberManifestGenerator *self,
    GtuberAdaptiveStream *astream, GtuberAdaptiveStreamManifest req_manifest)
{
  GtuberAdaptiveStreamManifest stream_manifest;

  stream_manifest = gtuber_adaptive_stream_get_manifest_type (astream);
  if (stream_manifest != req_manifest)
    return FALSE;

  if (self->filter_func)
    return self->filter_func (astream, self->filter_data);

  return TRUE;
}

static void
add_escaped_xml_uri (GString *string, const gchar *uri_str)
{
  GUri *uri;
  GUriParamsIter iter;
  gchar *attr, *value, *base_uri;
  const gchar *org_query;
  gboolean query_started = FALSE;

  uri = g_uri_parse (uri_str, G_URI_FLAGS_ENCODED, NULL);

  base_uri = g_uri_to_string_partial (uri, G_URI_HIDE_QUERY);
  g_string_append (string, base_uri);
  g_free (base_uri);

  if ((org_query = g_uri_get_query (uri))) {
    g_uri_params_iter_init (&iter, org_query, -1, "&", G_URI_PARAMS_NONE);

    while (g_uri_params_iter_next (&iter, &attr, &value, NULL)) {
      g_string_append_printf (string, "%s%s=%s",
          query_started ? "&amp;" : "?", attr, value);
      query_started = TRUE;

      g_free (attr);
      g_free (value);
    }
  }

  g_uri_unref (uri);
}

static gint
_sort_streams_cb (gconstpointer a, gconstpointer b)
{
  GtuberStream *stream_a, *stream_b;
  guint bitrate_a, bitrate_b;

  stream_a = *((GtuberStream **) a);
  stream_b = *((GtuberStream **) b);

  bitrate_a = gtuber_stream_get_bitrate (stream_a);
  bitrate_b = gtuber_stream_get_bitrate (stream_b);

  return (bitrate_a - bitrate_b);
}

static void
_add_representation_cb (GtuberAdaptiveStream *astream, DumpStringData *data)
{
  GtuberStream *stream;
  gchar *codecs_str;
  guint width, height, fps;
  guint64 start, end;

  stream = GTUBER_STREAM (astream);

  width = gtuber_stream_get_width (stream);
  height = gtuber_stream_get_height (stream);
  fps = gtuber_stream_get_fps (stream);

  /* <Representation> */
  add_line_no_newline (data->gen, data->string, 3, "<Representation");
  add_option_int (data->string, "id", gtuber_stream_get_itag (stream));

  if ((codecs_str = gtuber_stream_obtain_codecs_string (stream))) {
    add_option_string (data->string, "codecs", codecs_str);
    g_free (codecs_str);
  }

  add_option_int (data->string, "bandwidth", gtuber_stream_get_bitrate (stream));

  if (width)
    add_option_int (data->string, "width", width);
  if (height)
    add_option_int (data->string, "height", height);
  if (width && height)
    add_option_string (data->string, "sar", "1:1");
  if (fps)
    add_option_int (data->string, "frameRate", fps);

  finish_line (data->gen, data->string, ">");

  /* TODO: Audio channels config (requires detection in stream):
   * <AudioChannelConfiguration schemeIdUri="urn:mpeg:dash:23003:3:audio_channel_configuration:2011" value="2"/>
   */

  /* <BaseURL> */
  add_line_no_newline (data->gen, data->string, 4, "<BaseURL>");
  add_escaped_xml_uri (data->string, gtuber_stream_get_uri (stream));
  finish_line (data->gen, data->string, "</BaseURL>");

  /* <SegmentBase> */
  add_line_no_newline (data->gen, data->string, 4, "<SegmentBase");
  if (gtuber_adaptive_stream_get_index_range (astream, &start, &end))
    add_option_range (data->string, "indexRange", start, end);
  add_option_boolean (data->string, "indexRangeExact", TRUE);
  finish_line (data->gen, data->string, ">");

  /* <Initialization> */
  add_line_no_newline (data->gen, data->string, 5, "<Initialization");
  if (gtuber_adaptive_stream_get_init_range (astream, &start, &end))
    add_option_range (data->string, "range", start, end);
  finish_line (data->gen, data->string, "/>");

  add_line (data->gen, data->string, 4, "</SegmentBase>");
  add_line (data->gen, data->string, 3, "</Representation>");
}

static void
_add_adaptation_set_cb (DashAdaptationData *adaptation, DumpStringData *data)
{
  gchar *content_str, *mime_str;

  parse_content_and_mime_type (adaptation->mime_type, &content_str, &mime_str);

  if (!content_str || !mime_str) {
    g_debug ("Adaptation is missing contentType or mimeType, ignoring it");
    goto finish;
  }

  add_line_no_newline (data->gen, data->string, 2, "<AdaptationSet");
  add_option_string (data->string, "contentType", content_str);
  add_option_string (data->string, "mimeType", mime_str);
  add_option_boolean (data->string, "subsegmentAlignment", TRUE);
  add_option_int (data->string, "subsegmentStartsWithSAP", 1);
  if (!strcmp (content_str, "video")) {
    gchar *par;

    add_option_int (data->string, "maxWidth", adaptation->max_width);
    add_option_int (data->string, "maxHeight", adaptation->max_height);

    par = obtain_par_from_res (adaptation->max_width, adaptation->max_height);
    add_option_string (data->string, "par", par);
    g_free (par);

    add_option_int (data->string, "maxFrameRate", adaptation->max_fps);
  }
  finish_line (data->gen, data->string, ">");

  /* Sort and add representations */
  g_ptr_array_sort (adaptation->adaptive_streams, (GCompareFunc) _sort_streams_cb);
  g_ptr_array_foreach (adaptation->adaptive_streams, (GFunc) _add_representation_cb, data);

  add_line (data->gen, data->string, 2, "</AdaptationSet>");

finish:
  g_free (content_str);
  g_free (mime_str);
}

static void
_add_hls_stream_cb (GtuberAdaptiveStream *astream, DumpStringData *data)
{
  gboolean add;

  add = get_should_add_adaptive_stream (data->gen, astream,
      GTUBER_ADAPTIVE_STREAM_MANIFEST_HLS);

  if (add) {
    GtuberStream *stream;
    gchar *codecs;
    guint itag, bitrate, width, height, fps;
    gboolean audio_only;

    stream = GTUBER_STREAM (astream);

    itag = gtuber_stream_get_itag (stream);
    bitrate = gtuber_stream_get_bitrate (stream);
    width = gtuber_stream_get_width (stream);
    height = gtuber_stream_get_height (stream);
    fps = gtuber_stream_get_fps (stream);

    audio_only = (width == 0 && height == 0 && fps == 0
        && gtuber_stream_get_video_codec (stream) == NULL);

    /* EXT-X-MEDIA */
    g_string_append (data->string, "#EXT-X-STREAM-INF");
    g_string_append_printf (data->string, ":TYPE=%s",
        audio_only ? "AUDIO" : "VIDEO");
    g_string_append_printf (data->string, ",GROUP-ID=\"%u\"", itag);
    g_string_append_printf (data->string, ",NAME=\"%s\"",
        audio_only ? "audio_only" : "default");
    g_string_append_printf (data->string, ",AUTOSELECT=%s",
        audio_only ? "NO" : "YES");
    g_string_append_printf (data->string, ",DEFAULT=%s",
        audio_only ? "NO" : "YES");
    g_string_append (data->string, "\n");

    /* EXT-X-STREAM-INF */
    g_string_append (data->string, "#EXT-X-STREAM-INF");

    if (bitrate)
      g_string_append_printf (data->string, ":BANDWIDTH=%u", bitrate);
    if (width || height)
      g_string_append_printf (data->string, ",RESOLUTION=%ux%u", width, height);

    codecs = gtuber_stream_obtain_codecs_string (stream);
    if (codecs) {
      g_string_append_printf (data->string, ",CODECS=\"%s\"", codecs);
      g_free (codecs);
    }

    if (!audio_only)
      g_string_append_printf (data->string, ",VIDEO=\"%u\"", itag);
    else
      g_string_append_printf (data->string, ",AUDIO=\"%u\"", itag);

    if (fps)
      g_string_append_printf (data->string, ",FRAME-RATE=%u", fps);

    g_string_append (data->string, "\n");

    /* URI */
    g_string_append_printf (data->string, "%s\n", gtuber_stream_get_uri (stream));
  }
}

static gchar *
obtain_time_as_pts (guint value)
{
  return g_strdup_printf ("PT%uS", value);
}

typedef struct
{
  GtuberManifestGenerator *gen;
  GPtrArray *adaptations;
} SortStreamsData;

static SortStreamsData *
sort_streams_data_new (GtuberManifestGenerator *gen, GPtrArray *adaptations)
{
  SortStreamsData *data;

  data = g_new (SortStreamsData, 1);
  data->gen = g_object_ref (gen);
  data->adaptations = g_ptr_array_ref (adaptations);

  return data;
}

static void
sort_streams_data_free (SortStreamsData *data)
{
  g_object_unref (data->gen);
  g_ptr_array_unref (data->adaptations);
  g_free (data);
}

static void
_sort_dash_adaptations_cb (GtuberAdaptiveStream *astream, SortStreamsData *sort_data)
{
  gboolean add;

  add = get_should_add_adaptive_stream (sort_data->gen, astream,
      GTUBER_ADAPTIVE_STREAM_MANIFEST_DASH);

  if (add) {
    GtuberStream *stream;
    DashAdaptationData *data;

    stream = GTUBER_STREAM (astream);

    data = get_adaptation_data_for_stream (stream, sort_data->adaptations);
    if (data) {
      data->max_width = MAX (data->max_width, gtuber_stream_get_width (stream));
      data->max_height = MAX (data->max_height, gtuber_stream_get_height (stream));
      data->max_fps = MAX (data->max_fps, gtuber_stream_get_fps (stream));

      g_ptr_array_add (data->adaptive_streams, g_object_ref (astream));
    }
  }
}

static gboolean
dump_dash_data (GtuberManifestGenerator *self, GString *string)
{
  DumpStringData *data;
  SortStreamsData *sort_data;

  GPtrArray *astreams, *adaptations;
  gchar *dur_pts, *buf_pts;
  guint buf_time, duration;

  g_debug ("Generating DASH manifest data...");

  adaptations =
      g_ptr_array_new_with_free_func ((GDestroyNotify) dash_adaptation_data_free);

  astreams = gtuber_media_info_get_adaptive_streams (self->media_info);
  sort_data = sort_streams_data_new (self, adaptations);

  g_ptr_array_foreach (astreams, (GFunc) _sort_dash_adaptations_cb, sort_data);

  sort_streams_data_free (sort_data);

  if (!adaptations->len) {
    g_debug ("Adaptations array is empty");
    goto finish;
  }

  /* <xml> */
  add_line_no_newline (self, string, 0, "<?xml");
  add_option_string (string, "version", "1.0");
  add_option_string (string, "encoding", "UTF-8");
  finish_line (self, string, "?>");

  /* <MPD> */
  duration = gtuber_media_info_get_duration (self->media_info);
  dur_pts = obtain_time_as_pts (duration);

  buf_time = MIN (2, duration);
  buf_pts = obtain_time_as_pts (buf_time);

  add_line_no_newline (self, string, 0, "<MPD");
  add_option_string (string, "xmlns:xsi", "http://www.w3.org/2001/XMLSchema-instance");
  add_option_string (string, "xmlns", "urn:mpeg:dash:schema:mpd:2011");
  add_option_string (string, "xsi:schemaLocation", "urn:mpeg:dash:schema:mpd:2011 DASH-MPD.xsd");
  add_option_string (string, "type", "static");
  add_option_string (string, "mediaPresentationDuration", dur_pts);
  add_option_string (string, "minBufferTime", buf_pts);
  add_option_string (string, "profiles", "urn:mpeg:dash:profile:isoff-on-demand:2011");
  finish_line (self, string, ">");

  g_free (dur_pts);
  g_free (buf_pts);

  /* <Period> */
  add_line (self, string, 1, "<Period>");

  data = dump_string_data_new (self, string);
  g_ptr_array_foreach (adaptations, (GFunc) _add_adaptation_set_cb, data);
  dump_string_data_free (data);

  add_line (self, string, 1, "</Period>");
  add_line_no_newline (self, string, 0, "</MPD>");

  g_debug ("DASH manifest data generated");

finish:
  g_ptr_array_unref (adaptations);

  return (string->len > 0);
}

static gboolean
dump_hls_data (GtuberManifestGenerator *self, GString *string)
{
  DumpStringData *data;
  GPtrArray *astreams, *sorted_astreams;

  g_debug ("Generating HLS manifest data...");

  astreams = gtuber_media_info_get_adaptive_streams (self->media_info);

  /* Copy pointers only as we need to sort streams, not modify them */
  sorted_astreams = g_ptr_array_copy (astreams, (GCopyFunc) g_object_ref, NULL);

  g_ptr_array_sort (sorted_astreams, (GCompareFunc) _sort_streams_cb);

  data = dump_string_data_new (self, string);
  g_ptr_array_foreach (sorted_astreams, (GFunc) _add_hls_stream_cb, data);
  dump_string_data_free (data);

  g_ptr_array_unref (sorted_astreams);

  if (string->len == 0) {
    g_debug ("No HLS streams added");
    return FALSE;
  }

  /* Prepend header now that we know data is not empty */
  g_string_prepend (string, "#EXTM3U\n");

  g_debug ("HLS manifest data generated");

  return TRUE;
}

static gboolean
get_allows_type (GtuberManifestGenerator *self,
    GtuberAdaptiveStreamManifest possible_type)
{
  return (self->manifest_type == GTUBER_ADAPTIVE_STREAM_MANIFEST_UNKNOWN
      || self->manifest_type == possible_type);
}

static gchar *
gen_to_data_internal (GtuberManifestGenerator *self, gsize *length)
{
  GString *string;
  gboolean success = FALSE;

  g_return_val_if_fail (GTUBER_IS_MANIFEST_GENERATOR (self), NULL);
  g_return_val_if_fail (self->media_info != NULL, NULL);

  string = g_string_new ("");

  if (!success && get_allows_type (self, GTUBER_ADAPTIVE_STREAM_MANIFEST_DASH))
    success = dump_dash_data (self, string);
  if (!success && get_allows_type (self, GTUBER_ADAPTIVE_STREAM_MANIFEST_HLS))
    success = dump_hls_data (self, string);

  if (length)
    *length = string->len;

  return g_string_free (string, string->len == 0);
}

/**
 * gtuber_manifest_generator_new:
 *
 * Creates a new #GtuberManifestGenerator instance.
 *
 * Returns: (transfer full): a new #GtuberManifestGenerator instance.
 */
GtuberManifestGenerator *
gtuber_manifest_generator_new (void)
{
  return g_object_new (GTUBER_TYPE_MANIFEST_GENERATOR, NULL);
}

/**
 * gtuber_manifest_generator_get_pretty:
 * @gen: a #GtuberManifestGenerator
 *
 * Returns: %TRUE if pretty printing is enabled, %FALSE otherwise.
 */
gboolean
gtuber_manifest_generator_get_pretty (GtuberManifestGenerator *self)
{
  g_return_val_if_fail (GTUBER_IS_MANIFEST_GENERATOR (self), FALSE);

  return self->pretty;
}

/**
 * gtuber_manifest_generator_set_pretty:
 * @gen: a #GtuberManifestGenerator
 * @pretty: %TRUE to enable pretty printing, %FALSE otherwise
 *
 * Sets the pretty printing mode.
 */
void
gtuber_manifest_generator_set_pretty (GtuberManifestGenerator *self, gboolean pretty)
{
  g_return_if_fail (GTUBER_IS_MANIFEST_GENERATOR (self));

  self->pretty = pretty;
}

/**
 * gtuber_manifest_generator_get_indent:
 * @gen: a #GtuberManifestGenerator
 *
 * Returns: amount of indent in spaces for prettifying.
 **/
guint
gtuber_manifest_generator_get_indent (GtuberManifestGenerator *self)
{
  g_return_val_if_fail (GTUBER_IS_MANIFEST_GENERATOR (self), 0);

  return self->indent;
}

/**
 * gtuber_manifest_generator_set_duration:
 * @gen: a #GtuberManifestGenerator
 * @indent: amount of spaces
 *
 * Sets the amount of indent in spaces for prettifying.
 **/
void
gtuber_manifest_generator_set_indent (GtuberManifestGenerator *self, guint indent)
{
  g_return_if_fail (GTUBER_IS_MANIFEST_GENERATOR (self));

  self->indent = indent;
}

/**
 * gtuber_manifest_generator_get_manifest_type:
 * @gen: a #GtuberManifestGenerator
 *
 * Returns: a #GtuberAdaptiveStreamManifest representing
 *   requested type of the manifest to generate.
 */
GtuberAdaptiveStreamManifest
gtuber_manifest_generator_get_manifest_type (GtuberManifestGenerator *self)
{
  g_return_val_if_fail (GTUBER_IS_MANIFEST_GENERATOR (self),
      GTUBER_ADAPTIVE_STREAM_MANIFEST_UNKNOWN);

  return self->manifest_type;
}

/**
 * gtuber_manifest_generator_set_manifest_type:
 * @gen: a #GtuberManifestGenerator
 * @type: a #GtuberAdaptiveStreamManifest
 *
 * Sets the adaptive stream manifest type to use for generation.
 *
 * Generator will only try to generate data of the requested type.
 * Adaptive streams array must include streams supporting this
 * type for generation to be successful.
 *
 * Setting this option to %GTUBER_ADAPTIVE_STREAM_MANIFEST_UNKNOWN,
 * results in auto behavior (i.e. any possible manifest type will be generated).
 * Do that if your app does not distinguish (supports all) types.
 */
void
gtuber_manifest_generator_set_manifest_type (GtuberManifestGenerator *self,
    GtuberAdaptiveStreamManifest type)
{
  g_return_if_fail (GTUBER_IS_MANIFEST_GENERATOR (self));

  self->manifest_type = type;
}

/**
 * gtuber_manifest_generator_set_media_info:
 * @gen: a #GtuberManifestGenerator
 * @info: a #GtuberMediaInfo
 *
 * Set media info used to generate the manifest data.
 * Generator will take a reference on the passed media info object.
 */
void
gtuber_manifest_generator_set_media_info (GtuberManifestGenerator *self, GtuberMediaInfo *info)
{
  g_return_if_fail (GTUBER_IS_MANIFEST_GENERATOR (self));
  g_return_if_fail (GTUBER_IS_MEDIA_INFO (info));

  if (self->media_info)
    g_object_unref (self->media_info);

  self->media_info = g_object_ref (info);
}

/**
 * gtuber_manifest_generator_set_filter_func:
 * @gen: a #GtuberManifestGenerator
 * @filter: (nullable): the filter function to use
 * @user_data: (nullable): user data passed to the filter function
 * @destroy: (nullable): destroy notifier for @user_data
 *
 * Sets the #GtuberAdaptiveStream filtering function.
 *
 * The filter function will be called for each #GtuberAdaptiveStream
 * that generator considers adding during manifest generation.
 */
void
gtuber_manifest_generator_set_filter_func (GtuberManifestGenerator *self,
    GtuberAdaptiveStreamFilter filter, gpointer user_data, GDestroyNotify destroy)
{
  g_return_if_fail (GTUBER_IS_MANIFEST_GENERATOR (self));
  g_return_if_fail (filter || (user_data == NULL && !destroy));

  if (self->filter_destroy)
    self->filter_destroy (self->filter_data);

  self->filter_func = filter;
  self->filter_data = user_data;
  self->filter_destroy = destroy;
}

/**
 * gtuber_manifest_generator_to_data:
 * @gen: a #GtuberManifestGenerator
 *
 * Generates manifest data from #GtuberMediaInfo currently set
 * in #GtuberManifestGenerator and returns it as a buffer.
 *
 * Returns: (transfer full): a newly allocated string holding manifest data.
 */
gchar *
gtuber_manifest_generator_to_data (GtuberManifestGenerator *self)
{
  return gen_to_data_internal (self, NULL);
}

/**
 * gtuber_manifest_generator_to_file:
 * @gen: a #GtuberManifestGenerator
 * @filename: (type filename): the path to the target file
 * @error: return location for a #GError, or %NULL
 *
 * Generates manifest data from #GtuberMediaInfo currently set
 * in #GtuberManifestGenerator and puts it inside `filename`,
 * overwriting the file's current contents.
 *
 * This operation is atomic, in the sense that the data is written to
 * a temporary file which is then renamed to the given `filename`.
 *
 * Returns: %TRUE if saving was successful, %FALSE otherwise.
 */
gboolean
gtuber_manifest_generator_to_file (GtuberManifestGenerator *self,
    const gchar *filename, GError **error)
{
  gchar *data;
  gsize len;
  gboolean success;

  g_return_val_if_fail (GTUBER_IS_MANIFEST_GENERATOR (self), FALSE);
  g_return_val_if_fail (filename != NULL, FALSE);

  if (!(data = gen_to_data_internal (self, &len))) {
    g_set_error (error, GTUBER_MANIFEST_GENERATOR_ERROR,
        GTUBER_MANIFEST_GENERATOR_ERROR_NO_DATA,
        "No data was generated");
    return FALSE;
  }

  success = g_file_set_contents (filename, data, len, error);
  g_free (data);

  return success;
}
