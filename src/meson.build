# The library subdirectories first
subdir('libply')
subdir('libply-splash-core')
subdir('libply-splash-graphics')

# plymouthd
plymouthd_run_dir = plymouth_runtime_dir
plymouthd_spool_dir = '/var/spool/plymouth'
plymouthd_time_dir = plymouth_time_dir

plymouthd_policy = static_library(
  'plymouthd-policy-private',
  'plymouthd-policy.c',
  dependencies: libply_splash_core_dep,
  include_directories: config_h_inc,
)

plymouthd_policy_dep = declare_dependency(
  dependencies: libply_splash_core_dep,
  include_directories: include_directories('.'),
  link_with: plymouthd_policy,
)

plymouthd_settings = static_library(
  'plymouthd-settings-private',
  [
    'plymouthd-settings.c',
    'plymouthd-theme-resolver.c',
  ],
  dependencies: [libply_dep, plymouthd_policy_dep],
  include_directories: config_h_inc,
)

plymouthd_settings_dep = declare_dependency(
  dependencies: [libply_dep, plymouthd_policy_dep],
  include_directories: include_directories('.'),
  link_with: plymouthd_settings,
)

plymouthd_splash = static_library(
  'plymouthd-splash-private',
  'plymouthd-splash.c',
  dependencies: libply_splash_core_dep,
  include_directories: config_h_inc,
)

plymouthd_splash_dep = declare_dependency(
  dependencies: libply_splash_core_dep,
  include_directories: include_directories('.'),
  link_with: plymouthd_splash,
)

plymouthd_interaction = static_library(
  'plymouthd-interaction-private',
  'plymouthd-interaction.c',
  dependencies: [libply_dep, libply_splash_core_dep],
  include_directories: config_h_inc,
)

plymouthd_interaction_dep = declare_dependency(
  dependencies: [libply_dep, libply_splash_core_dep],
  include_directories: include_directories('.'),
  link_with: plymouthd_interaction,
)

plymouthd_messages = static_library(
  'plymouthd-messages-private',
  'plymouthd-messages.c',
  dependencies: [libply_dep, libply_splash_core_dep],
  include_directories: config_h_inc,
)

plymouthd_messages_dep = declare_dependency(
  dependencies: [libply_dep, libply_splash_core_dep],
  include_directories: include_directories('.'),
  link_with: plymouthd_messages,
)

plymouthd_cflags = [
  '-DPLYMOUTH_LOCALE_DIRECTORY="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DPLYMOUTH_DRM_ESCROW_DIRECTORY="@0@"'.format(get_option('prefix') / get_option('libexecdir') / 'plymouth'),
  '-DPLYMOUTH_LOG_DIRECTORY="@0@"'.format('/var/log'),
  '-DPLYMOUTH_SPOOL_DIRECTORY="@0@"'.format(plymouthd_spool_dir),
]

plymouthd_logging = static_library(
  'plymouthd-logging-private',
  'plymouthd-logging.c',
  c_args: plymouthd_cflags,
  dependencies: [libply_dep, libply_splash_core_dep],
  include_directories: config_h_inc,
)

plymouthd_logging_dep = declare_dependency(
  dependencies: [libply_dep, libply_splash_core_dep],
  include_directories: include_directories('.'),
  link_with: plymouthd_logging,
)

plymouthd_session = static_library(
  'plymouthd-session-private',
  'plymouthd-session.c',
  dependencies: [libply_dep, libply_splash_core_dep],
  include_directories: config_h_inc,
)

plymouthd_session_dep = declare_dependency(
  dependencies: [libply_dep, libply_splash_core_dep],
  include_directories: include_directories('.'),
  link_with: plymouthd_session,
)

plymouthd_progress = static_library(
  'plymouthd-progress-private',
  'plymouthd-progress.c',
  dependencies: [libply_dep, libply_splash_core_dep],
  include_directories: config_h_inc,
)

plymouthd_progress_dep = declare_dependency(
  dependencies: [libply_dep, libply_splash_core_dep],
  include_directories: include_directories('.'),
  link_with: plymouthd_progress,
)

ply_peer_credentials = static_library(
  'ply-peer-credentials-private',
  'ply-peer-credentials.c',
  dependencies: libply_dep,
  include_directories: config_h_inc,
)

ply_peer_credentials_dep = declare_dependency(
  dependencies: libply_dep,
  include_directories: include_directories('.'),
  link_with: ply_peer_credentials,
)

ply_boot_server_sources = files('ply-boot-server.c')

ply_boot_server = static_library(
  'ply-boot-server-private',
  ply_boot_server_sources,
  dependencies: libply_dep,
  include_directories: config_h_inc,
)

ply_boot_server_dep = declare_dependency(
  dependencies: [libply_dep, ply_peer_credentials_dep],
  include_directories: include_directories('.'),
  link_with: ply_boot_server,
)

plymouthd_sources = files(
  'main.c',
  'plugins/splash/details/plugin.c',
  'ply-boot-protocol.h',
  'ply-boot-server.h',
)

plymouthd_deps = [
  libply_dep,
  libply_splash_core_dep,
  ply_boot_server_dep,
  plymouthd_policy_dep,
  plymouthd_settings_dep,
  plymouthd_splash_dep,
  plymouthd_interaction_dep,
  plymouthd_messages_dep,
  plymouthd_logging_dep,
  plymouthd_session_dep,
  plymouthd_progress_dep,
]

plymouthd = executable('plymouthd',
  plymouthd_sources,
  dependencies: plymouthd_deps,
  c_args: plymouthd_cflags,
  export_dynamic: true,
  include_directories: config_h_inc,
  install: true,
  install_dir: get_option('sbindir'),
)

plymouthd_fd_escrow = executable('plymouthd-fd-escrow',
  'plymouthd-fd-escrow.c',
  install: true,
  install_dir: get_option('libexecdir') / 'plymouth',
)

install_data('plymouthd.defaults',
  install_dir: plymouth_policy_dir,
)

install_data('plymouthd.conf',
  install_dir: plymouth_conf_dir,
)

install_emptydir(plymouthd_run_dir)
install_emptydir(plymouthd_spool_dir)
install_emptydir(plymouthd_time_dir)


# These subdirectories last
subdir('plugins')
subdir('client')
if get_option('upstart-monitoring')
  subdir('upstart-bridge')
endif
