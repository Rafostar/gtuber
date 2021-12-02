gtuber_utils_youtube_dep = dependency('', required: false)

utils_option = get_option('utils-' + name)
if utils_option.disabled()
  subdir_done()
endif

utils_deps += [
  gtuber_utils_common_dep
]
foreach dep : utils_deps
  if not dep.found()
    if utils_option.enabled()
      error(name + ' utils were enabled, but required dependencies were not found')
    endif
    subdir_done()
  endif
endforeach

gtuber_utils_youtube_dep = declare_dependency(
  link_with: library(
    gtuber_utils_prefix + name + gtuber_version_suffix,
    utils_sources,
    dependencies: utils_deps,
    include_directories: conf_inc,
    c_args: utils_c_args,
    version: gtuber_version,
    install: true,
  )
)
build_utils += name
