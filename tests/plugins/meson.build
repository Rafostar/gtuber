# Tests
all_tests = {
  'bilibili': [1, 2, 3],
  'invidious': [1, 2],
  'lbry': [1, 2],
  'piped': [1, 2],
  'twitch': [1, 2],
  'youtube': [1, 2, 3],
}

foreach name, plugin_tests : all_tests
  if not build_plugins.contains(name)
    continue
  endif
  test_sources = ['../tests.c', '@0@.c'.format(name)]
  exec = executable('@0@'.format(name), test_sources,
    dependencies: gtuber_dep,
  )
  foreach test_num : plugin_tests
    test('@0@ plugin test @1@'.format(name, test_num), exec,
      args: [test_num.to_string()],
      is_parallel: false,
      suite: name,
      timeout: 90,
    )
  endforeach
endforeach
