import gi, sys
gi.require_version('Gtuber', '0.0')
from gi.repository import GLib, Gtuber

if len(sys.argv) < 2:
    print("No URI privided as argument!")
    sys.exit(1)

client = Gtuber.Client()
info = None

try:
    info = client.fetch_media_info(sys.argv[1], None)
except GLib.Error as err:
    print(err.message)

if info:
    print("TITLE: {}".format(info.props.title))
    print("DURATION: {}".format(info.props.duration))

    streams = info.get_streams()
    adaptive_streams = info.get_adaptive_streams()

    print("STREAMS: {}".format(len(streams)))
    print("ADAPTIVE STREAMS: {}\n".format(len(adaptive_streams)))

    for arr in streams, adaptive_streams:
        for stream in arr:
            print("VIDEO CODEC: {}".format(stream.props.video_codec))
            print("AUDIO CODEC: {}".format(stream.props.audio_codec))
            print("RESOLUTION: {}x{}@{}".format(stream.props.width, stream.props.height, stream.props.fps))
            print("URI: {}\n".format(stream.props.uri))
