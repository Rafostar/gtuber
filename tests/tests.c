#include "tests.h"

void
compare_fetch (GtuberClient *client, const gchar *uri,
    GtuberMediaInfo *expect, GtuberMediaInfo **out)
{
  GtuberMediaInfo *fetch;
  GError *error = NULL;

  fetch = gtuber_client_fetch_media_info (client, uri, NULL, &error);

  g_assert_null (error);
  g_assert_nonnull (fetch);
  g_assert_true (
      gtuber_media_info_get_has_streams (fetch)
      || gtuber_media_info_get_has_adaptive_streams (fetch));

  if (expect) {
    if (gtuber_media_info_get_id (expect))
      assert_equals_string (
          gtuber_media_info_get_id (fetch),
          gtuber_media_info_get_id (expect));

    if (gtuber_media_info_get_title (expect))
      assert_equals_string (
          gtuber_media_info_get_title (fetch),
          gtuber_media_info_get_title (expect));

    if (gtuber_media_info_get_description (expect))
      assert_equals_string (
          gtuber_media_info_get_description (fetch),
          gtuber_media_info_get_description (expect));

    if (gtuber_media_info_get_duration (expect))
      assert_equals_int (
          gtuber_media_info_get_duration (fetch),
          gtuber_media_info_get_duration (expect));
  }

  if (out)
    *out = fetch;
  else
    g_object_unref (fetch);
}
