#include "../tests.h"

GTUBER_TEST_MAIN_START ()

GTUBER_TEST_CASE (1)
{
  GtuberClient *client = gtuber_client_new ();
  GtuberMediaInfo *info = g_object_new (GTUBER_TYPE_MEDIA_INFO, NULL);

  gtuber_media_info_set_id (info, "BaW_jenozKc");
  gtuber_media_info_set_title (info, "youtube-dl test video \"'/\\ä↭𝕐");
  gtuber_media_info_set_duration (info, 10);
  compare_fetch (client, "https://www.youtube.com/watch?v=BaW_jenozKc", info, NULL);

  g_object_unref (info);
  g_object_unref (client);
}

GTUBER_TEST_CASE (2)
{
  GtuberClient *client = gtuber_client_new ();
  GtuberMediaInfo *info = g_object_new (GTUBER_TYPE_MEDIA_INFO, NULL);

  gtuber_media_info_set_id (info, "_b-2C3KPAM0");
  gtuber_media_info_set_title (info, "[A-made] 變態妍字幕版 太妍 我就是這樣的人");
  gtuber_media_info_set_description (info, "made by Wacom from Korea | 字幕&加油添醋 by TY's Allen | 感謝heylisa00cavey1001同學熱情提供梗及翻譯");
  gtuber_media_info_set_duration (info, 85);
  compare_fetch (client, "https://www.youtube.com/v/_b-2C3KPAM0", info, NULL);

  g_object_unref (info);
  g_object_unref (client);
}

GTUBER_TEST_CASE (3)
{
  GtuberClient *client = gtuber_client_new ();
  GtuberMediaInfo *out_info = NULL;

  compare_fetch (client, "https://youtu.be/BaW_jenozKc", NULL, &out_info);
  check_streams (out_info);
  check_adaptive_streams (out_info);

  g_object_unref (out_info);
  g_object_unref (client);
}

GTUBER_TEST_MAIN_END ()
