build_utils = []
name = 'common'

gtuber_utils_dep = dependency('', required: false)

utils_option = get_option('utils-' + name)
if not utils_option.disabled()
  gtuber_utils_dep = declare_dependency(
    link_with: library(
      gtuber_utils_prefix + name + gtuber_version_suffix,
      ['gtuber-utils.c'],
      dependencies: gtuber_dep,
      include_directories: conf_inc,
      c_args: ['-DG_LOG_DOMAIN="GtuberUtils"'],
      version: gtuber_version,
      install: true,
    )
  )
  build_utils += name
endif
summary(name, build_utils.contains(name) ? 'Yes' : 'No', section: 'Utils')

# Other Utils
other_utils = [
  'json',
]

foreach name : other_utils
  name_upper = name.substring(0, 1).to_upper() + name.substring(1)
  utils_deps = [gtuber_dep]
  utils_sources = ['gtuber-utils-' + name + '.c']
  utils_c_args = ['-DG_LOG_DOMAIN="GtuberUtils' + name_upper + '"']
  subdir(name)
  summary(name, build_utils.contains(name) ? 'Yes' : 'No', section: 'Utils')
endforeach
