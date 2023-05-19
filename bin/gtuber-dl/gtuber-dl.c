/*
 * Copyright (C) 2023 Rafał Dzięgiel <rafostar.github@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <gtuber/gtuber.h>
#include <gst/gst.h>
#include <locale.h>

#include "gtuber-dl-terminal.h"

#define MP4_MUX_NAME  "mp4mux"
#define WEBM_MUX_NAME "webmmux"
#define MKV_MUX_NAME  "matroskamux"
#define OPUS_MUX_NAME "avmux_opus"

#define MP4_MUX_FLAGS  (GTUBER_CODEC_AVC | GTUBER_CODEC_AV1 | GTUBER_CODEC_MP4A /* FIXME: | GTUBER_CODEC_OPUS */)
#define WEBM_MUX_FLAGS (GTUBER_CODEC_VP9 | GTUBER_CODEC_OPUS)
#define MKV_MUX_FLAGS  (MP4_MUX_FLAGS | WEBM_MUX_FLAGS | GTUBER_CODEC_HEVC)
#define OPUS_MUX_FLAGS (GTUBER_CODEC_OPUS)

#define GST_CAT_DEFAULT gtuber_dl_debug
GST_DEBUG_CATEGORY_STATIC (GST_CAT_DEFAULT);

typedef struct
{
  gchar *itags;
  gchar *output;
  gboolean print_info;
  gboolean quiet;
  gboolean non_interactive;
  gboolean version;

  gchar **uris;

  gboolean using_stdout;
} GtuberDlArgs;

static void
gtuber_dl_args_free (GtuberDlArgs *dl_args)
{
  g_free (dl_args->itags);
  g_free (dl_args->output);

  g_strfreev (dl_args->uris);

  g_free (dl_args);
}

static void
_print_no_element (const gchar *name)
{
  gst_printerrln ("Error: Could not create \"%s\" element", GST_STR_NULL (name));
}

static void
_print_link_failed (GstElement *el1, GstElement *el2)
{
  gchar *el1_name, *el2_name;

  el1_name = gst_object_get_name (GST_OBJECT (el1));
  el2_name = gst_object_get_name (GST_OBJECT (el2));

  gst_printerrln ("Error: Could not link \"%s\" with \"%s\"",
      GST_STR_NULL (el1_name), GST_STR_NULL (el2_name));

  g_free (el1_name);
  g_free (el2_name);
}

static GtuberMediaInfo *
_fetch_media_info (GtuberDlArgs *dl_args)
{
  GtuberClient *client = gtuber_client_new ();
  GtuberMediaInfo *info;
  GError *error = NULL;
  const gchar *msg = "Fetching media info...";

  GST_INFO ("%s", msg);

  if (!dl_args->quiet)
    gst_print ("%s\r", msg);

  info = gtuber_client_fetch_media_info (client, dl_args->uris[0], NULL, &error);

  if (error) {
    GST_ERROR ("Error: %s", error->message);
    gst_printerrln ("Error: %s", error->message);
  } else {
    GST_INFO ("Fetched media info");
  }

  g_clear_object (&client);
  g_clear_error (&error);

  return info;
}

static gboolean
_same_itag_func (gconstpointer a, gconstpointer b)
{
  GtuberStream *stream = (GtuberStream *) a;
  guint itag = GPOINTER_TO_UINT (b);

  return (gtuber_stream_get_itag (stream) == itag);
}

static const gchar *
_get_file_ext (GtuberDlArgs *dl_args, GtuberMediaInfo *info)
{
  gchar **itags;
  GtuberCodecFlags muxer_flags = 0;
  guint i;

  if (!dl_args->itags)
    return NULL;

  itags = g_strsplit (dl_args->itags, ",", 0);

  for (i = 0; itags[i]; ++i) {
    guint itag;

    g_strstrip (itags[i]);
    itag = g_ascii_strtoull (itags[i], NULL, 10);

    if (itag > 0) {
      GPtrArray *streams, *astreams;
      GtuberStream *stream = NULL;
      guint index = -1;

      streams = gtuber_media_info_get_streams (info);
      astreams = gtuber_media_info_get_adaptive_streams (info);

      if (g_ptr_array_find_with_equal_func (astreams,
          GUINT_TO_POINTER (itag), (GEqualFunc) _same_itag_func, &index)) {
        stream = GTUBER_STREAM (g_ptr_array_index (astreams, index));
      } else if (g_ptr_array_find_with_equal_func (streams,
          GUINT_TO_POINTER (itag), (GEqualFunc) _same_itag_func, &index)) {
        stream = GTUBER_STREAM (g_ptr_array_index (streams, index));
      }

      if (stream)
        muxer_flags |= gtuber_stream_get_codec_flags (stream);
    }
  }

  g_strfreev (itags);

  return ((GTUBER_CODEC_MP4A & muxer_flags) == muxer_flags)
      ? ".m4a"
      : ((OPUS_MUX_FLAGS & muxer_flags) == muxer_flags)
      ? ".opus"
      : ((MP4_MUX_FLAGS & muxer_flags) == muxer_flags)
      ? ".mp4"
      : ((WEBM_MUX_FLAGS & muxer_flags) == muxer_flags)
      ? ".webm"
      : ".mkv";
}

static const gchar *
_get_mux_name (GtuberDlArgs *dl_args, GtuberMediaInfo *info, gboolean *audio_only)
{
  const gchar *ext = NULL;

  if (dl_args->output)
    ext = strrchr (dl_args->output, '.');
  if (!ext)
    ext = _get_file_ext (dl_args, info);

  if (audio_only)
    *audio_only = (!g_strcmp0 (ext, ".opus") || (!g_strcmp0 (ext, ".m4a")));

  return (!g_strcmp0 (ext, ".mkv"))
      ? MKV_MUX_NAME
      : (!g_strcmp0 (ext, ".webm"))
      ? WEBM_MUX_NAME
      : (!g_strcmp0 (ext, ".opus"))
      ? OPUS_MUX_NAME
      : MP4_MUX_NAME;
}

static void
update_filename (GtuberDlArgs *dl_args, GtuberMediaInfo *info)
{
  GFile *dir, *out_file;
  gchar *dir_path, *out_path;

  dir_path = g_get_current_dir ();
  dir = g_file_new_for_path (dir_path);

  if (dl_args->output) {
    out_file = g_file_resolve_relative_path (dir, dl_args->output);
  } else {
    const gchar *title;
    gchar *name_with_ext;

    if (!(title = gtuber_media_info_get_title (info)))
      title = gtuber_media_info_get_id (info);

    name_with_ext = g_strdup_printf ("%s%s",
        (title) ? title : "gtuber_dl", _get_file_ext (dl_args, info));
    out_file = g_file_resolve_relative_path (dir, name_with_ext);

    g_free (name_with_ext);
  }

  out_path = g_file_get_path (out_file);

  g_object_unref (dir);
  g_object_unref (out_file);
  g_free (dir_path);

  g_free (dl_args->output);
  dl_args->output = out_path;

  GST_INFO ("Output file path: %s", dl_args->output);
}

static GtuberStream *
_select_better_quality (GtuberStream *candidate, GtuberStream *prev)
{
  if (!prev
      || gtuber_stream_get_height (candidate) > gtuber_stream_get_height (prev)
      || gtuber_stream_get_width (candidate) > gtuber_stream_get_width (prev)
      || gtuber_stream_get_fps (candidate) > gtuber_stream_get_fps (prev)
      || gtuber_stream_get_codec_flags (candidate) > gtuber_stream_get_codec_flags (prev)
      || gtuber_stream_get_bitrate (candidate) > gtuber_stream_get_bitrate (prev))
    return candidate;

  return prev;
}

static gboolean
_can_be_muxed (GtuberStream *stream, const gchar *mux_name)
{
  GtuberCodecFlags stream_flags = gtuber_stream_get_codec_flags (stream);
  GtuberCodecFlags muxer_flags;
  gboolean can_mux;

  /* Apply flags depending on what media container supports */
  muxer_flags = (!strcmp (mux_name, MP4_MUX_NAME))
      ? MP4_MUX_FLAGS
      : (!strcmp (mux_name, WEBM_MUX_NAME))
      ? WEBM_MUX_FLAGS
      : (!strcmp (mux_name, MKV_MUX_NAME))
      ? MKV_MUX_FLAGS
      : (!strcmp (mux_name, OPUS_MUX_NAME))
      ? OPUS_MUX_FLAGS
      : (GTUBER_CODEC_UNKNOWN_VIDEO | GTUBER_CODEC_UNKNOWN_AUDIO);

  can_mux = ((stream_flags & muxer_flags) > 0);

  GST_LOG ("Can%s mux itag: %u", (can_mux) ? "" : "not",
      gtuber_stream_get_itag (stream));

  return can_mux;
}

static void
_update_best_streams (GtuberStream *candidate, const gchar *mux_name,
    gboolean audio_only, GtuberStream **best_v, GtuberStream **best_a,
    GtuberStream **best_va)
{
  guint width, height, fps;
  const gchar *vcodec, *acodec;
  gboolean has_video, has_audio;

  if (!_can_be_muxed (candidate, mux_name))
    return;

  width = gtuber_stream_get_width (candidate);
  height = gtuber_stream_get_height (candidate);
  fps = gtuber_stream_get_fps (candidate);
  vcodec = gtuber_stream_get_video_codec (candidate);
  acodec = gtuber_stream_get_audio_codec (candidate);

  has_video = (width > 0 || height > 0 || fps > 0 || vcodec != NULL);
  has_audio = (acodec != NULL);

  if (has_video && has_audio && !audio_only)
    *best_va = _select_better_quality (candidate, *best_va);
  else if (has_video && !audio_only)
    *best_v = _select_better_quality (candidate, *best_v);
  else if (has_audio)
    *best_a = _select_better_quality (candidate, *best_a);
}

static gchar *
_determine_itags (GtuberDlArgs *dl_args, GtuberMediaInfo *info)
{
  GPtrArray *astreams, *streams;
  GtuberStream *best_v = NULL, *best_a = NULL, *best_va = NULL;
  const gchar *mux_name;
  gboolean audio_only = FALSE;
  guint i;

  if (!dl_args->non_interactive) {
    gtuber_dl_terminal_print_formats (info);
    return gtuber_dl_terminal_read_itags ();
  }

  mux_name = _get_mux_name (dl_args, info, &audio_only);
  astreams = gtuber_media_info_get_adaptive_streams (info);
  streams = gtuber_media_info_get_adaptive_streams (info);

  for (i = 0; i < streams->len; ++i) {
    GtuberStream *stream = GTUBER_STREAM (g_ptr_array_index (streams, i));
    _update_best_streams (stream, mux_name, audio_only, &best_v, &best_a, &best_va);
  }
  for (i = 0; i < astreams->len; ++i) {
    GtuberStream *stream = GTUBER_STREAM (g_ptr_array_index (astreams, i));
    _update_best_streams (stream, mux_name, audio_only, &best_v, &best_a, &best_va);
  }

  return (audio_only && best_a)
      ? g_strdup_printf ("%u", gtuber_stream_get_itag (best_a))
      : (best_v && best_a)
      ? g_strdup_printf ("%u,%u", gtuber_stream_get_itag (best_v), gtuber_stream_get_itag (best_a))
      : (best_va)
      ? g_strdup_printf ("%u", gtuber_stream_get_itag (best_va))
      : NULL;
}

static void
parsebin_pad_added_cb (GstElement *parsebin, GstPad *pad, GstElement *pipeline)
{
  GstElement *queue, *mux;
  GstPad *queue_sink_pad = NULL, *queue_src_pad = NULL, *mux_sink_pad = NULL;

  if (!GST_PAD_IS_SRC (pad))
    return;

  if (gst_debug_category_get_threshold (GST_CAT_DEFAULT) >= GST_LEVEL_DEBUG) {
    GstCaps *caps = gst_pad_get_current_caps (pad);

    GST_DEBUG ("Linking parsebin new pad, caps: %" GST_PTR_FORMAT, caps);
    gst_clear_caps (&caps);
  }

  queue = gst_element_factory_make ("queue", NULL);
  mux = gst_bin_get_by_name (GST_BIN (pipeline), "mux");

  if (!queue) {
    _print_no_element ("queue");
    goto finish;
  }

  g_object_set (queue,
      "max-size-time", 5 * GST_SECOND,
      "max-size-bytes", 20 * 1024 * 1024,
      "max-size-buffers", 0,
      NULL);

  gst_bin_add (GST_BIN (pipeline), queue);
  gst_element_sync_state_with_parent (queue);

  queue_sink_pad = gst_element_get_static_pad (queue, "sink");
  queue_src_pad = gst_element_get_static_pad (queue, "src");
  mux_sink_pad = gst_element_get_compatible_pad (mux, pad, NULL);

  if (!queue_sink_pad || !queue_src_pad || !mux_sink_pad) {
    gst_printerrln ("Error: Some pads are missing");
    goto finish;
  }

  if (GST_PAD_LINK_FAILED (gst_pad_link (pad, queue_sink_pad))) {
    _print_link_failed (parsebin, queue);
    goto finish;
  }
  if (GST_PAD_LINK_FAILED (gst_pad_link (queue_src_pad, mux_sink_pad))) {
    _print_link_failed (queue, mux);
    goto finish;
  }

  GST_DEBUG ("Parsebin pad linked");

finish:
  gst_clear_object (&queue_sink_pad);
  gst_clear_object (&queue_src_pad);
  gst_clear_object (&mux_sink_pad);

  gst_clear_object (&mux);
}

static GstElement *
make_pipeline (GtuberDlArgs *dl_args, GtuberMediaInfo *info)
{
  GstElement *pipeline, *src, *parsebin, *mux, *sink;
  const gchar *mux_name = NULL, *sink_name = NULL;

  GST_INFO ("Creating pipeline");

  mux_name = _get_mux_name (dl_args, info, NULL);
  GST_DEBUG ("Using muxer: %s", mux_name);

  sink_name = (dl_args->using_stdout) ? "fdsink" : "filesink";
  GST_DEBUG ("Using sink: %s", sink_name);

  src = gst_element_factory_make ("gtubersrc", "src");
  parsebin = gst_element_factory_make ("parsebin", NULL);
  mux = gst_element_factory_make (mux_name, "mux");
  sink = gst_element_factory_make (sink_name, "sink");

  if (!src || !parsebin || !mux || !sink)
    goto fail_create;

  g_object_set (src,
      "media-info", info,
      "itags", dl_args->itags,
      "codecs", 0,
      NULL);

  if (!dl_args->using_stdout)
    g_object_set (sink, "location", dl_args->output, NULL);

  pipeline = gst_pipeline_new (g_get_prgname ());
  gst_bin_add_many (GST_BIN (pipeline), src, parsebin, mux, sink, NULL);

  if (!gst_element_link (src, parsebin)) {
    _print_link_failed (src, parsebin);
    goto fail_link;
  }
  if (!gst_element_link (mux, sink)) {
    _print_link_failed (mux, sink);
    goto fail_link;
  }

  g_signal_connect (parsebin, "pad-added", G_CALLBACK (parsebin_pad_added_cb), pipeline);

  GST_INFO ("Created pipeline");

  return pipeline;

fail_link:
  gst_printerrln ("Error: Could not link elements");
  gst_object_unref (pipeline);

  return NULL;

fail_create:
  if (!src)
    _print_no_element ("gtubersrc");
  if (!parsebin)
    _print_no_element ("parsebin");
  if (!mux)
    _print_no_element (mux_name);
  if (!sink)
    _print_no_element (sink_name);

  gst_clear_object (&src);
  gst_clear_object (&parsebin);
  gst_clear_object (&mux);
  gst_clear_object (&sink);

  return NULL;
}

typedef struct
{
  GstElement *pipeline;
  GMainLoop *loop;
  GError *error;
} GtuberDlDownloader;

static gboolean
bus_watch_cb (GstBus *bus, GstMessage *msg, GtuberDlDownloader *downloader)
{
  switch (GST_MESSAGE_TYPE (msg)) {
    case GST_MESSAGE_BUFFERING:{
      gint percent = 0;
      gst_message_parse_buffering (msg, &percent);
      gst_element_set_state (downloader->pipeline,
          (percent < 100) ? GST_STATE_PAUSED : GST_STATE_PLAYING);
      break;
    }
    case GST_MESSAGE_ERROR:
      gst_message_parse_error (msg, &downloader->error, NULL);
      G_GNUC_FALLTHROUGH;
    case GST_MESSAGE_EOS:
      g_main_loop_quit (downloader->loop);
      G_GNUC_FALLTHROUGH;
    default:
      break;
  }

  return TRUE;
}

static gboolean
progress_watch_cb (GtuberDlDownloader *downloader)
{
  gint64 time = 0;

  if (gst_element_query_position (downloader->pipeline, GST_FORMAT_PERCENT, &time))
    gst_print ("Downloading... %5.1lf%%\r", (gdouble) time / GST_FORMAT_PERCENT_SCALE);

  return TRUE;
}

static gboolean
download (GstElement *pipeline, GtuberDlArgs *dl_args, GError **error)
{
  GtuberDlDownloader *downloader = g_new0 (GtuberDlDownloader, 1);
  GstBus *bus;
  GSource *progress_source;
  GstStateChangeReturn ret;
  guint bus_watch_id;

  GST_INFO ("Starting download...");
  ret = gst_element_set_state (pipeline, GST_STATE_PLAYING);

  if (ret == GST_STATE_CHANGE_FAILURE) {
    g_set_error (error, GST_CORE_ERROR, GST_CORE_ERROR_STATE_CHANGE,
        "Cannot change pipeline state");
    return FALSE;
  }

  downloader->pipeline = pipeline;
  downloader->loop = g_main_loop_new (NULL, FALSE);

  bus = gst_element_get_bus (pipeline);
  bus_watch_id = gst_bus_add_watch (bus, (GstBusFunc) bus_watch_cb, downloader);
  gst_object_unref (bus);

  progress_source = g_timeout_source_new (100);
  g_source_set_callback (progress_source, (GSourceFunc) progress_watch_cb, downloader, NULL);
  g_source_attach (progress_source, NULL);

  GST_INFO ("Downloading...");
  g_main_loop_run (downloader->loop);
  GST_INFO ("Download finishing...");

  g_source_destroy (progress_source);
  g_source_unref (progress_source);
  g_source_remove (bus_watch_id);

  *error = downloader->error;
  g_main_loop_unref (downloader->loop);
  g_free (downloader);

  gst_element_set_state (pipeline, GST_STATE_NULL);
  GST_INFO ("Download %s", (*error == NULL) ? "successful" : "failed");

  if (*error)
    return FALSE;

  if (!dl_args->quiet)
    gst_println ("\33[2KDownloaded");

  return TRUE;
}

static gint
gtuber_dl_main (gint argc, gchar **argv)
{
  GtuberDlArgs *dl_args = g_new0 (GtuberDlArgs, 1);
  GtuberMediaInfo *info = NULL;
  GstElement *pipeline = NULL;

  GOptionEntry options[] = {
    { "info", 'I', 0, G_OPTION_ARG_NONE, &dl_args->print_info, "Print media info and exit", NULL },
    { "itags", 'i', 0, G_OPTION_ARG_STRING, &dl_args->itags, "A comma separated list of itags to download", NULL },
    { "non-interactive", 'n', 0, G_OPTION_ARG_NONE, &dl_args->non_interactive, "Auto select itags for download without user prompt", NULL },
    { "output", 'o', 0, G_OPTION_ARG_FILENAME, &dl_args->output, "Download location", NULL },
    { "quiet", 'q', 0, G_OPTION_ARG_NONE, &dl_args->quiet, "Disable terminal printing", NULL },
    { "version", 0, 0, G_OPTION_ARG_NONE, &dl_args->version, "Print version information and exit", NULL },
    { G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, &dl_args->uris, "Media URI", NULL },
    { NULL },
  };
  GOptionContext *ctx;
  GError *error = NULL;
  gboolean ret = 0;

  GST_DEBUG_CATEGORY_INIT (GST_CAT_DEFAULT, "gtuber-dl", GST_DEBUG_FG_RED, "Gtuber DL");

  g_set_prgname ("gtuber-dl");
  setlocale (LC_ALL, "");

  ctx = g_option_context_new ("URI");
  g_option_context_add_main_entries (ctx, options, NULL);
  g_option_context_add_group (ctx, gst_init_get_option_group ());

#ifdef G_OS_WIN32
  g_option_context_parse_strv (ctx, &argv, &error);
#else
  g_option_context_parse (ctx, &argc, &argv, &error);
#endif
  g_option_context_free (ctx);

  GST_INFO ("Start");

  if (error)
    goto finish;

  if (dl_args->version) {
    gst_println ("%s %s", g_get_prgname (), GTUBER_VERSION_S);
    goto finish;
  }

  if (!dl_args->uris) {
    gst_printerrln ("No URI argument, see --help for how to use");
    goto finish;
  }

  if (!(info = _fetch_media_info (dl_args)))
    goto finish;

  if (dl_args->print_info) {
    gtuber_dl_terminal_print_formats (info);
    goto finish;
  }

  dl_args->using_stdout = (g_strcmp0 (dl_args->output, "-") == 0);

  if (dl_args->using_stdout)
    dl_args->quiet = TRUE;

  if (!dl_args->itags) {
    if (!(dl_args->itags = _determine_itags (dl_args, info))) {
      gst_printerrln ("Could not determine itags to download");
      goto finish;
    }
  }
  GST_INFO ("Using itags: %s", dl_args->itags);

  if (!dl_args->using_stdout)
    update_filename (dl_args, info);

  if (!(pipeline = make_pipeline (dl_args, info)))
    goto finish;

  download (pipeline, dl_args, &error);

finish:
  if (error) {
    gst_printerrln ("Error: %s",
        (error->message) ? error->message : "Unknown error occurred");
    ret = 1;

    g_error_free (error);
  }

  gst_clear_object (&pipeline);
  g_clear_object (&info);
  gtuber_dl_args_free (dl_args);

  GST_INFO ("Finish");

  return ret;
}

gint
main (gint argc, gchar *argv[])
{
  gint ret;

#ifdef G_OS_WIN32
  argv = g_win32_get_command_line ();
#endif

#if defined(__APPLE__) && TARGET_OS_MAC && !TARGET_OS_IPHONE
  ret = gst_macos_main ((GstMainFunc) gtuber_dl_main, argc, argv, NULL);
#else
  ret = gtuber_dl_main (argc, argv);
#endif

#ifdef G_OS_WIN32
  g_strfreev (argv);
#endif

  return ret;
}
