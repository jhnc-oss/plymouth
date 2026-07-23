libply_test_sources = {
  'array': 'test-array.c',
  'bitarray': 'test-bitarray.c',
  'buffer': 'test-buffer.c',
  'command-parser': 'test-command-parser.c',
  'hashtable': 'test-hashtable.c',
  'key-file': 'test-key-file.c',
  'list': 'test-list.c',
  'rectangle': 'test-rectangle.c',
  'region': 'test-region.c',
  'trigger': 'test-trigger.c',
  'utils': 'test-utils.c',
}

test_c_args = [
  '-Wall',
  '-Wextra',
  '-Werror',
  '-Wno-unused-parameter',
]

test_environment = environment()
test_environment.set('LC_ALL', 'C')
test_timeout = 30

foreach test_name, test_source : libply_test_sources
  test_executable = executable(
    'test-' + test_name,
    test_source,
    c_args: test_c_args,
    dependencies: libply_dep,
    include_directories: include_directories('.'),
  )

  test(
    'libply-' + test_name,
    test_executable,
    env: test_environment,
    protocol: 'tap',
    suite: ['unit', 'libply'],
    timeout: test_timeout,
  )
endforeach

splash_core_test_sources = {
  'rich-text': 'test-rich-text.c',
  'terminal-emulator': 'test-terminal-emulator.c',
}

foreach test_name, test_source : splash_core_test_sources
  test_executable = executable(
    'test-' + test_name,
    test_source,
    c_args: test_c_args,
    dependencies: libply_splash_core_dep,
    include_directories: include_directories('.'),
  )

  test(
    'splash-core-' + test_name,
    test_executable,
    env: test_environment,
    protocol: 'tap',
    suite: ['unit', 'splash-core'],
    timeout: test_timeout,
  )
endforeach
