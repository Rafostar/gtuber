const { Gtuber } = imports.gi;

let client = new Gtuber.Client();
let info;

try {
  if (!ARGV[0])
    throw new Error("No URI privided as argument!");

  info = client.fetch_media_info(ARGV[0], null);
} catch(err) {
  logError(err);
}

if (info) {
  log(`TITLE: ${info.title}`);
  log(`DURATION: ${info.duration}`);

  let streams = info.get_streams();
  let adaptiveStreams = info.get_adaptive_streams();

  log(`STREAMS: ${streams.length}`);
  log(`ADAPTIVE STREAMS: ${adaptiveStreams.length}\n`);

  for(let arr of [streams, adaptiveStreams]) {
    for(let stream of arr) {
      log(`VIDEO CODEC: ${stream.video_codec}`);
      log(`AUDIO CODEC: ${stream.audio_codec}`);
      log(`RESOLUTION: ${stream.width}x${stream.height}@${stream.fps}`);
      log(`URI: ${stream.uri}\n`);
    }
  }
}
