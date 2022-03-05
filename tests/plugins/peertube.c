#include "../tests.h"

GTUBER_TEST_MAIN_START ()

GTUBER_TEST_CASE (1)
{
  GtuberMediaInfo *info, *out_info;

  info = g_object_new (GTUBER_TYPE_MEDIA_INFO, NULL);

  gtuber_media_info_set_id (info, "52384");
  gtuber_media_info_set_title (info, "Ditching YouTube: The Smart Move");
  gtuber_media_info_set_duration (info, 485);

  compare_fetch (client, "peertube://open.tube/videos/watch/d261a2a5-8974-4b2c-9cfd-fe25ac337956", info, &out_info);

  check_streams (out_info);

  g_object_unref (info);
  g_object_unref (out_info);
}

GTUBER_TEST_CASE (2)
{
  GtuberMediaInfo *info, *out_info;

  info = g_object_new (GTUBER_TYPE_MEDIA_INFO, NULL);

  gtuber_media_info_set_id (info, "27746");
  gtuber_media_info_set_title (info, "Libera Resistenza Lecco 12.02.2022 - avv. Teresa Rosano");
  gtuber_media_info_set_description (info, "Modifica alla Costituzione: benefici apparenti e gravi pericoli.");
  gtuber_media_info_set_duration (info, 630);

  compare_fetch (client, "peertube://peertube.it/w/tbRjxBQj75URZMhWrT41Ri", info, &out_info);

  check_streams (out_info);
  check_adaptive_streams (out_info);

  g_object_unref (info);
  g_object_unref (out_info);
}

GTUBER_TEST_MAIN_END ()
