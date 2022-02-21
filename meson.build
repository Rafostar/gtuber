project('gtuber', 'c',
  version: '0.3.0',
  meson_version: '>=0.53.0',
  license: 'LGPL-2.1-or-later',
  default_options: [
    'warning_level=1',
    'buildtype=debugoptimized',
  ],
)

glib_req = '>=2.68'

gnome = import('gnome')
pkgconfig = import('pkgconfig')

prefix = get_option('prefix')
bindir = get_option('bindir')
datadir = get_option('datadir')
libdir = get_option('libdir')
includedir = get_option('includedir')

cc = meson.get_compiler('c')

add_global_arguments(
  '-DG_LOG_USE_STRUCTURED=1',
  language: 'c',
)

gtuber_version = meson.project_version().split('-')[0]
version_array = gtuber_version.split('.')
gtuber_version_suffix = '-' + version_array[0] + '.0'

gtuber_api_name = meson.project_name() + gtuber_version_suffix
gtuber_plugin_devel_api_name = '@0@-plugin-devel@1@'.format(
  meson.project_name(), gtuber_version_suffix
)

gtuber_utils_prefix = meson.project_name() + '-utils-'
gtuber_plugin_prefix = meson.project_name() + '-'

gtuber_plugins_libdir = join_paths(prefix, libdir, gtuber_api_name)

warning_flags = [
  '-Wmissing-declarations',
  '-Wredundant-decls',
  '-Wwrite-strings',
  '-Wformat',
  '-Wformat-security',
  '-Winit-self',
  '-Wmissing-include-dirs',
  '-Waddress',
  '-Wno-multichar',
  '-Wvla',
  '-Wpointer-arith',
  '-Wmissing-prototypes',
  '-Wdeclaration-after-statement',
  '-Wold-style-definition',
]

foreach extra_arg : warning_flags
  if cc.has_argument (extra_arg)
    add_project_arguments([extra_arg], language: 'c')
  endif
endforeach

cdata = configuration_data()

cdata.set('PACKAGE', '"@0@"'.format(meson.project_name()))
cdata.set('VERSION', '"@0@"'.format(meson.project_version()))
cdata.set('PACKAGE_VERSION', '"@0@"'.format(meson.project_version()))

cdata.set('GTUBER_API_NAME', '"@0@"'.format(gtuber_api_name))
cdata.set('GTUBER_PLUGIN_PATH', '"@0@"'.format(gtuber_plugins_libdir))

cdata.set('GST_PACKAGE_NAME', '"gst-gtuber"')
cdata.set('GST_PACKAGE_ORIGIN', '"Unknown package origin"')
cdata.set('GST_LICENSE', '"LGPL"')

configure_file(
  output: 'config.h',
  configuration: cdata,
)

summary({
  'prefix': prefix,
  'bindir': bindir,
  'datadir': datadir,
  'libdir': libdir,
  'includedir': includedir,
}, section: 'Directories')

subdir('gtuber')
summary('introspection', build_gir ? 'Yes' : 'No', section: 'Build')
summary('vapi', build_vapi ? 'Yes' : 'No', section: 'Build')

subdir('doc')
summary('doc', build_doc, section: 'Build')

subdir('utils')
subdir('gst')

subdir('plugins')

subdir('tests')
