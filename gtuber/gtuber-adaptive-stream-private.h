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

#ifndef __GTUBER_ADAPTIVE_STREAM_PRIVATE_H__
#define __GTUBER_ADAPTIVE_STREAM_PRIVATE_H__

#include <gtuber/gtuber-adaptive-stream.h>

G_BEGIN_DECLS

struct _GtuberAdaptiveStream
{
  GtuberStream parent;

  GtuberAdaptiveStreamManifest manifest_type;

  guint64 init_start;
  guint64 init_end;

  guint64 index_start;
  guint64 index_end;
};

struct _GtuberAdaptiveStreamClass
{
  GtuberStreamClass parent_class;
};

G_END_DECLS

#endif /* __GTUBER_ADAPTIVE_STREAM_PRIVATE_H__ */
