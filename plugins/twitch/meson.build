# Website
website = 'twitch'

plugin_option = get_option(website)
if plugin_option.disabled()
  subdir_done()
endif

# Dependencies
plugin_deps = [
  dependency('json-glib-1.0', version: '>=1.2.0', required: false)
]

foreach dep : plugin_deps
  if not dep.found()
    if plugin_option.enabled()
      error(website + ' plugin was enabled, but required dependencies were not found')
    endif
    subdir_done()
  endif
endforeach

plugin_sources = [
  'gtuber-' + website + '.c',
]
plugin_c_args = [
  '-DG_LOG_DOMAIN="Gtuber' + website.substring(0, 1).to_upper() + website.substring(1) + '"'
]

library(
  gtuber_plugin_prefix + website,
  plugin_sources,
  dependencies: [gtuber_dep, gtuber_helper_json_dep] + plugin_deps,
  include_directories: conf_inc,
  c_args: plugin_c_args,
  version: gtuber_version,
  install: true,
  install_dir: gtuber_plugins_libdir,
)
build_plugins += website
