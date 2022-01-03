gst_gtuber_deps = [
  gst_dep,
  gst_base_dep,
  gtuber_dep,
]

foreach dep : gst_gtuber_deps
  if not dep.found()
    if gst_lib_option.enabled()
      error('GStreamer plugin was enabled, but required dependencies were not found')
    endif
    subdir_done()
  endif
endforeach

gst_gtuber_sources = [
  'gstgtuber.c',
  'gstgtuberelement.c',
  'gstgtubersrc.c',
  'gstgtuberbin.c',
  'gstgtuberadaptivebin.c',
  'gstgtuberuridemux.c',
  'gstgtuberdashdemux.c',
  'gstgtuberhlsdemux.c',
]

library('gstgtuber',
  gst_gtuber_sources,
  c_args: gst_gtuber_args,
  include_directories: conf_inc,
  dependencies: gst_gtuber_deps,
  install: true,
  install_dir: gst_plugins_libdir,
)
build_gst_libs += gst_lib_name
