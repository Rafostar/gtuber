use gtuber::{prelude::*, Client};
use anyhow::Result;
use std::io::{self, Write};

fn print_stream_meta<S: StreamExt>(stream: &S) {
	if let Some(video) = stream.video_codec() {
		println!("Video codec: {}", video);
		println!("Resolution: {}x{}@{}", stream.width(), stream.height(), stream.fps());
	}
	if let Some(audio) = stream.audio_codec() {
		println!("Audio codec: {}", audio);
	}
	if let Some(uri) = stream.uri() {
		println!("URI: {}", uri);
	}
}

fn main() -> Result<()> {
	let mut args = std::env::args();
	let mut stdout = io::stdout();

	let client = Client::new();
	let media = client.fetch_media_info(&args.nth(1).expect("Usage: gtuber <url>"), None::<&gio::Cancellable>)?;
	if let Some(title) = media.title() {
		println!("Title: {}", title);
	}
	println!("Duration: {}s", media.duration());

	let streams = media.streams();
	let adaptive_streams = media.adaptive_streams();
	println!("There are {} streams and {} adaptive streams.", streams.len(), adaptive_streams.len());

	streams.iter().map(|stream| {
		stdout.write(b"\n")?;
		print_stream_meta(stream);
		Ok(())
	}).collect::<Result<()>>()?;
	adaptive_streams.iter().map(|stream| {
		stdout.write(b"\n")?;
		print_stream_meta(stream);
		Ok(())
	}).collect::<Result<()>>()?;

	Ok(())
}
