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
  fallback: ['glib', 'libgmodule_dep'],
)
soup_dep = dependency('libsoup-3.0',
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

gtuber_headers_dir = join_paths(includedir, gtuber_api_name, 'gtuber')

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
  'gtuber-enums.h',
  'gtuber-client.h',
  'gtuber-stream.h',
  'gtuber-adaptive-stream.h',
  'gtuber-media-info.h',
  'gtuber-manifest-generator.h',
  'gtuber-misc-functions.h',
  gtuber_version_header,
]
gtuber_plugin_devel_headers = [
  'gtuber-plugin-devel.h',
  'gtuber-website.h',
  'gtuber-cache.h',
  'gtuber-config.h',
  'gtuber-stream-devel.h',
  'gtuber-adaptive-stream-devel.h',
  'gtuber-media-info-devel.h',
]
gtuber_sources = [
  'gtuber-client.c',
  'gtuber-stream.c',
  'gtuber-adaptive-stream.c',
  'gtuber-media-info.c',
  'gtuber-manifest-generator.c',
  'gtuber-misc-functions.c',
]
gtuber_plugin_devel_sources = [
  'gtuber-website.c',
  'gtuber-cache.c',
  'gtuber-config.c',
]
gtuber_sources_other = [
  'gtuber-loader.c',
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
  gtuber_sources + gtuber_plugin_devel_sources + gtuber_sources_other + gtuber_enums,
  dependencies: gtuber_deps,
  include_directories: conf_inc,
  c_args: gtuber_c_args,
  version: gtuber_version,
  install: true,
)

install_headers(gtuber_headers + gtuber_plugin_devel_headers,
  install_dir: gtuber_headers_dir
)

gir = find_program('g-ir-scanner', required: get_option('introspection'))
build_gir = (gir.found() and not get_option('introspection').disabled())

if build_gir
  # Simplified GIR for interpreted languages
  gtuber_gir = gnome.generate_gir(
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
    export_packages: gtuber_api_name,
    install: true,
    includes: ['GObject-2.0', 'Gio-2.0'],
    header: join_paths(meson.project_name(), 'gtuber.h'),
  )
  # Full GIR for compiled languages
  gtuber_plugin_devel_gir = gnome.generate_gir(
    gtuber_lib,
    sources: gtuber_sources + gtuber_plugin_devel_sources +
      gtuber_headers + gtuber_plugin_devel_headers + gtuber_enums,
    extra_args: [
      '--quiet',
      '--warn-all',
      '-DGTUBER_COMPILATION'
    ],
    nsversion: version_array[0] + '.0',
    namespace: 'GtuberPluginDevel',
    identifier_prefix: 'Gtuber',
    symbol_prefix: 'gtuber',
    export_packages: gtuber_api_name,
    install: false,
    includes: ['GObject-2.0', 'Gio-2.0', 'GModule-2.0', 'Soup-3.0'],
    header: join_paths(meson.project_name(), 'gtuber-plugin-devel.h'),
  )
endif

vapigen = find_program('vapigen', required: get_option('vapi'))
build_vapi = (vapigen.found() and not get_option('vapi').disabled())

if build_vapi
  if not build_gir
    if get_option('vapi').enabled()
      error('cannot build "vapi" without "introspection"')
    endif
  else
    gnome.generate_vapi(gtuber_api_name,
      sources: gtuber_gir[0],
      packages: ['gobject-2.0', 'gio-2.0'],
      install: true,
    )
    gnome.generate_vapi(gtuber_plugin_devel_api_name,
      sources: gtuber_plugin_devel_gir[0],
      packages: ['gobject-2.0', 'gio-2.0', 'gmodule-2.0', 'libsoup-3.0'],
      install: true,
    )
  endif
endif

pkgconfig.generate(
  libraries: gtuber_lib,
  subdirs: [gtuber_api_name],
  filebase: gtuber_api_name,
  name: meson.project_name(),
  version: meson.project_version(),
  description: ' A C library to fetch media info from websites',
  requires: [
    'glib-2.0',
    'gobject-2.0',
    'gio-2.0',
    'gmodule-2.0',
    'libsoup-3.0',
  ],
)

gtuber_dep = declare_dependency(
  link_with: gtuber_lib,
  include_directories: conf_inc,
  dependencies: gtuber_deps,
  sources: gtuber_enums,
)
