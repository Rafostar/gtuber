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

#ifndef __GTUBER_MEDIA_INFO_DEVEL_H__
#define __GTUBER_MEDIA_INFO_DEVEL_H__

#if !defined(__GTUBER_INSIDE__) && !defined(GTUBER_COMPILATION)
#error "Only <gtuber/gtuber.h> and <gtuber/gtuber-plugin-devel.h> can be included directly."
#endif

#include <gtuber/gtuber-media-info.h>
#include <gtuber/gtuber-stream.h>
#include <gtuber/gtuber-adaptive-stream.h>

G_BEGIN_DECLS

void              gtuber_media_info_set_id                  (GtuberMediaInfo *info, const gchar *id);

void              gtuber_media_info_set_title               (GtuberMediaInfo *info, const gchar *title);

void              gtuber_media_info_set_description         (GtuberMediaInfo *info, const gchar *description);

void              gtuber_media_info_set_duration            (GtuberMediaInfo *info, guint duration);

void              gtuber_media_info_insert_chapter          (GtuberMediaInfo *info, guint64 start, const gchar *name);

void              gtuber_media_info_add_stream              (GtuberMediaInfo *info, GtuberStream *stream);

void              gtuber_media_info_add_adaptive_stream     (GtuberMediaInfo *info, GtuberAdaptiveStream *stream);

G_END_DECLS

#endif /* __GTUBER_MEDIA_INFO_DEVEL_H__ */
