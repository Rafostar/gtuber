all_bins = ['gtuber-dl']
build_bins = []

foreach bin_name : all_bins
  bin_option = get_option(bin_name)
  if not bin_option.disabled()
    subdir(bin_name)
  endif
  summary(bin_name, build_bins.contains(bin_name) ? 'Yes' : 'No', section: 'Bin')
endforeach
