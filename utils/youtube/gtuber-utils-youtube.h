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

#ifndef __GTUBER_UTILS_YOUTUBE_H__
#define __GTUBER_UTILS_YOUTUBE_H__

#include <glib.h>
#include <gtuber/gtuber.h>

G_BEGIN_DECLS

void       gtuber_utils_youtube_parse_mime_type_string           (const gchar *yt_mime, GtuberStreamMimeType *mime_type, gchar **vcodec, gchar **acodec);

void       gtuber_utils_youtube_insert_chapters_from_description (GtuberMediaInfo *info, const gchar *description);

G_END_DECLS

#endif /* __GTUBER_UTILS_YOUTUBE_H__ */
