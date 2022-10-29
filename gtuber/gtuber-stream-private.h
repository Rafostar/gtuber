/*
 * Copyright (C) 2021 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
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

#include <glib.h>
#include <gtuber/gtuber-stream.h>

G_BEGIN_DECLS

struct _GtuberStream
{
  GObject parent;

  gchar *uri;
  guint itag;
  GtuberStreamMimeType mime_type;
  guint width;
  guint height;
  guint fps;
  guint bitrate;

  gchar *vcodec;
  gchar *acodec;
};

struct _GtuberStreamClass
{
  GObjectClass parent_class;
};

G_END_DECLS
