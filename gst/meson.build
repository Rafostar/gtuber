gst_req = '>= 1.19.1'

gst_dep = dependency('gstreamer-1.0',
  version: gst_req,
  required: false,
)
gst_base_dep = dependency('gstreamer-base-1.0',
  version: gst_req,
  required: false,
)

gst_plugins_libdir = join_paths(prefix, libdir, 'gstreamer-1.0')
gst_gtuber_args = ['-DHAVE_CONFIG_H']

if get_option('default_library') == 'static'
  gst_gtuber_args += ['-DGST_STATIC_COMPILATION']
endif

all_gst_libs = ['gst-gtuber']
build_gst_libs = []

foreach gst_lib_name : all_gst_libs
  gst_lib_option = get_option(gst_lib_name)
  if not gst_lib_option.disabled()
    subdir(gst_lib_name)
  endif
  summary(gst_lib_name, build_gst_libs.contains(gst_lib_name) ? 'Yes' : 'No', section: 'GStreamer')
endforeach
