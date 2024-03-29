/*
 * Copyright (C) 2021 Rafał Dzięgiel <rafostar.github@gmail.com>
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

#pragma once

#if !defined(__GTUBER_INSIDE__) && !defined(GTUBER_COMPILATION)
#error "Only <gtuber/gtuber.h> and <gtuber/gtuber-plugin-devel.h> can be included directly."
#endif

#include <gtuber/gtuber-adaptive-stream.h>

G_BEGIN_DECLS

GtuberAdaptiveStream * gtuber_adaptive_stream_new                 (void);

void                   gtuber_adaptive_stream_set_manifest_type   (GtuberAdaptiveStream *stream, GtuberAdaptiveStreamManifest type);

void                   gtuber_adaptive_stream_set_init_range      (GtuberAdaptiveStream *stream, guint64 start, guint64 end);

void                   gtuber_adaptive_stream_set_index_range     (GtuberAdaptiveStream *stream, guint64 start, guint64 end);

G_END_DECLS
