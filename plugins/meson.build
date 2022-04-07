# Plugins
all_plugins = [
  'bilibili',
  'invidious',
  'lbry',
  'niconico',
  'peertube',
  'piped',
  'reddit',
  'twitch',
  'youtube',
]

# Shared deps
json_glib_dep = dependency('json-glib-1.0', version: '>=1.2.0', required: false)
libxml_dep = dependency('libxml-2.0', version: '>=2.9.0', required: false)

build_plugins = []
foreach name : all_plugins
  plugin_option = get_option(name)
  if not plugin_option.disabled()
    split_name_arr = name.split('-')
    name_upper = ''
    foreach split_name : split_name_arr
      name_upper += split_name.substring(0, 1).to_upper() + split_name.substring(1)
    endforeach
    plugin_deps = [gtuber_dep]
    plugin_sources = ['gtuber-' + name + '.c']
    plugin_c_args = ['-DG_LOG_DOMAIN="Gtuber' + name_upper + '"']
    subdir(name)
  endif
  summary(name, build_plugins.contains(name) ? 'Yes' : 'No', section: 'Plugins')
endforeach
