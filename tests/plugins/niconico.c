#include "../tests.h"

GTUBER_TEST_MAIN_START ()

GTUBER_TEST_CASE (1)
{
  GtuberMediaInfo *info, *out_info;

  info = g_object_new (GTUBER_TYPE_MEDIA_INFO, NULL);

  gtuber_media_info_set_id (info, "sm22312215");
  gtuber_media_info_set_title (info, "Big Buck Bunny");
  gtuber_media_info_set_description (info, "(c) copyright 2008, Blender Foundation / www.bigbuckbunny.org");
  gtuber_media_info_set_duration (info, 33);

  compare_fetch (client, "https://www.nicovideo.jp/watch/sm22312215", info, &out_info);

  check_adaptive_streams (out_info);

  g_object_unref (info);
  g_object_unref (out_info);
}

GTUBER_TEST_MAIN_END ()
