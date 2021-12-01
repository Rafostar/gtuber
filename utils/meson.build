# Utils
all_utils = [
  'common',
  'json',
  'youtube',
]

build_utils = []
foreach name : all_utils
  name_upper = name.substring(0, 1).to_upper() + name.substring(1)
  utils_deps = [gtuber_dep]
  utils_sources = ['gtuber-utils-' + name + '.c']
  utils_c_args = ['-DG_LOG_DOMAIN="GtuberUtils' + name_upper + '"']
  subdir(name)
  summary(name, build_utils.contains(name) ? 'Yes' : 'No', section: 'Utils')
endforeach
