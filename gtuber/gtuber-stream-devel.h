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

#ifndef __GTUBER_STREAM_DEVEL_H__
#define __GTUBER_STREAM_DEVEL_H__

#if !defined(__GTUBER_INSIDE__) && !defined(GTUBER_COMPILATION)
#error "Only <gtuber/gtuber.h> and <gtuber/gtuber-plugin-devel.h> can be included directly."
#endif

#include <gtuber/gtuber-stream.h>

G_BEGIN_DECLS

GtuberStream *       gtuber_stream_new              (void);

void                 gtuber_stream_set_uri          (GtuberStream *stream, const gchar *uri);

void                 gtuber_stream_set_itag         (GtuberStream *stream, guint itag);

void                 gtuber_stream_set_mime_type    (GtuberStream *stream, GtuberStreamMimeType mime_type);

void                 gtuber_stream_set_codecs       (GtuberStream *stream, const gchar *vcodec, const gchar *acodec);

void                 gtuber_stream_set_video_codec  (GtuberStream *stream, const gchar *vcodec);

void                 gtuber_stream_set_audio_codec  (GtuberStream *stream, const gchar *acodec);

void                 gtuber_stream_set_width        (GtuberStream *stream, guint width);

void                 gtuber_stream_set_height       (GtuberStream *stream, guint height);

void                 gtuber_stream_set_fps          (GtuberStream *stream, guint fps);

void                 gtuber_stream_set_bitrate      (GtuberStream *stream, guint bitrate);

G_END_DECLS

#endif /* __GTUBER_STREAM_DEVEL_H__ */
