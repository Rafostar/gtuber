doc_version_conf = configuration_data()
doc_version_conf.set('GTUBER_VERSION', meson.project_version())
configure_file(
  input: 'version.xml.in',
  output: 'version.xml',
  configuration: doc_version_conf,
)

gtkdocincl = include_directories('.')
ignore_headers = [
  'gtuber.h',
  'gtuber-plugin-devel.h',
  'gtuber-cache-private.h',
  'gtuber-loader-private.h',
  'gtuber-stream-private.h',
  'gtuber-adaptive-stream-private.h',
]

gnome.gtkdoc('gtuber',
  main_xml: 'gtuber-docs.xml',
  namespace: 'gtuber',
  dependencies : [gtuber_dep],
  include_directories : [gtkdocincl],
  src_dir: 'gtuber',
  scan_args: gtkdoc_common_scan_args + [
    '--rebuild-types',
    '--ignore-headers=' + ' '.join(ignore_headers),
  ],
  install: true,
  check: true,
)
