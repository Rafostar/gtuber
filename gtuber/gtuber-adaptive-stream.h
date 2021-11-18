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

#ifndef __GTUBER_ADAPTIVE_ADAPTIVE_STREAM_H__
#define __GTUBER_ADAPTIVE_ADAPTIVE_STREAM_H__

#if !defined(__GTUBER_INSIDE__) && !defined(GTUBER_COMPILATION)
#error "Only <gtuber/gtuber.h> and <gtuber/gtuber-plugin-devel.h> can be included directly."
#endif

#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

#define GTUBER_TYPE_ADAPTIVE_STREAM            (gtuber_adaptive_stream_get_type ())
#define GTUBER_IS_ADAPTIVE_STREAM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GTUBER_TYPE_ADAPTIVE_STREAM))
#define GTUBER_IS_ADAPTIVE_STREAM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GTUBER_TYPE_ADAPTIVE_STREAM))
#define GTUBER_ADAPTIVE_STREAM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GTUBER_TYPE_ADAPTIVE_STREAM, GtuberAdaptiveStreamClass))
#define GTUBER_ADAPTIVE_STREAM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GTUBER_TYPE_ADAPTIVE_STREAM, GtuberAdaptiveStream))
#define GTUBER_ADAPTIVE_STREAM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GTUBER_TYPE_ADAPTIVE_STREAM, GtuberAdaptiveStreamClass))

/**
 * GtuberAdaptiveStream:
 *
 * Contains values of peculiar adaptive media stream.
 */
typedef struct _GtuberAdaptiveStream GtuberAdaptiveStream;
typedef struct _GtuberAdaptiveStreamClass GtuberAdaptiveStreamClass;

#ifdef G_DEFINE_AUTOPTR_CLEANUP_FUNC
G_DEFINE_AUTOPTR_CLEANUP_FUNC (GtuberAdaptiveStream, g_object_unref)
#endif

GType                            gtuber_adaptive_stream_get_type            (void);

GtuberAdaptiveStreamManifest     gtuber_adaptive_stream_get_manifest_type   (GtuberAdaptiveStream *stream);

gboolean                         gtuber_adaptive_stream_get_init_range      (GtuberAdaptiveStream *stream, guint64 *start, guint64 *end);

gboolean                         gtuber_adaptive_stream_get_index_range     (GtuberAdaptiveStream *stream, guint64 *start, guint64 *end);

G_END_DECLS

#endif /* __GTUBER_ADAPTIVE_STREAM_H__ */
