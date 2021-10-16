build_rust = not get_option('rs').disabled()
if build_rust
	if not build_gir
		error('cannot build "rust" without "introspection"')
	else
		subdir('rust')
	endif
endif
summary('Rust', build_rust ? 'Yes' : 'No', section: 'Bindings')
