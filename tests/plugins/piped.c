#include "../tests.h"

GTUBER_TEST_MAIN_START ()

GTUBER_TEST_CASE (1)
{
  GtuberClient *client = gtuber_client_new ();
  GtuberMediaInfo *info = g_object_new (GTUBER_TYPE_MEDIA_INFO, NULL);

  gtuber_media_info_set_id (info, "BaW_jenozKc");
  gtuber_media_info_set_title (info, "youtube-dl test video \"'/\\ä↭𝕐");
  gtuber_media_info_set_duration (info, 10);
  compare_fetch (client, "https://piped.kavin.rocks/watch?v=BaW_jenozKc", info, NULL);

  g_object_unref (info);
  g_object_unref (client);
}

GTUBER_TEST_CASE (2)
{
  GtuberClient *client = gtuber_client_new ();
  GtuberMediaInfo *out_info = NULL;

  compare_fetch (client, "https://piped.kavin.rocks/v/BaW_jenozKc", NULL, &out_info);
  check_streams (out_info);
  check_adaptive_streams (out_info);

  g_object_unref (out_info);
  g_object_unref (client);
}

GTUBER_TEST_MAIN_END ()
