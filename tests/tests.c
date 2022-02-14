#include "tests.h"

typedef struct
{
  SoupSession *session;
  GHashTable *user_headers;
} CheckStreamData;

static CheckStreamData *
check_stream_data_new (GtuberMediaInfo *info)
{
  CheckStreamData *data;

  data = g_new (CheckStreamData, 1);
  data->session = soup_session_new ();
  data->user_headers = gtuber_media_info_get_request_headers (info);

  if (data->user_headers)
    g_hash_table_ref (data->user_headers);

  return data;
}

static void
check_stream_data_free (CheckStreamData *data)
{
  g_object_unref (data->session);

  if (data->user_headers)
    g_hash_table_unref (data->user_headers);

  g_free (data);
}

void
compare_fetch (GtuberClient *client, const gchar *uri,
    GtuberMediaInfo *expect, GtuberMediaInfo **out)
{
  GtuberMediaInfo *fetch;
  GError *error = NULL;

  fetch = gtuber_client_fetch_media_info (client, uri, NULL, &error);

  if (error)
    g_printerr ("Fetch media info error: %s\n", error->message);

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
_insert_msg_header_cb (const gchar *name, const gchar *value, SoupMessageHeaders *hdrs)
{
  soup_message_headers_replace (hdrs, name, value);
}

static void
_set_msg_req_headers (SoupMessage *msg, GHashTable *user_headers)
{
  SoupMessageHeaders *req_headers;

  req_headers = soup_message_get_request_headers (msg);
  g_hash_table_foreach (user_headers, (GHFunc) _insert_msg_header_cb, req_headers);
}

static void
_check_stream_cb (GtuberStream *stream, CheckStreamData *data)
{
  guint status_code, tries = 2;

  g_assert_nonnull (stream);
  g_assert_cmpuint (gtuber_stream_get_itag (stream), >, 0);

  while (tries--) {
    SoupMessage *msg;
    GInputStream *input_stream;

    msg = soup_message_new ("HEAD", gtuber_stream_get_uri (stream));
    _set_msg_req_headers (msg, data->user_headers);

    input_stream = soup_session_send (data->session, msg, NULL, NULL);

    g_object_get (msg, "status-code", &status_code, NULL);

    if (input_stream) {
      g_input_stream_close (input_stream, NULL, NULL);
      g_object_unref (input_stream);
    }
    g_object_unref (msg);

    g_test_message ("Stream itag: %u, status code: %u",
        gtuber_stream_get_itag (stream), status_code);

    if (status_code < 400)
      break;
  }

  if (status_code >= 400)
    g_test_message ("Failed URI: %s", gtuber_stream_get_uri (stream));

  g_assert_cmpuint (status_code, <, 400);
}

static void
_check_adaptive_stream_cb (GtuberAdaptiveStream *astream, CheckStreamData *data)
{
  _check_stream_cb (GTUBER_STREAM (astream), data);
}

void
check_streams (GtuberMediaInfo *info)
{
  GPtrArray *streams;
  CheckStreamData *data;

  g_assert_nonnull (info);

  streams = gtuber_media_info_get_streams (info);
  g_assert_true (streams->len > 0);

  data = check_stream_data_new (info);
  g_ptr_array_foreach (streams, (GFunc) _check_stream_cb, data);
  check_stream_data_free (data);
}

void
check_adaptive_streams (GtuberMediaInfo *info)
{
  GPtrArray *astreams;
  CheckStreamData *data;

  g_assert_nonnull (info);

  astreams = gtuber_media_info_get_adaptive_streams (info);
  g_assert_true (astreams->len > 0);

  data = check_stream_data_new (info);
  g_ptr_array_foreach (astreams, (GFunc) _check_adaptive_stream_cb, data);
  check_stream_data_free (data);
}

void
assert_no_streams (GtuberMediaInfo *info)
{
  GPtrArray *streams;

  streams = gtuber_media_info_get_streams (info);
  assert_equals_int (streams->len, 0);
}

void
assert_no_adaptive_streams (GtuberMediaInfo *info)
{
  GPtrArray *astreams;

  astreams = gtuber_media_info_get_adaptive_streams (info);
  assert_equals_int (astreams->len, 0);
}
