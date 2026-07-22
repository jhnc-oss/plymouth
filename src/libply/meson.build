libply_sources = files(
  'ply-array.c',
  'ply-bitarray.c',
  'ply-buffer.c',
  'ply-command-parser.c',
  'ply-event-loop.c',
  'ply-hashtable.c',
  'ply-key-file.c',
  'ply-list.c',
  'ply-logger.c',
  'ply-rectangle.c',
  'ply-region.c',
  'ply-terminal-session.c',
  'ply-trigger.c',
  'ply-utils.c',
)

libply_deps = [
  ldl_dep,
  lm_dep,
]

ply_clock = static_library(
  'ply-clock-private',
  'ply-clock.c',
  include_directories: config_h_inc,
  pic: true,
)

ply_clock_dep = declare_dependency(
  include_directories: include_directories('.'),
  link_with: ply_clock,
)

ply_secure_boot = static_library(
  'ply-secure-boot-private',
  'ply-secure-boot.c',
  include_directories: config_h_inc,
  pic: true,
)

ply_secure_boot_dep = declare_dependency(
  include_directories: include_directories('.'),
  link_with: ply_secure_boot,
)

ply_progress = static_library(
  'ply-progress-private',
  'ply-progress.c',
  dependencies: lm_dep,
  include_directories: config_h_inc,
  pic: true,
)

libply = library('ply',
  libply_sources,
  dependencies: libply_deps,
  include_directories: config_h_inc,
  link_whole: [ply_clock, ply_secure_boot, ply_progress],
  version: plymouth_soversion,
  install: true,
)

libply_dep = declare_dependency(
  link_with: libply,
  dependencies: libply_deps,
  include_directories: include_directories('.'),
)

ply_progress_dep = declare_dependency(
  dependencies: [libply_dep, ply_clock_dep],
  include_directories: include_directories('.'),
  link_with: ply_progress,
)

libply_headers = files(
  'ply-array.h',
  'ply-bitarray.h',
  'ply-buffer.h',
  'ply-command-parser.h',
  'ply-event-loop.h',
  'ply-hashtable.h',
  'ply-i18n.h',
  'ply-key-file.h',
  'ply-list.h',
  'ply-logger.h',
  'ply-progress.h',
  'ply-rectangle.h',
  'ply-region.h',
  'ply-terminal-session.h',
  'ply-trigger.h',
  'ply-utils.h',
)

install_headers(libply_headers,
  install_dir: get_option('includedir') / 'plymouth-1' / 'ply',
)
