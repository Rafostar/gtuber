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

#ifndef __GTUBER_STREAM_H__
#define __GTUBER_STREAM_H__

#if !defined(__GTUBER_INSIDE__) && !defined(GTUBER_COMPILATION)
#error "Only <gtuber/gtuber.h> and <gtuber/gtuber-plugin-devel.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>

#include <gtuber/gtuber-enums.h>

G_BEGIN_DECLS

#define GTUBER_TYPE_STREAM            (gtuber_stream_get_type ())
#define GTUBER_IS_STREAM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTUBER_TYPE_STREAM))
#define GTUBER_IS_STREAM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTUBER_TYPE_STREAM))
#define GTUBER_STREAM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTUBER_TYPE_STREAM, GtuberStreamClass))
#define GTUBER_STREAM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTUBER_TYPE_STREAM, GtuberStream))
#define GTUBER_STREAM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTUBER_TYPE_STREAM, GtuberStreamClass))

/**
 * GtuberStream:
 *
 * Contains values of peculiar media stream.
 */
typedef struct _GtuberStream GtuberStream;
typedef struct _GtuberStreamClass GtuberStreamClass;

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtuberStream, g_object_unref)
#endif

GType gtuber_stream_get_type                            (void);

const gchar *        gtuber_stream_get_uri              (GtuberStream *stream);

guint                gtuber_stream_get_itag             (GtuberStream *stream);

GtuberStreamMimeType gtuber_stream_get_mime_type        (GtuberStream *stream);

GtuberCodecFlags     gtuber_stream_get_codec_flags      (GtuberStream *stream);

gchar *              gtuber_stream_obtain_codecs_string (GtuberStream *stream);

const gchar *        gtuber_stream_get_video_codec      (GtuberStream *stream);

const gchar *        gtuber_stream_get_audio_codec      (GtuberStream *stream);

guint                gtuber_stream_get_width            (GtuberStream *stream);

guint                gtuber_stream_get_height           (GtuberStream *stream);

guint                gtuber_stream_get_fps              (GtuberStream *stream);

guint                gtuber_stream_get_bitrate          (GtuberStream *stream);

G_END_DECLS

#endif /* __GTUBER_STREAM_H__ */
