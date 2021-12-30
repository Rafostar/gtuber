#include "../tests.h"

GTUBER_TEST_MAIN_START ()

/* Direct */
GTUBER_TEST_CASE (1)
{
  GtuberMediaInfo *info, *out_info;

  info = g_object_new (GTUBER_TYPE_MEDIA_INFO, NULL);

  gtuber_media_info_set_id (info, "@Odysee:8/getyouryoutubechannelonodysee:5");
  gtuber_media_info_set_title (info, "Get Your YouTube Channel On Odysee");
  gtuber_media_info_set_duration (info, 21);

  compare_fetch (client, "https://odysee.com/@Odysee:8/getyouryoutubechannelonodysee:5", info, &out_info);
  check_streams (out_info);

  g_object_unref (info);
  g_object_unref (out_info);
}

/* HLS */
GTUBER_TEST_CASE (2)
{
  GtuberMediaInfo *info, *out_info;

  info = g_object_new (GTUBER_TYPE_MEDIA_INFO, NULL);

  gtuber_media_info_set_id (info, "@Odysee#8/call-an-ambulance#6");
  gtuber_media_info_set_title (info, "Call an Ambulance!");
  gtuber_media_info_set_duration (info, 8);

  compare_fetch (client, "lbry://@Odysee#8/call-an-ambulance#6", info, &out_info);
  check_adaptive_streams (out_info);

  g_object_unref (info);
  g_object_unref (out_info);
}

GTUBER_TEST_MAIN_END ()
