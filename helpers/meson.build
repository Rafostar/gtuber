# Helpers
all_helpers = [
  'json',
]

build_helpers = []
foreach name : all_helpers
  name_upper = name.substring(0, 1).to_upper() + name.substring(1)
  helper_deps = [gtuber_dep]
  helper_sources = ['gtuber-helper-' + name + '.c']
  helper_c_args = ['-DG_LOG_DOMAIN="GtuberHelper' + name_upper + '"']
  subdir(name)
  summary(name, build_helpers.contains(name) ? 'Yes' : 'No', section: 'Helpers')
endforeach
