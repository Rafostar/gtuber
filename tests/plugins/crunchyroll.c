#include "../tests.h"

GTUBER_TEST_MAIN_START ()

GTUBER_TEST_CASE (1)
{
  GtuberClient *client = gtuber_client_new ();
  GtuberMediaInfo *info, *out_info = NULL;

  info = g_object_new (GTUBER_TYPE_MEDIA_INFO, NULL);

  gtuber_media_info_set_id (info, "G6J0JJ2VR");
  gtuber_media_info_set_title (info, "KONOSUBA -God's blessing on this wonderful world! S01E01 - This Self-Proclaimed Goddess and Reincarnation in Another World!");
  gtuber_media_info_set_duration (info, 1510);

  compare_fetch (client, "https://www.crunchyroll.com/watch/G6J0JJ2VR/this-self-proclaimed-goddess-and-reincarnation-in-another-world", info, &out_info);

  g_assert_true (gtuber_media_info_get_description (out_info) != NULL);

  check_adaptive_streams (out_info);

  g_object_unref (info);
  g_object_unref (out_info);
  g_object_unref (client);
}

/* Dubbing */
GTUBER_TEST_CASE (2)
{
  GtuberClient *client = gtuber_client_new ();
  GtuberMediaInfo *info, *out_info = NULL;

  info = g_object_new (GTUBER_TYPE_MEDIA_INFO, NULL);

  gtuber_media_info_set_id (info, "GRG5XX4PR");
  gtuber_media_info_set_title (info, "KONOSUBA -God's blessing on this wonderful world! S01E01 - ¡La diosa autoproclamada y la reencarnación en otro mundo!");
  gtuber_media_info_set_duration (info, 1510);

  compare_fetch (client, "https://www.crunchyroll.com/es-es/watch/GRG5XX4PR", info, &out_info);

  g_assert_true (gtuber_media_info_get_description (out_info) != NULL);

  check_adaptive_streams (out_info);

  g_assert_true (g_str_has_prefix (gtuber_media_info_get_description (out_info), "Kazuma Satou es un chico de preparatoria otaku"));

  g_object_unref (info);
  g_object_unref (out_info);
}

GTUBER_TEST_MAIN_END ()
