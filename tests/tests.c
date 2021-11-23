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

  g_test_message ("Tested MediaInfo ID: %s",
      gtuber_media_info_get_id (fetch));

  if (out)
    *out = fetch;
  else
    g_object_unref (fetch);
}

static void
_check_stream_cb (GtuberStream *stream, SoupSession *session)
{
  guint status_code, tries = 2;

  g_assert_nonnull (stream);
  g_assert_cmpuint (gtuber_stream_get_itag (stream), >, 0);

  while (tries--) {
    SoupMessage *msg;
    GInputStream *input_stream;

    msg = soup_message_new ("HEAD", gtuber_stream_get_uri (stream));
    input_stream = soup_session_send (session, msg, NULL, NULL);

    g_object_get (msg, "status-code", &status_code, NULL);

    if (input_stream) {
      g_input_stream_close (input_stream, NULL, NULL);
      g_object_unref (input_stream);
    }
    g_object_unref (msg);

    if (status_code < 400)
      break;
  }

  g_test_message ("Stream itag: %u, status code: %u",
    gtuber_stream_get_itag (stream), status_code);

  if (status_code >= 400)
    g_test_message ("Failed URI: %s", gtuber_stream_get_uri (stream));

  g_assert_cmpuint (status_code, <, 400);
}

static void
_check_adaptive_stream_cb (GtuberAdaptiveStream *astream, SoupSession *session)
{
  _check_stream_cb (GTUBER_STREAM (astream), session);
}

void
check_streams (GtuberMediaInfo *info)
{
  const GPtrArray *streams;
  SoupSession *session;

  g_assert_nonnull (info);

  streams = gtuber_media_info_get_streams (info);
  g_assert_true (streams->len > 0);

  session = soup_session_new ();
  g_ptr_array_foreach ((GPtrArray *) streams, (GFunc) _check_stream_cb, session);
  g_object_unref (session);
}

void
check_adaptive_streams (GtuberMediaInfo *info)
{
  const GPtrArray *astreams;
  SoupSession *session;

  g_assert_nonnull (info);

  astreams = gtuber_media_info_get_adaptive_streams (info);
  g_assert_true (astreams->len > 0);

  session = soup_session_new ();
  g_ptr_array_foreach ((GPtrArray *) astreams, (GFunc) _check_adaptive_stream_cb, session);
  g_object_unref (session);
}

void assert_no_streams (GtuberMediaInfo *info)
{
  const GPtrArray *streams;

  streams = gtuber_media_info_get_streams (info);
  assert_equals_int (streams->len, 0);
}

void assert_no_adaptive_streams (GtuberMediaInfo *info)
{
  const GPtrArray *astreams;

  astreams = gtuber_media_info_get_adaptive_streams (info);
  assert_equals_int (astreams->len, 0);
}
