cargo_script = find_program('build-aux/cargo.sh')

global_srcrt = meson.global_source_root()
current_srcdir = meson.current_source_dir()
path_from_rt = current_srcdir.replace(global_srcrt, '')

gir_rs = custom_target(
	'cargo-build',
	build_by_default: true,
	# wierd hack: if global_source_root/.git/modules/ path to this dir /gir/refs/heads/master has
	# changed (i.e. got updated), then this custom_target will be rebuilt
	input: join_paths(global_srcrt, '.git/modules/' + path_from_rt + '/gir/refs/heads/master'),
	output: 'gir',
	console: true,
	install: false,
	command: [
		cargo_script,
		current_srcdir,
		join_paths(meson.current_source_dir(), 'gir'),
		'@OUTPUT@',
		'release',
		'gir'
	]
)

custom_target('rust_bindings',
	build_by_default: true,
	depends: [
		gir_rs,
		gtuber_gir
	],
	input: files(),
	output: 'nothing',
	console: true,
	install: false,
	command: [
		generator,
		'--gir-files-directories', '/usr/share/gir-1.0/', '/usr/local/share/gir-1.0/', gtuber_build_dir,
		'--gir-path', gir_rs.full_path(),
		'--no-fmt', current_srcdir
	]
)
run_command(cargo, 'fmt')

crate_dir = join_paths(datadir, 'cargo/registry/gtuber-0.0.1/')
install_data('Cargo.toml', install_dir: crate_dir)
install_subdir('src/', install_dir: crate_dir)
install_subdir('sys/', install_dir: crate_dir)
