youtube_option = get_option('youtube')

if youtube_option.disabled()
  subdir_done()
endif

json_dep = dependency('json-glib-1.0', version: '>=1.2.0', required: false)

if not json_dep.found()
  if youtube_option.enabled()
    error('The youtube plugin was enabled, but required dependencies were not found.')
  endif
  subdir_done()
endif

youtube_sources = [
  'gtuber-youtube.c',
]
youtube_c_args = [
  '-DG_LOG_DOMAIN="GtuberYoutube"'
]

youtube_lib = library(
  gtuber_plugin_prefix + 'youtube',
  youtube_sources,
  dependencies: [gtuber_dep, json_dep],
  include_directories: conf_inc,
  c_args: youtube_c_args,
  version: gtuber_version,
  install: true,
  install_dir: gtuber_plugins_libdir,
)
