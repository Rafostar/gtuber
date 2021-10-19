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

#include "gtuber-bilibili.h"
#include "utils/json/gtuber-utils-json.h"

gchar *
bilibili_normal_obtain_info_uri (GtuberBilibili *self, const gchar *id_name)
{
  return g_strdup_printf (
      "https://api.bilibili.com/x/web-interface/view?%s=%s",
      id_name, self->video_id);
}

gchar *
bilibili_normal_obtain_media_uri (GtuberBilibili *self)
{
  return g_strdup_printf (
      "https://api.bilibili.com/x/player/playurl?aid=%i&cid=%i&bvid=%s&qn=0&fnver=0&fnval=80&fourk=1",
      self->aid, self->cid, self->bvid);
}

GtuberFlow
bilibili_normal_parse_info (GtuberBilibili *self, JsonReader *reader,
    GtuberMediaInfo *info, GError **error)
{
  const gchar *title, *description, *redirect;
  GtuberFlow res = GTUBER_FLOW_ERROR;

  /* We need a complete set */
  self->bvid = g_strdup (gtuber_utils_json_get_string (reader, "data", "bvid", NULL));
  self->aid = gtuber_utils_json_get_int (reader, "data", "aid", NULL);
  self->cid = gtuber_utils_json_get_int (reader, "data", "cid", NULL);

  res = bilibili_get_flow_from_plugin_props (self, error);
  if (res == GTUBER_FLOW_ERROR)
    goto finish;

  bilibili_set_media_info_id_from_cid (self, info);

  title = gtuber_utils_json_get_string (reader, "data", "title", NULL);
  gtuber_media_info_set_title (info, title);
  g_debug ("Video title: %s", title);

  description = gtuber_utils_json_get_string (reader, "data", "desc", NULL);
  gtuber_media_info_set_description (info, description);
  g_debug ("Video description: %s", description);

  /* FIXME: Add convenient util to enter arr */
  if (json_reader_read_member (reader, "data")) {
    if (json_reader_read_member (reader, "pages")
        && json_reader_is_array (reader)) {
      gint count = json_reader_count_elements (reader);
      gint i;

      for (i = 0; i < count; i++) {
        if (json_reader_read_element (reader, i)) {
          guint el_cid;

          el_cid = gtuber_utils_json_get_int (reader, "cid", NULL);
          if (el_cid == self->cid) {
            guint duration;

            duration = gtuber_utils_json_get_int (reader, "duration", NULL);
            gtuber_media_info_set_duration (info, duration);

            json_reader_end_element (reader);
            break;
          }
        }
        json_reader_end_element (reader);
      }
    }
    json_reader_end_member (reader);
  }
  json_reader_end_member (reader);

  /* Check redirect and eventually switch to Bangumi */
  redirect = gtuber_utils_json_get_string (reader, "data", "redirect_url", NULL);
  if (redirect) {
    GUri *guri;

    g_debug ("This video redirects to: %s", redirect);
    guri = g_uri_parse (redirect, 0, NULL);

    if (guri) {
      gchar **parts;
      const gchar *path;
      guint i = 0;
      gboolean is_bangumi = FALSE;

      path = g_uri_get_path (guri);
      parts = g_strsplit (path, "/", 0);

      while (parts[i]) {
        if (!is_bangumi && !strcmp (parts[i], "bangumi"))
          is_bangumi = TRUE;

        if (is_bangumi) {
          gboolean found = FALSE;

          if ((found = g_str_has_prefix (parts[i], "ep")))
            self->bili_type = BILIBILI_BANGUMI_EP;
          else if ((found = g_str_has_prefix (parts[i], "ss")))
            self->bili_type = BILIBILI_BANGUMI_SS;

          if (found) {
            g_free (self->video_id);
            self->video_id = g_strdup (parts[i] + 2);

            g_debug ("Updated type: %i, video: %s",
                self->bili_type, self->video_id);
            break;
          }
        }

        i++;
      }

      g_strfreev (parts);
      g_uri_unref (guri);
    }
  }

finish:
  return res;
}
