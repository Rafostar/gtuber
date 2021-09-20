# Helper
helper = 'json'

helper_option = get_option('helper-' + helper)
if helper_option.disabled()
  subdir_done()
endif

# Dependencies
helper_deps = [
  dependency('json-glib-1.0', version: '>=1.2.0', required: false)
]

foreach dep : helper_deps
  if not dep.found()
    if helper_option.enabled()
      error(helper + ' helper was enabled, but required dependencies were not found')
    endif
    subdir_done()
  endif
endforeach

helper_sources = [
  'gtuber-helper-' + helper + '.c',
]
helper_c_args = [
  '-DG_LOG_DOMAIN="GtuberHelper' + helper.substring(0, 1).to_upper() + helper.substring(1) + '"'
]

gtuber_helper_json_dep = declare_dependency(
  link_with: library(
    gtuber_helper_prefix + helper,
    helper_sources,
    dependencies: [gtuber_dep] + helper_deps,
    include_directories: conf_inc,
    c_args: helper_c_args,
    version: gtuber_version,
    install: true,
  )
)
build_helpers += helper
