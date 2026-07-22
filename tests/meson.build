libply_test_sources = {
  'array': 'test-array.c',
  'bitarray': 'test-bitarray.c',
  'buffer': 'test-buffer.c',
  'hashtable': 'test-hashtable.c',
  'list': 'test-list.c',
  'rectangle': 'test-rectangle.c',
  'region': 'test-region.c',
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
