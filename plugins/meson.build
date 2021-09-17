# Plugins
all_plugins = [
  'youtube',
]

build_plugins = []
foreach plugin : all_plugins
  subdir(plugin)
  summary(plugin, build_plugins.contains(plugin) ? 'Yes' : 'No', section: 'Plugins')
endforeach
