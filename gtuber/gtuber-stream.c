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
 * SECTION:gtuber-stream
 * @title: GtuberStream
 * @short_description: represents a direct media stream
 */

/**
 * SECTION:gtuber-stream-devel
 * @title: GtuberStream Development
 */

#include "gtuber-stream.h"
#include "gtuber-stream-devel.h"
#include "gtuber-stream-private.h"

enum
{
  PROP_0,
  PROP_URI,
  PROP_ITAG,
  PROP_MIME_TYPE,
  PROP_CODEC_FLAGS,
  PROP_VIDEO_CODEC,
  PROP_AUDIO_CODEC,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_FPS,
  PROP_BITRATE,
  PROP_LAST
};

#define parent_class gtuber_stream_parent_class
G_DEFINE_TYPE (GtuberStream, gtuber_stream, G_TYPE_OBJECT);

static GParamSpec *param_specs[PROP_LAST] = { NULL, };

static void gtuber_stream_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec);
static void gtuber_stream_finalize (GObject *object);

static void
gtuber_stream_init (GtuberStream *self)
{
  self = gtuber_stream_get_instance_private (self);

  self->uri = NULL;
  self->itag = 0;
  self->mime_type = GTUBER_STREAM_MIME_TYPE_UNKNOWN;
  self->width = 0;
  self->height = 0;
  self->fps = 0;
  self->bitrate = 0;

  self->vcodec = NULL;
  self->acodec = NULL;
}

static void
gtuber_stream_class_init (GtuberStreamClass *klass)
{
  GObjectClass *gobject_class = (GObjectClass *) klass;

  gobject_class->get_property = gtuber_stream_get_property;
  gobject_class->finalize = gtuber_stream_finalize;

  param_specs[PROP_URI] = g_param_spec_string ("uri",
      "Stream URI", "The URI leading to stream", NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_ITAG] = g_param_spec_uint ("itag",
     "Itag", "Stream identifier", 0, G_MAXUINT, 0,
     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_MIME_TYPE] = g_param_spec_enum ("mime-type",
      "Stream MIME Type", "The MIME type of the stream", GTUBER_TYPE_STREAM_MIME_TYPE,
      GTUBER_STREAM_MIME_TYPE_UNKNOWN, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_CODEC_FLAGS] = g_param_spec_flags ("codec-flags",
      "Codec Flags", "Flags indicating which codecs were detected in stream",
      GTUBER_TYPE_CODEC_FLAGS, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_VIDEO_CODEC] = g_param_spec_string ("video-codec",
      "Video Codec", "The stream video codec", NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_AUDIO_CODEC] = g_param_spec_string ("audio-codec",
      "Audio Codec", "The stream audio codec", NULL,
      G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_WIDTH] = g_param_spec_uint ("width",
     "Width", "Stream video width", 0, G_MAXUINT, 0,
     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_HEIGHT] = g_param_spec_uint ("height",
     "Height", "Stream video height", 0, G_MAXUINT, 0,
     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_FPS] = g_param_spec_uint ("fps",
     "FPS", "Stream video framerate", 0, G_MAXUINT, 0,
     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  param_specs[PROP_BITRATE] = g_param_spec_uint ("bitrate",
     "Bitrate", "Stream bitrate (bandwidth)", 0, G_MAXUINT, 0,
     G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties (gobject_class, PROP_LAST, param_specs);
}

static void
gtuber_stream_get_property (GObject *object, guint prop_id,
    GValue *value, GParamSpec *pspec)
{
  GtuberStream *self = GTUBER_STREAM (object);

  switch (prop_id) {
    case PROP_URI:
      g_value_set_string (value, gtuber_stream_get_uri (self));
      break;
    case PROP_ITAG:
      g_value_set_uint (value, gtuber_stream_get_itag (self));
      break;
    case PROP_MIME_TYPE:
      g_value_set_enum (value, gtuber_stream_get_mime_type (self));
      break;
    case PROP_CODEC_FLAGS:
      g_value_set_flags (value, gtuber_stream_get_codec_flags (self));
      break;
    case PROP_VIDEO_CODEC:
      g_value_set_string (value, gtuber_stream_get_video_codec (self));
      break;
    case PROP_AUDIO_CODEC:
      g_value_set_string (value, gtuber_stream_get_audio_codec (self));
      break;
    case PROP_WIDTH:
      g_value_set_uint (value, gtuber_stream_get_width (self));
      break;
    case PROP_HEIGHT:
      g_value_set_uint (value, gtuber_stream_get_height (self));
      break;
    case PROP_FPS:
      g_value_set_uint (value, gtuber_stream_get_fps (self));
      break;
    case PROP_BITRATE:
      g_value_set_uint (value, gtuber_stream_get_bitrate (self));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gtuber_stream_finalize (GObject *object)
{
  GtuberStream *self = GTUBER_STREAM (object);

  g_debug ("Stream finalize, itag: %u", self->itag);

  g_free (self->uri);

  g_free (self->vcodec);
  g_free (self->acodec);

  G_OBJECT_CLASS (parent_class)->finalize (object);
}

/**
 * gtuber_stream_new:
 *
 * Creates a new #GtuberStream instance.
 *
 * This is mainly useful for plugin development.
 *
 * Returns: (transfer full): a new #GtuberStream instance.
 */
GtuberStream *
gtuber_stream_new (void)
{
  return g_object_new (GTUBER_TYPE_STREAM, NULL);
}

/**
 * gtuber_stream_get_uri:
 * @stream: a #GtuberStream
 *
 * Returns: (transfer none): URI of the stream.
 */
const gchar *
gtuber_stream_get_uri (GtuberStream *self)
{
  g_return_val_if_fail (GTUBER_IS_STREAM (self), NULL);

  return self->uri;
}

/**
 * gtuber_stream_set_uri:
 * @stream: a #GtuberStream
 * @uri: an URI
 *
 * Sets the stream URI.
 *
 * This is mainly useful for plugin development.
 */
void
gtuber_stream_set_uri (GtuberStream *self, const gchar *uri)
{
  g_return_if_fail (GTUBER_IS_STREAM (self));

  g_free (self->uri);
  self->uri = g_strdup (uri);
}

/**
 * gtuber_stream_get_itag:
 * @stream: a #GtuberStream
 *
 * Returns: itag of the stream or 0 when undetermined.
 */
guint
gtuber_stream_get_itag (GtuberStream *self)
{
  g_return_val_if_fail (GTUBER_IS_STREAM (self), 0);

  return self->itag;
}

/**
 * gtuber_stream_set_itag:
 * @stream: a #GtuberStream
 * @itag: an itag
 *
 * Sets the stream itag. Used to identify stream among others.
 *
 * This is mainly useful for plugin development.
 */
void
gtuber_stream_set_itag (GtuberStream *self, guint itag)
{
  g_return_if_fail (GTUBER_IS_STREAM (self));

  self->itag = itag;
}

/**
 * gtuber_stream_get_mime_type:
 * @stream: a #GtuberStream
 *
 * Returns: a #GtuberStreamMimeType representing
 *   MIME type of the stream.
 */
GtuberStreamMimeType
gtuber_stream_get_mime_type (GtuberStream *self)
{
  g_return_val_if_fail (GTUBER_IS_STREAM (self), 0);

  return self->mime_type;
}

/**
 * gtuber_stream_set_mime_type:
 * @stream: a #GtuberStream
 * @mime_type: a #GtuberStreamMimeType
 *
 * Sets the stream MIME type.
 *
 * This is mainly useful for plugin development.
 */
void
gtuber_stream_set_mime_type (GtuberStream *self, GtuberStreamMimeType mime_type)
{
  g_return_if_fail (GTUBER_IS_STREAM (self));

  self->mime_type = mime_type;
}

/**
 * gtuber_stream_get_codec_flags:
 * @stream: a #GtuberStream
 *
 * Get flags indicating which codecs were detected in stream.
 *
 * Returns: #GtuberCodecFlags flags
 */
GtuberCodecFlags
gtuber_stream_get_codec_flags (GtuberStream *self)
{
  const gchar *vcodec, *acodec;
  GtuberCodecFlags flags = 0;

  g_return_val_if_fail (GTUBER_IS_STREAM (self), flags);

  vcodec = gtuber_stream_get_video_codec (self);
  if (vcodec) {
    if (g_str_has_prefix (vcodec, "avc"))
      flags |= GTUBER_CODEC_AVC;
    else if (g_str_has_prefix (vcodec, "vp9"))
      flags |= GTUBER_CODEC_VP9;
    else if (g_str_has_prefix (vcodec, "hev"))
      flags |= GTUBER_CODEC_HEVC;
    else if (g_str_has_prefix (vcodec, "av01"))
      flags |= GTUBER_CODEC_AV1;
    else
      flags |= GTUBER_CODEC_UNKNOWN_VIDEO;
  }

  acodec = gtuber_stream_get_audio_codec (self);
  if (acodec) {
    if (g_str_has_prefix (acodec, "mp4a"))
      flags |= GTUBER_CODEC_MP4A;
    else if (g_str_has_prefix (acodec, "opus"))
      flags |= GTUBER_CODEC_OPUS;
    else
      flags |= GTUBER_CODEC_UNKNOWN_AUDIO;
  }

  return flags;
}

/**
 * gtuber_stream_obtain_codecs_string:
 * @stream: a #GtuberStream
 *
 * Obtains a single string with all codecs used to encode the stream.
 * Multiple codecs will be separated by commas.
 *
 * Returns: (transfer full): codecs string or %NULL when none.
 */
gchar *
gtuber_stream_obtain_codecs_string (GtuberStream *self)
{
  const gchar *vcodec, *acodec;
  gchar *codecs_str;

  g_return_val_if_fail (GTUBER_IS_STREAM (self), NULL);

  vcodec = gtuber_stream_get_video_codec (self);
  acodec = gtuber_stream_get_audio_codec (self);

  codecs_str = (vcodec && acodec)
      ? g_strdup_printf ("%s,%s", vcodec, acodec)
      : (vcodec)
      ? g_strdup (vcodec)
      : g_strdup (acodec);

  return codecs_str;
}

/**
 * gtuber_stream_set_codecs:
 * @stream: a #GtuberStream
 * @vcodec: the stream video codec
 * @acodec: the stream audio codec
 *
 * Sets the video and audio codecs used to encode the stream.
 */
void
gtuber_stream_set_codecs (GtuberStream *self,
    const gchar *vcodec, const gchar *acodec)
{
  g_return_if_fail (GTUBER_IS_STREAM (self));

  g_free (self->vcodec);
  g_free (self->acodec);

  self->vcodec = g_strdup (vcodec);
  self->acodec = g_strdup (acodec);
}

/**
 * gtuber_stream_get_video_codec:
 * @stream: a #GtuberStream
 *
 * Returns: (transfer none): the stream video codec.
 */
const gchar *
gtuber_stream_get_video_codec (GtuberStream *self)
{
  g_return_val_if_fail (GTUBER_IS_STREAM (self), NULL);

  return self->vcodec;
}

/**
 * gtuber_stream_set_video_codec:
 * @stream: a #GtuberStream
 * @vcodec: the stream video codec
 *
 * Sets the video codec used to encode the stream.
 *
 * This is mainly useful for plugin development.
 */
void
gtuber_stream_set_video_codec (GtuberStream *self, const gchar *vcodec)
{
  g_return_if_fail (GTUBER_IS_STREAM (self));

  g_free (self->vcodec);
  self->vcodec = g_strdup (vcodec);
}

/**
 * gtuber_stream_get_audio_codec:
 * @stream: a #GtuberStream
 *
 * Returns: (transfer none): the stream audio codec.
 */
const gchar *
gtuber_stream_get_audio_codec (GtuberStream *self)
{
  g_return_val_if_fail (GTUBER_IS_STREAM (self), NULL);

  return self->acodec;
}

/**
 * gtuber_stream_set_audio_codec:
 * @stream: a #GtuberStream
 * @acodec: the stream audio codec
 *
 * Sets the audio codec used to encode the stream.
 *
 * This is mainly useful for plugin development.
 */
void
gtuber_stream_set_audio_codec (GtuberStream *self, const gchar *acodec)
{
  g_return_if_fail (GTUBER_IS_STREAM (self));

  g_free (self->acodec);
  self->acodec = g_strdup (acodec);
}

/**
 * gtuber_stream_get_width:
 * @stream: a #GtuberStream
 *
 * Returns: width of video or 0 when undetermined.
 */
guint
gtuber_stream_get_width (GtuberStream *self)
{
  g_return_val_if_fail (GTUBER_IS_STREAM (self), 0);

  return self->width;
}

/**
 * gtuber_stream_set_width:
 * @stream: a #GtuberStream
 * @width: video width
 *
 * Sets the video stream width.
 *
 * This is mainly useful for plugin development.
 */
void
gtuber_stream_set_width (GtuberStream *self, guint width)
{
  g_return_if_fail (GTUBER_IS_STREAM (self));

  self->width = width;
}

/**
 * gtuber_stream_get_height:
 * @stream: a #GtuberStream
 *
 * Returns: height of video or 0 when undetermined.
 */
guint
gtuber_stream_get_height (GtuberStream *self)
{
  g_return_val_if_fail (GTUBER_IS_STREAM (self), 0);

  return self->height;
}

/**
 * gtuber_stream_set_height:
 * @stream: a #GtuberStream
 * @height: video height
 *
 * Sets the video stream height.
 *
 * This is mainly useful for plugin development.
 */
void
gtuber_stream_set_height (GtuberStream *self, guint height)
{
  g_return_if_fail (GTUBER_IS_STREAM (self));

  self->height = height;
}

/**
 * gtuber_stream_get_fps:
 * @stream: a #GtuberStream
 *
 * Returns: framerate of video or 0 when undetermined.
 */
guint
gtuber_stream_get_fps (GtuberStream *self)
{
  g_return_val_if_fail (GTUBER_IS_STREAM (self), 0);

  return self->fps;
}

/**
 * gtuber_stream_set_fps:
 * @stream: a #GtuberStream
 * @fps: video framerate
 *
 * Sets the video stream framerate.
 *
 * This is mainly useful for plugin development.
 */
void
gtuber_stream_set_fps (GtuberStream *self, guint fps)
{
  g_return_if_fail (GTUBER_IS_STREAM (self));

  self->fps = fps;
}

/**
 * gtuber_stream_get_bitrate:
 * @stream: a #GtuberStream
 *
 * Returns: bitrate of stream or 0 when undetermined.
 */
guint
gtuber_stream_get_bitrate (GtuberStream *self)
{
  g_return_val_if_fail (GTUBER_IS_STREAM (self), 0);

  return self->bitrate;
}

/**
 * gtuber_stream_set_bitrate:
 * @stream: a #GtuberStream
 * @bitrate: bitrate
 *
 * Sets the stream bitrate.
 *
 * This is mainly useful for plugin development.
 */
void
gtuber_stream_set_bitrate (GtuberStream *self, guint bitrate)
{
  g_return_if_fail (GTUBER_IS_STREAM (self));

  self->bitrate = bitrate;
}
