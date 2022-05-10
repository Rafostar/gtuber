#include "../tests.h"

GTUBER_TEST_MAIN_START ()

GTUBER_TEST_CASE (1)
{
  GtuberClient *client = gtuber_client_new ();
  GtuberMediaInfo *info = g_object_new (GTUBER_TYPE_MEDIA_INFO, NULL);

  gtuber_media_info_set_id (info, "zv89llsvexdz");
  gtuber_media_info_set_title (info, "That small heart attack.");
  gtuber_media_info_set_duration (info, 12);

  compare_fetch (client, "https://www.reddit.com/r/videos/comments/6rrwyj/that_small_heart_attack/", info, NULL);

  g_object_unref (info);
  g_object_unref (client);
}

GTUBER_TEST_CASE (2)
{
  GtuberClient *client = gtuber_client_new ();
  GtuberMediaInfo *info = g_object_new (GTUBER_TYPE_MEDIA_INFO, NULL);

  gtuber_media_info_set_id (info, "2wmfee9eycp71");
  gtuber_media_info_set_title (info, "\"Ako Vučić izgubi izbore, ja ću da crknem, Jugoslavija je gotova\"");
  gtuber_media_info_set_duration (info, 134);

  compare_fetch (client, "https://www.redditmedia.com/r/serbia/comments/pu9wbx/ako_vu%C4%8Di%C4%87_izgubi_izbore_ja_%C4%87u_da_crknem/", info, NULL);

  g_object_unref (info);
  g_object_unref (client);
}

GTUBER_TEST_CASE (3)
{
  GtuberClient *client = gtuber_client_new ();
  GtuberMediaInfo *info, *out_info = NULL;

  info = g_object_new (GTUBER_TYPE_MEDIA_INFO, NULL);

  gtuber_media_info_set_id (info, "2c0z8joa9tk81");
  gtuber_media_info_set_title (info, "Just got my Deck! Had to do this for old times' sake.");
  gtuber_media_info_set_duration (info, 10);

  compare_fetch (client, "https://www.reddit.com/r/linux_gaming/comments/t4dvbj", info, &out_info);
  check_adaptive_streams (out_info);

  g_object_unref (info);
  g_object_unref (out_info);
  g_object_unref (client);
}

GTUBER_TEST_MAIN_END ()
