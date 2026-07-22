libply_test_sources = {
  'array': 'test-array.c',
  'bitarray': 'test-bitarray.c',
  'buffer': 'test-buffer.c',
  'command-parser': 'test-command-parser.c',
  'event-loop': 'test-event-loop.c',
  'hashtable': 'test-hashtable.c',
  'key-file': 'test-key-file.c',
  'list': 'test-list.c',
  'logger': 'test-logger.c',
  'rectangle': 'test-rectangle.c',
  'region': 'test-region.c',
  'terminal-session': 'test-terminal-session.c',
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

progress_test_executable = executable(
  'test-progress',
  'test-progress.c',
  c_args: test_c_args,
  dependencies: ply_progress_dep,
  include_directories: include_directories('.'),
)

test(
  'libply-progress',
  progress_test_executable,
  env: test_environment,
  protocol: 'tap',
  suite: ['unit', 'libply'],
  timeout: test_timeout,
)

boot_client_test_executable = executable(
  'test-boot-client',
  'test-boot-client.c',
  c_args: test_c_args,
  dependencies: [
    libply_dep,
    libply_boot_client_dep,
  ],
  include_directories: include_directories('.'),
)

test(
  'boot-client-protocol',
  boot_client_test_executable,
  env: test_environment,
  protocol: 'tap',
  suite: ['unit', 'boot-client'],
  timeout: test_timeout,
)

boot_server_test_c_args = test_c_args + [
  '-Wno-cast-function-type',
]

boot_server_test_executable = executable(
  'test-boot-server',
  'test-boot-server.c',
  c_args: boot_server_test_c_args,
  dependencies: ply_boot_server_dep,
  include_directories: [
    include_directories('.'),
    include_directories('../src'),
  ],
  link_args: '-Wl,--wrap=ply_get_credentials_from_fd',
)

test(
  'boot-server-protocol',
  boot_server_test_executable,
  env: test_environment,
  protocol: 'tap',
  suite: ['unit', 'boot-server'],
  timeout: test_timeout,
)

script_source_dir = meson.project_source_root() / 'src/plugins/splash/script'

script_scan_test_executable = executable(
  'test-script-scan',
  'test-script-scan.c',
  c_args: test_c_args,
  dependencies: script_engine_dep,
  include_directories: [
    include_directories('.'),
    include_directories('../src/plugins/splash/script'),
  ],
)

test(
  'script-scan',
  script_scan_test_executable,
  env: test_environment,
  protocol: 'tap',
  suite: ['unit', 'script'],
  timeout: test_timeout,
)

script_execute_test_c_args = test_c_args + [
  '-DPLYMOUTH_SOURCE_ROOT="@0@"'.format(meson.project_source_root()),
]

script_execute_test_executable = executable(
  'test-script-execute',
  'test-script-execute.c',
  c_args: script_execute_test_c_args,
  dependencies: script_engine_dep,
  include_directories: [
    include_directories('.'),
    include_directories('../src/plugins/splash/script'),
  ],
)

test(
  'script-execute',
  script_execute_test_executable,
  env: test_environment,
  protocol: 'tap',
  suite: ['unit', 'script'],
  timeout: test_timeout,
)

splash_core_test_sources = {
  'pixel-buffer': 'test-pixel-buffer.c',
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

image_test_executable = executable(
  'test-image',
  'test-image.c',
  c_args: test_c_args,
  dependencies: [
    libply_splash_core_dep,
    libply_splash_graphics_dep,
  ],
  include_directories: include_directories('.'),
)

test(
  'splash-graphics-image',
  image_test_executable,
  env: test_environment,
  protocol: 'tap',
  suite: ['unit', 'splash-graphics'],
  timeout: test_timeout,
)

animation_time_test_executable = executable(
  'test-animation-time',
  'test-animation-time.c',
  c_args: test_c_args,
  dependencies: [libply_dep, ply_animation_time_dep],
  include_directories: include_directories('.'),
)

test(
  'splash-graphics-animation-time',
  animation_time_test_executable,
  env: test_environment,
  protocol: 'tap',
  suite: ['unit', 'splash-graphics'],
  timeout: test_timeout,
)

bash = find_program('bash')

test(
  'script-generate-version',
  bash,
  args: [
    files('test-generate-version.sh'),
    meson.project_source_root() / 'scripts/generate-version.sh',
  ],
  env: test_environment,
  protocol: 'tap',
  suite: ['functional', 'scripts'],
  timeout: test_timeout,
)

test(
  'script-set-default-theme',
  bash,
  args: [
    files('test-set-default-theme.sh'),
    meson.project_build_root() / 'scripts/plymouth-set-default-theme',
  ],
  env: test_environment,
  protocol: 'tap',
  suite: ['functional', 'scripts'],
  timeout: test_timeout,
)

test(
  'script-populate-initrd',
  bash,
  args: [
    files('test-populate-initrd.sh'),
    meson.project_build_root() / 'scripts/plymouth-populate-initrd',
  ],
  env: test_environment,
  protocol: 'tap',
  suite: ['functional', 'scripts'],
  timeout: test_timeout,
)

test(
  'script-generate-initrd',
  bash,
  args: [
    files('test-generate-initrd.sh'),
    meson.project_build_root() / 'scripts/plymouth-generate-initrd',
  ],
  env: test_environment,
  protocol: 'tap',
  suite: ['functional', 'scripts'],
  timeout: test_timeout,
)
