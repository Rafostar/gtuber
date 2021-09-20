# Helpers
all_helpers = [
  'json',
]

build_helpers = []
foreach helper : all_helpers
  subdir(helper)
  summary(helper, build_helpers.contains(helper) ? 'Yes' : 'No', section: 'Helpers')
endforeach
