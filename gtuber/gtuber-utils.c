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

#include <glib.h>
#include "gtuber-utils.h"

/**
 * gtuber_utils_get_mime_type_from_string:
 * @string: a null-terminated string
 *
 * Returns: a #GtuberStreamMimeType with
 *   detected MIME type from the string.
 **/
GtuberStreamMimeType
gtuber_utils_get_mime_type_from_string (const gchar *string)
{
  gboolean is_mp4, is_webm;

  is_mp4 = g_str_has_suffix (string, "mp4");
  is_webm = is_mp4 ? FALSE : g_str_has_suffix (string, "webm");

  if (!is_mp4 && !is_webm)
    goto unknown;

  if (g_str_has_prefix (string, "video")) {
    if (is_mp4)
      return GTUBER_STREAM_MIME_TYPE_VIDEO_MP4;
    if (is_webm)
      return GTUBER_STREAM_MIME_TYPE_VIDEO_WEBM;
  } else if (g_str_has_prefix (string, "audio")) {
    if (is_mp4)
      return GTUBER_STREAM_MIME_TYPE_AUDIO_MP4;
    if (is_webm)
      return GTUBER_STREAM_MIME_TYPE_AUDIO_WEBM;
  }

unknown:
  return GTUBER_STREAM_MIME_TYPE_UNKNOWN;
}
