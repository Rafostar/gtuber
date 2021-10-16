generator = find_program('rust/gir/generator.py', required: get_option('rs'))
cargo = find_program('cargo', required: get_option('rs'))
build_rust = (cargo.found() and generator.found() and not get_option('rs').disabled())
if build_rust
	if not build_gir
		if get_option('rs').enabled()
			error('cannot build "rust" without "introspection"')
		endif
	else
		subdir('rust')
	endif
endif
summary('Rust', build_rust ? 'Yes' : 'No', section: 'Bindings')
