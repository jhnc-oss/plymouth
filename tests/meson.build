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

subdir('plugins')

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

secure_boot_test_executable = executable(
  'test-secure-boot',
  'test-secure-boot.c',
  c_args: test_c_args,
  dependencies: [libply_dep, ply_secure_boot_dep],
  include_directories: include_directories('.'),
)

test(
  'libply-secure-boot',
  secure_boot_test_executable,
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
)

test(
  'boot-server-protocol',
  boot_server_test_executable,
  env: test_environment,
  protocol: 'tap',
  suite: ['unit', 'boot-server'],
  timeout: test_timeout,
)

plymouthd_policy_test_executable = executable(
  'test-plymouthd-policy',
  'test-plymouthd-policy.c',
  c_args: test_c_args,
  dependencies: plymouthd_policy_dep,
  include_directories: include_directories('.'),
)

test(
  'plymouthd-policy',
  plymouthd_policy_test_executable,
  env: test_environment,
  protocol: 'tap',
  suite: ['unit', 'plymouthd'],
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

vconsole_test_executable = executable(
  'test-vconsole',
  'test-vconsole.c',
  c_args: test_c_args,
  dependencies: ply_vconsole_dep,
  include_directories: include_directories('.'),
)

test(
  'splash-core-vconsole',
  vconsole_test_executable,
  env: test_environment,
  protocol: 'tap',
  suite: ['unit', 'splash-core'],
  timeout: test_timeout,
)

active_console_test_executable = executable(
  'test-active-console',
  'test-active-console.c',
  c_args: test_c_args,
  dependencies: ply_active_console_dep,
  include_directories: include_directories('.'),
)

test(
  'splash-core-active-console',
  active_console_test_executable,
  env: test_environment,
  protocol: 'tap',
  suite: ['unit', 'splash-core'],
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

bgrt_test_executable = executable(
  'test-bgrt',
  'test-bgrt.c',
  c_args: test_c_args,
  dependencies: ply_bgrt_dep,
  include_directories: include_directories('.'),
)

test(
  'splash-plugin-boot-logo-metadata',
  bgrt_test_executable,
  env: test_environment,
  protocol: 'tap',
  suite: ['unit', 'splash-plugin'],
  timeout: test_timeout,
)

renderer_test_c_args = test_c_args + [
  '-DTEST_RENDERER_PLUGIN_DIR="@0@"'.format(
    meson.project_build_root() / 'tests/plugins'
  ),
  '-DTEST_RENDERER_PLUGIN_PATH="@0@"'.format(
    fake_renderer_plugin.full_path()
  ),
]

renderer_test_executable = executable(
  'test-renderer',
  'test-renderer.c',
  c_args: renderer_test_c_args,
  dependencies: [libply_dep, ply_renderer_dep],
  include_directories: [
    include_directories('.'),
    include_directories('../src/libply-splash-core'),
  ],
)

test(
  'splash-core-renderer-plugin',
  renderer_test_executable,
  depends: fake_renderer_plugin,
  env: test_environment,
  protocol: 'tap',
  suite: ['unit', 'splash-core'],
  timeout: test_timeout,
)

label_test_c_args = test_c_args + [
  '-DTEST_LABEL_PLUGIN_DIR="@0@"'.format(
    meson.project_build_root() / 'tests/plugins'
  ),
  '-DTEST_LABEL_PLUGIN_PATH="@0@"'.format(
    fake_label_plugin.full_path()
  ),
]

label_test_executable = executable(
  'test-label',
  'test-label.c',
  c_args: label_test_c_args,
  dependencies: [libply_dep, libply_splash_core_dep, ply_label_dep],
  include_directories: [
    include_directories('.'),
    include_directories('../src/libply-splash-graphics'),
  ],
)

test(
  'splash-graphics-label-plugin',
  label_test_executable,
  depends: fake_label_plugin,
  env: test_environment,
  protocol: 'tap',
  suite: ['unit', 'splash-graphics'],
  timeout: test_timeout,
)

boot_splash_test_c_args = test_c_args + [
  '-DTEST_SPLASH_PLUGIN_DIR="@0@/"'.format(
    meson.project_build_root() / 'tests/plugins'
  ),
  '-DTEST_SPLASH_PLUGIN_PATH="@0@"'.format(
    fake_splash_plugin.full_path()
  ),
  '-DTEST_SPLASH_THEME_PATH="@0@"'.format(
    meson.current_source_dir() / 'plugins/fake-splash-theme.conf'
  ),
  '-DTEST_RENDERER_PLUGIN_DIR="@0@"'.format(
    meson.project_build_root() / 'tests/plugins'
  ),
]

boot_splash_test_executable = executable(
  'test-boot-splash',
  'test-boot-splash.c',
  c_args: boot_splash_test_c_args,
  dependencies: [libply_dep, libply_splash_core_dep, ply_renderer_dep],
  include_directories: [
    include_directories('.'),
    include_directories('../src/libply-splash-core'),
  ],
)

test(
  'splash-core-boot-splash-plugin',
  boot_splash_test_executable,
  depends: [fake_renderer_plugin, fake_splash_plugin],
  env: test_environment,
  protocol: 'tap',
  suite: ['unit', 'splash-core'],
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
  'executable-plymouthd-help',
  bash,
  args: [
    files('test-plymouthd.sh'),
    plymouthd,
  ],
  depends: plymouthd,
  env: test_environment,
  protocol: 'tap',
  suite: ['functional', 'executables'],
  timeout: test_timeout,
)

test(
  'executable-plymouth-help',
  bash,
  args: [
    files('test-plymouth.sh'),
    plymouth_client,
    plymouth_plugin_path,
  ],
  depends: plymouth_client,
  env: test_environment,
  protocol: 'tap',
  suite: ['functional', 'executables'],
  timeout: test_timeout,
)

test(
  'executable-plymouthd-fd-escrow',
  bash,
  args: [
    files('test-plymouthd-fd-escrow.sh'),
    plymouthd_fd_escrow,
  ],
  depends: plymouthd_fd_escrow,
  env: test_environment,
  protocol: 'tap',
  suite: ['functional', 'executables'],
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
