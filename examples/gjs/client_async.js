const { Gio, GLib, Gtuber } = imports.gi;

let loop = GLib.MainLoop.new(null, false);
let client = new Gtuber.Client();

Gio._promisify(client, 'fetch_media_info_async', 'fetch_media_info_finish');

function printInfo(info)
{
  log(`TITLE: ${info.title}`);
  log(`DURATION: ${info.duration}`);

  let streams = info.get_streams();
  let adaptiveStreams = info.get_adaptive_streams();

  log(`STREAMS: ${streams.length}`);
  log(`ADAPTIVE STREAMS: ${adaptiveStreams.length}\n`);

  for(let arr of [streams, adaptiveStreams]) {
    for(let stream of arr) {
      const [success, vcodec, acodec] = stream.get_codecs();
      if(success) {
        log("VIDEO CODEC: " + vcodec);
        log("AUDIO CODEC: " + acodec);
      }
      log(`RESOLUTION: ${stream.width}x${stream.height}@${stream.fps}`);
      log(`URI: ${stream.uri}\n`);
    }
  }
  loop.quit();
}

function printError(err)
{
  logError(err);
  loop.quit();
}

if (ARGV[0]) {
  client.fetch_media_info_async(ARGV[0], null).then(printInfo).catch(printError);
  loop.run();
} else {
  logError(new Error ("No URI privided as argument!"));
}
