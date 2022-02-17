plugin_deps += [
  gtuber_utils_common_dep,
  gtuber_utils_json_dep,
  json_glib_dep,
]
foreach dep : plugin_deps
  if not dep.found()
    if plugin_option.enabled()
      error(name + ' plugin was enabled, but required dependencies were not found')
    endif
    subdir_done()
  endif
endforeach

library(
  gtuber_plugin_prefix + name,
  plugin_sources,
  dependencies: plugin_deps,
  include_directories: conf_inc,
  c_args: plugin_c_args,
  version: gtuber_version,
  install: true,
  install_dir: gtuber_plugins_libdir,
)
build_plugins += name
