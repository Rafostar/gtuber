# Plugins
all_plugins = [
  'bilibili',
  'invidious',
  'lbry',
  'piped',
  'twitch',
  'youtube',
]

# Shared deps
json_glib_dep = dependency('json-glib-1.0', version: '>=1.2.0', required: false)

build_plugins = []
foreach name : all_plugins
  plugin_option = get_option(name)
  if not plugin_option.disabled()
    name_upper = name.substring(0, 1).to_upper() + name.substring(1)
    plugin_deps = [gtuber_dep]
    plugin_sources = ['gtuber-' + name + '.c']
    plugin_c_args = ['-DG_LOG_DOMAIN="Gtuber' + name_upper + '"']
    subdir(name)
  endif
  summary(name, build_plugins.contains(name) ? 'Yes' : 'No', section: 'Plugins')
endforeach
