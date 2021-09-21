gtuber_helper_json_dep = dependency('', required: false)

helper_option = get_option('helper-' + name)
if helper_option.disabled()
  subdir_done()
endif

helper_deps += [
  dependency('json-glib-1.0', version: '>=1.2.0', required: false)
]
foreach dep : helper_deps
  if not dep.found()
    if helper_option.enabled()
      error(name + ' helper was enabled, but required dependencies were not found')
    endif
    subdir_done()
  endif
endforeach

gtuber_helper_json_dep = declare_dependency(
  link_with: library(
    gtuber_helper_prefix + name,
    helper_sources,
    dependencies: helper_deps,
    include_directories: conf_inc,
    c_args: helper_c_args,
    version: gtuber_version,
    install: true,
  )
)
build_helpers += name
