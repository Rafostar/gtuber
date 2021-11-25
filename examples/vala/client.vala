static void
print_stream_info (Gtuber.Stream stream)
{
  stdout.printf ("VIDEO CODEC: %s\n", stream.video_codec);
  stdout.printf ("AUDIO CODEC: %s\n", stream.audio_codec);
  stdout.printf ("RESOLUTION: %ux%ux%u\n", stream.width, stream.height, stream.fps);
  stdout.printf ("URI: %s\n\n", stream.uri);
}

static void
print_media_info (Gtuber.MediaInfo info)
{
  stdout.printf ("TITLE: %s\n", info.title);
  stdout.printf ("DURATION: %u\n\n", info.duration);

  var streams = info.get_streams ();
  var adaptive_streams = info.get_adaptive_streams ();

  stdout.printf ("STREAMS: %i\n", streams.length);
  stdout.printf ("ADAPTIVE STREAMS: %i\n\n", adaptive_streams.length);

  foreach (Gtuber.Stream stream in streams)
    print_stream_info (stream);
  foreach (Gtuber.Stream stream in adaptive_streams)
    print_stream_info (stream);
}

int
main (string[] argv)
{
  if (argv.length < 2) {
    stderr.printf ("Error: No URI privided as argument!\n");
    return 1;
  }

  var client = new Gtuber.Client ();

  try {
    var info = client.fetch_media_info (argv[1]);
    print_media_info (info);
  } catch (Error e) {
    stderr.printf ("Error: %s\n", e.message);
  }

  return 0;
}
