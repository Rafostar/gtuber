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

#include <glib.h>
#include <gtuber/gtuber.h>

G_BEGIN_DECLS

void       gtuber_utils_youtube_parse_mime_type_string               (const gchar *yt_mime, GtuberStreamMimeType *mime_type, gchar **vcodec, gchar **acodec);

void       gtuber_utils_youtube_insert_chapters_from_description     (GtuberMediaInfo *info, const gchar *description);

gboolean   gtuber_utils_youtube_parse_hls_input_stream               (GInputStream *stream, GtuberMediaInfo *info, GError **error);

gboolean   gtuber_utils_youtube_parse_hls_input_stream_with_base_uri (GInputStream *stream, GtuberMediaInfo *info, const gchar *base_uri, GError **error);

G_END_DECLS
