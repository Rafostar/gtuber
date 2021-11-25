#include <gtuber/gtuber.h>

static void
print_stream_info (GtuberStream *stream, gpointer user_data)
{
  g_print ("VIDEO CODEC: %s\n", gtuber_stream_get_video_codec (stream));
  g_print ("AUDIO CODEC: %s\n", gtuber_stream_get_audio_codec (stream));
  g_print ("RESOLUTION: %ix%ix%i\n",
      gtuber_stream_get_width (stream),
      gtuber_stream_get_height (stream),
      gtuber_stream_get_fps (stream));
  g_print ("URI: %s\n\n", gtuber_stream_get_uri (stream));
}

static void
print_adaptive_stream_info (GtuberAdaptiveStream *adaptive_stream, gpointer user_data)
{
  print_stream_info (GTUBER_STREAM (adaptive_stream), user_data);
}

int
main (int argc, char **argv)
{
  GtuberClient *client;
  GtuberMediaInfo *info;
  GError *error = NULL;

  if (!argv[1]) {
    g_printerr ("No URI privided as argument!\n");
    return 1;
  }

  client = gtuber_client_new ();
  info = gtuber_client_fetch_media_info (client, argv[1], NULL, &error);

  if (error) {
    g_printerr ("ERROR: %s\n", error->message);
    g_error_free (error);
  }
  if (info) {
    GPtrArray *streams, *adaptive_streams;

    g_print ("TITLE: %s\n", gtuber_media_info_get_title (info));
    g_print ("DURATION: %u\n\n", gtuber_media_info_get_duration (info));

    streams = gtuber_media_info_get_streams (info);
    adaptive_streams = gtuber_media_info_get_adaptive_streams (info);

    g_print ("STREAMS: %i\n", streams->len);
    g_print ("ADAPTIVE STREAMS: %i\n\n", adaptive_streams->len);

    g_ptr_array_foreach (streams, (GFunc) print_stream_info, NULL);
    g_ptr_array_foreach (adaptive_streams, (GFunc) print_adaptive_stream_info, NULL);

    g_object_unref (info);
  }
  g_object_unref (client);

  return 0;
}
