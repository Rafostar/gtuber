glib_dep = dependency('glib-2.0',
  version: glib_req,
  required: true,
  fallback: ['glib', 'libglib_dep'],
)
gobject_dep = dependency('gobject-2.0',
  version: glib_req,
  required: true,
  fallback: ['glib', 'libgobject_dep'],
)
gio_dep = dependency('gio-2.0',
  version: glib_req,
  required: true,
  fallback: ['glib', 'libgio_dep'],
)
gmodule_dep = dependency('gmodule-2.0',
  version: glib_req,
  required: true,
  fallback: ['glib', 'libgmodule_dep']
)
soup_dep = dependency('libsoup-2.4',
  version: soup_req,
  required: true,
)

version_conf = configuration_data()

version_conf.set(
  'GTUBER_VERSION',
  meson.project_version(),
)
version_conf.set(
  'GTUBER_MAJOR_VERSION',
  version_array[0].to_int(),
)
version_conf.set(
  'GTUBER_MINOR_VERSION',
  version_array[1].to_int(),
)
version_conf.set(
  'GTUBER_MICRO_VERSION',
  version_array[2].to_int(),
)

gtuber_headers_dir = join_paths(includedir, gtuber_api_name)

gtuber_version_header = configure_file(
  input: 'gtuber-version.h.in',
  output: 'gtuber-version.h',
  configuration: version_conf,
)

# Include the generated gtuber_version_header
conf_inc = [
  include_directories('.'),
  include_directories('..'),
]

gtuber_headers = [
  'gtuber.h',
  'gtuber-types.h',
  'gtuber-client.h',
  'gtuber-stream.h',
  'gtuber-adaptive-stream.h',
  'gtuber-media-info.h',
  gtuber_version_header,
]
gtuber_sources = [
  'gtuber-client.c',
  'gtuber-stream.c',
  'gtuber-adaptive-stream.c',
  'gtuber-media-info.c',
  'gtuber-loader.c',
  'gtuber-website.c',
]
gtuber_c_args = [
  '-DG_LOG_DOMAIN="Gtuber"',
  '-DGTUBER_COMPILATION',
]

gtuber_enums = gnome.mkenums_simple(
  'gtuber-enum-types',
  sources: gtuber_headers,
  install_header: true,
  install_dir: gtuber_headers_dir,
)

gtuber_deps = [
  glib_dep,
  gobject_dep,
  gio_dep,
  gmodule_dep,
  soup_dep,
]

gtuber_lib = library(
  gtuber_api_name,
  gtuber_sources + gtuber_enums,
  dependencies: gtuber_deps,
  include_directories: conf_inc,
  c_args: gtuber_c_args,
  version: gtuber_version,
  install: true,
)

install_headers(gtuber_headers, install_dir: gtuber_headers_dir)

gir = find_program('g-ir-scanner', required: get_option('introspection'))
build_gir = (gir.found() and get_option('introspection'))

if build_gir
  gnome.generate_gir(
    gtuber_lib,
    sources: gtuber_sources + gtuber_headers + gtuber_enums,
    extra_args: [
      '--quiet',
      '--warn-all',
      '-DGTUBER_COMPILATION'
    ],
    nsversion: version_array[0] + '.0',
    namespace: 'Gtuber',
    identifier_prefix: 'Gtuber',
    symbol_prefix: 'gtuber',
    install: true,
    includes: ['GObject-2.0', 'Gio-2.0'],
  )
endif

pkgconfig.generate(
  libraries: gtuber_lib,
  subdirs: 'gtuber',
  version: meson.project_version(),
  name: gtuber_api_name,
  filebase: gtuber_api_name,
  description: 'A C library to get media info from websites',
  requires: [
    'glib-2.0',
    'gobject-2.0',
    'gio-2.0',
    'gmodule-2.0',
    'soup-2.4',
  ],
)

gtuber_dep = declare_dependency(
  link_with: gtuber_lib,
  include_directories: conf_inc,
  dependencies: gtuber_deps,
  sources: gtuber_enums,
)
