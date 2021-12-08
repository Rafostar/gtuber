#include "../tests.h"

GTUBER_TEST_MAIN_START ()

/* BV */
GTUBER_TEST_CASE (1)
{
  GtuberMediaInfo *info = g_object_new (GTUBER_TYPE_MEDIA_INFO, NULL);
  GtuberMediaInfo *fetch = NULL;

  gtuber_media_info_set_id (info, "344682149");
  gtuber_media_info_set_title (info, "4621 comercial video!");
  gtuber_media_info_set_duration (info, 107);
  compare_fetch (client, "https://www.bilibili.com/video/BV1Ub4y1o7uJ", info, &fetch);

  check_adaptive_streams (fetch);

  g_object_unref (info);
  g_object_unref (fetch);
}

/* AV */
GTUBER_TEST_CASE (2)
{
  GtuberMediaInfo *info = g_object_new (GTUBER_TYPE_MEDIA_INFO, NULL);
  GtuberMediaInfo *fetch = NULL;

  gtuber_media_info_set_id (info, "1554319");
  gtuber_media_info_set_title (info, "【金坷垃】金泡沫");
  gtuber_media_info_set_duration (info, 308);
  compare_fetch (client, "https://www.bilibili.com/video/av1074402", info, &fetch);

  check_adaptive_streams (fetch);

  g_object_unref (info);
  g_object_unref (fetch);
}

/* Bangumi EP */
GTUBER_TEST_CASE (3)
{
  GtuberMediaInfo *info = g_object_new (GTUBER_TYPE_MEDIA_INFO, NULL);
  GtuberMediaInfo *fetch = NULL;

  gtuber_media_info_set_id (info, "410355895");
  gtuber_media_info_set_title (info, "定档PV 10月30日起不见不散");
  gtuber_media_info_set_duration (info, 82);
  compare_fetch (client, "https://www.bilibili.com/bangumi/play/ep423320", info, &fetch);

  check_adaptive_streams (fetch);

  g_object_unref (info);
  g_object_unref (fetch);
}

GTUBER_TEST_MAIN_END ()
