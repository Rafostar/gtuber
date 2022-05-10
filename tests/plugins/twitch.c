#include "../tests.h"

GTUBER_TEST_MAIN_START ()

/* VOD */
GTUBER_TEST_CASE (1)
{
  GtuberClient *client = gtuber_client_new ();
  GtuberMediaInfo *info, *out_info = NULL;

  info = g_object_new (GTUBER_TYPE_MEDIA_INFO, NULL);

  gtuber_media_info_set_id (info, "6528877");
  gtuber_media_info_set_title (info, "LCK Summer Split - Week 6 Day 1");
  gtuber_media_info_set_duration (info, 17208);
  compare_fetch (client, "http://www.twitch.tv/videos/6528877", info, &out_info);

  assert_no_streams (out_info);
  check_adaptive_streams (out_info);

  g_object_unref (info);
  g_object_unref (out_info);
  g_object_unref (client);
}

/* Clip */
GTUBER_TEST_CASE (2)
{
  GtuberClient *client = gtuber_client_new ();
  GtuberMediaInfo *info, *out_info = NULL;

  info = g_object_new (GTUBER_TYPE_MEDIA_INFO, NULL);

  gtuber_media_info_set_id (info, "42850523");
  gtuber_media_info_set_title (info, "EA Play 2016 Live from the Novo Theatre");
  compare_fetch (client, "https://www.twitch.tv/ea/clip/FaintLightGullWholeWheat", info, &out_info);

  assert_no_adaptive_streams (out_info);
  check_streams (out_info);

  g_object_unref (info);
  g_object_unref (out_info);
  g_object_unref (client);
}

GTUBER_TEST_MAIN_END ()
