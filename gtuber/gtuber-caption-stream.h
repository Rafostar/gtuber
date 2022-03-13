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

#pragma once

#if !defined(__GTUBER_INSIDE__) && !defined(GTUBER_COMPILATION)
#error "Only <gtuber/gtuber.h> and <gtuber/gtuber-plugin-devel.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GTUBER_TYPE_CAPTION_STREAM            (gtuber_caption_stream_get_type ())
#define GTUBER_IS_CAPTION_STREAM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTUBER_TYPE_CAPTION_STREAM))
#define GTUBER_IS_CAPTION_STREAM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTUBER_TYPE_CAPTION_STREAM))
#define GTUBER_CAPTION_STREAM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTUBER_TYPE_CAPTION_STREAM, GtuberCaptionStreamClass))
#define GTUBER_CAPTION_STREAM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTUBER_TYPE_CAPTION_STREAM, GtuberCaptionStream))
#define GTUBER_CAPTION_STREAM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTUBER_TYPE_CAPTION_STREAM, GtuberCaptionStreamClass))

/**
 * GtuberCaptionStream:
 *
 * Contains values of peculiar caption media stream.
 */
typedef struct _GtuberCaptionStream GtuberCaptionStream;
typedef struct _GtuberCaptionStreamClass GtuberCaptionStreamClass;

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtuberCaptionStream, g_object_unref)
#endif

GType                            gtuber_caption_stream_get_type            (void);

const gchar *                    gtuber_caption_stream_get_lang_code       (GtuberCaptionStream *stream);

G_END_DECLS
