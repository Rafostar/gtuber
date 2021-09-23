gtk_doc_dep = dependency('gtk-doc', version: '>=1.32.1', required: get_option('doc'))
build_doc = (gtk_doc_dep.found() and get_option('doc'))
if build_doc
  subdir('reference')
endif
