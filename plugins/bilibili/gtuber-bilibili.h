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

#pragma once

#include <gtuber/gtuber-plugin-devel.h>
#include <json-glib/json-glib.h>

G_BEGIN_DECLS

typedef enum
{
  BILIBILI_UNKNOWN,
  BILIBILI_BV,
  BILIBILI_AV,
  BILIBILI_BANGUMI_EP,
  BILIBILI_BANGUMI_SS,
} BilibiliType;

GTUBER_WEBSITE_PLUGIN_DECLARE (Bilibili, bilibili, BILIBILI)

struct _GtuberBilibili
{
  GtuberWebsite parent;

  /* Current temporary ID */
  gchar *video_id;

  /* Parsed from response */
  gchar *bvid;
  guint aid;
  guint cid;

  BilibiliType bili_type;

  gboolean had_info;
};

GtuberFlow bilibili_get_flow_from_plugin_props (GtuberBilibili *self, GError **error);

gchar * bilibili_normal_obtain_info_uri (GtuberBilibili *self, const gchar *id_name);
gchar * bilibili_normal_obtain_media_uri (GtuberBilibili *self);

gchar * bilibili_bangumi_obtain_info_uri (GtuberBilibili *self, const gchar *id_name);
gchar * bilibili_bangumi_obtain_media_uri (GtuberBilibili *self, const gchar *id_name);

GtuberFlow bilibili_normal_parse_info (GtuberBilibili *self, JsonReader *reader, GtuberMediaInfo *info, GError **error);
GtuberFlow bilibili_bangumi_parse_info (GtuberBilibili *self, JsonReader *reader, GtuberMediaInfo *info, GError **error);

void bilibili_set_media_info_id_from_cid (GtuberBilibili *self, GtuberMediaInfo *info);

G_END_DECLS
