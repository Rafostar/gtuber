gtuber_dl_deps = [
  gtuber_dep,
  gst_dep,
]

foreach dep : gtuber_dl_deps
  if not dep.found()
    if bin_option.enabled()
      error('Binary @0@ was enabled, but required dependencies were not found'.format(bin_name))
    endif
    subdir_done()
  endif
endforeach

gtuber_dl_sources = [
  'gtuber-dl.c',
  'gtuber-dl-terminal.c',
]

executable('gtuber-dl',
  gtuber_dl_sources,
  c_args: gst_gtuber_args,
  include_directories: conf_inc,
  dependencies: gtuber_dl_deps,
  install: true,
  install_dir: bindir,
)
build_bins += bin_name
