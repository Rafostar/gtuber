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
            [success, vcodec, acodec] = stream.get_codecs()
            if success:
                print("VIDEO CODEC: {}".format(vcodec))
                print("AUDIO CODEC: {}".format(acodec))

            print("RESOLUTION: {}x{}@{}".format(stream.props.width, stream.props.height, stream.props.fps))
            print("URI: {}\n".format(stream.props.uri))
