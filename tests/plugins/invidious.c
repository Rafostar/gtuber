#include "../tests.h"

GTUBER_TEST_MAIN_START ()

GTUBER_TEST_CASE (1)
{
  GtuberMediaInfo *info = g_object_new (GTUBER_TYPE_MEDIA_INFO, NULL);

  gtuber_media_info_set_id (info, "BaW_jenozKc");
  gtuber_media_info_set_title (info, "youtube-dl test video \"'/\\ä↭𝕐");
  gtuber_media_info_set_duration (info, 10);
  compare_fetch (client, "https://invidious.snopyta.org/watch?v=BaW_jenozKc", info, NULL);

  g_object_unref (info);
}

GTUBER_TEST_CASE (2)
{
  GtuberMediaInfo *info = NULL;

  compare_fetch (client, "https://vid.puffyan.us/v/BaW_jenozKc", NULL, &info);
  check_streams (info);
  check_adaptive_streams (info);

  g_object_unref (info);
}

GTUBER_TEST_MAIN_END ()
