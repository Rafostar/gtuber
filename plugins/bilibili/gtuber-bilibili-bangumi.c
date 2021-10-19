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
#include "utils/common/gtuber-utils-common.h"
#include "utils/json/gtuber-utils-json.h"

static gboolean
_read_episodes (GtuberBilibili *self, JsonReader *reader,
    GtuberMediaInfo *info, GError **error, GtuberFlow *res)
{
  gint j, episodes = json_reader_count_elements (reader);
  guint search_id = 0;

  /* With episode number find the exact episode info,
   * otherwise knowing season use latest episode */
  if (self->bili_type == BILIBILI_BANGUMI_EP) {
    search_id = g_ascii_strtod (self->video_id, NULL);
    g_debug ("Search ID: %i", search_id);
  }

  /* Find latest published episode of season */
  if (self->bili_type == BILIBILI_BANGUMI_SS) {
    for (j = 0; j < episodes; j++) {
      if (json_reader_read_element (reader, j)) {
        guint pub_time;

        pub_time = gtuber_utils_json_get_int (reader, "pub_time", NULL);
        if (pub_time > search_id) {
          search_id = pub_time;
          g_debug ("Latest publish date: %i", search_id);
        }
      }
      json_reader_end_element (reader);
    }
  }

  for (j = 0; j < episodes; j++) {
    gboolean found_params = FALSE;

    if (json_reader_read_element (reader, j)) {
      guint found_id = (self->bili_type == BILIBILI_BANGUMI_EP)
          ? gtuber_utils_json_get_int (reader, "id", NULL)
          : gtuber_utils_json_get_int (reader, "pub_time", NULL);

      if ((found_params = found_id == search_id)) {
        self->bvid = g_strdup (gtuber_utils_json_get_string (reader, "bvid", NULL));
        self->aid = gtuber_utils_json_get_int (reader, "aid", NULL);
        self->cid = gtuber_utils_json_get_int (reader, "cid", NULL);

        *res = bilibili_get_flow_from_plugin_props (self, error);

        if (*res != GTUBER_FLOW_ERROR) {
          const gchar *title, *l_title;
          guint duration;

          bilibili_set_media_info_id_from_cid (self, info);

          title = gtuber_utils_json_get_string (reader, "title", NULL);
          l_title = gtuber_utils_json_get_string (reader, "long_title", NULL);

          if (title && l_title) {
            gchar *merged_title = g_strdup_printf ("%s %s", title, l_title);
            gtuber_media_info_set_title (info, merged_title);
            g_free (merged_title);
          } else if (title) {
            gtuber_media_info_set_title (info, title);
          }
          g_debug ("Video title: %s", gtuber_media_info_get_title (info));

          /* Bilibili Bangumi has value in milliseconds */
          duration = (gtuber_utils_json_get_int (reader, "duration", NULL) / 1000);
          gtuber_media_info_set_duration (info, duration);
        }
      }
    }
    json_reader_end_element (reader);

    if (found_params)
      return TRUE;
  }

  return FALSE;
}

gchar *
bilibili_bangumi_obtain_info_uri (GtuberBilibili *self, const gchar *id_name)
{
  return g_strdup_printf (
      "https://api.bilibili.com/pgc/view/web/season?%s=%s",
      id_name, self->video_id);
}

gchar *
bilibili_bangumi_obtain_media_uri (GtuberBilibili *self, const gchar *id_name)
{
  return g_strdup_printf (
      "https://api.bilibili.com/pgc/player/web/playurl?avid=%i&cid=%i&bvid=%s&qn=0&fnver=0&fnval=80&fourk=1&%s=%s",
      self->aid, self->cid, self->bvid, id_name, self->video_id);
}

GtuberFlow
bilibili_bangumi_parse_info (GtuberBilibili *self, JsonReader *reader,
    GtuberMediaInfo *info, GError **error)
{
  GtuberFlow res = GTUBER_FLOW_ERROR;
  gboolean found_params = FALSE;
  gint i;

  if (!json_reader_read_member (reader, "result")) {
    /* Close reading result before return */
    json_reader_end_member (reader);

    g_set_error (error, GTUBER_WEBSITE_ERROR,
        GTUBER_WEBSITE_ERROR_PARSE_FAILED,
        "No result data in response");
    goto finish;
  }

  g_debug ("Searching for requested video info...");

  /* Find our video in "section" array -> "episodes" array */
  if (json_reader_read_member (reader, "section")
      && json_reader_is_array (reader)) {
    gint sections = json_reader_count_elements (reader);

    for (i = 0; i < sections; i++) {
      if (json_reader_read_element (reader, i)) {
        if (json_reader_read_member (reader, "episodes")
            && json_reader_is_array (reader)) {
          found_params = _read_episodes (self, reader, info, error, &res);
        }
        json_reader_end_member (reader);
      }
      json_reader_end_element (reader);

      if (found_params)
        break;
    }

    g_debug ("Found info in section->episodes array: %s",
          found_params ? "yes" : "no");
  }
  json_reader_end_member (reader);

  /* Also try another "episodes" array if above failed */
  if (!found_params) {
    if (json_reader_read_member (reader, "episodes")
        && json_reader_is_array (reader)) {
      found_params = _read_episodes (self, reader, info, error, &res);
    }
    json_reader_end_member (reader);

    g_debug ("Found info in episodes array: %s",
          found_params ? "yes" : "no");
  }

  if (found_params) {
    const gchar *description;

    description = gtuber_utils_json_get_string (reader, "evaluate", NULL);
    gtuber_media_info_set_description (info, description);
    g_debug ("Video description: %s", description);
  }

  g_debug ("Parsing video info %ssuccessful", !found_params ? "un" : "");

  /* Finish reading "result" */
  json_reader_end_member (reader);

finish:
  return res;
}
