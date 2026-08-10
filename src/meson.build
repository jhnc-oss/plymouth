# The library subdirectories first
subdir('libply')
subdir('libply-splash-core')
subdir('libply-splash-graphics')

# plymouthd
plymouthd_run_dir = plymouth_runtime_dir
plymouthd_spool_dir = '/var/spool/plymouth'
plymouthd_time_dir = plymouth_time_dir

plymouthd_cflags = [
  '-DPLYMOUTH_LOCALE_DIRECTORY="@0@"'.format(get_option('prefix') / get_option('localedir')),
  '-DPLYMOUTH_DRM_ESCROW_DIRECTORY="@0@"'.format(get_option('prefix') / get_option('libexecdir') / 'plymouth'),
  '-DPLYMOUTH_LOG_DIRECTORY="@0@"'.format('/var/log'),
  '-DPLYMOUTH_SPOOL_DIRECTORY="@0@"'.format(plymouthd_spool_dir),
]

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

plymouthd_private_inc = include_directories('daemon')

plymouthd_private_sources = files(
  'daemon/plymouthd.c',
  'daemon/plymouthd-commands.c',
  'daemon/plymouthd-diagnostics.c',
  'daemon/plymouthd-display.c',
  'daemon/plymouthd-interaction.c',
  'daemon/plymouthd-logging.c',
  'daemon/plymouthd-messages.c',
  'daemon/plymouthd-options.c',
  'daemon/plymouthd-policy.c',
  'daemon/plymouthd-process.c',
  'daemon/plymouthd-progress.c',
  'daemon/plymouthd-session.c',
  'plymouthd-settings.c',
  'plymouthd-splash.c',
  'plymouthd-theme-resolver.c',
  'plymouthd-transition.c',
)

plymouthd_private = static_library(
  'plymouthd-private',
  plymouthd_private_sources,
  c_args: plymouthd_cflags,
  dependencies: [libply_dep, libply_splash_core_dep, ply_boot_server_dep],
  include_directories: [config_h_inc, plymouthd_private_inc],
)

plymouthd_private_dep = declare_dependency(
  dependencies: [libply_dep, libply_splash_core_dep, ply_boot_server_dep],
  include_directories: [include_directories('.'), plymouthd_private_inc],
  link_with: plymouthd_private,
)

# Compatibility aliases for focused internal tests.
plymouthd_policy_dep = plymouthd_private_dep
plymouthd_settings_dep = plymouthd_private_dep
plymouthd_splash_dep = plymouthd_private_dep
plymouthd_interaction_dep = plymouthd_private_dep
plymouthd_messages_dep = plymouthd_private_dep
plymouthd_logging_dep = plymouthd_private_dep
plymouthd_session_dep = plymouthd_private_dep
plymouthd_progress_dep = plymouthd_private_dep
plymouthd_diagnostics_dep = plymouthd_private_dep
plymouthd_transition_dep = plymouthd_private_dep
plymouthd_options_dep = plymouthd_private_dep
plymouthd_commands_dep = plymouthd_private_dep
plymouthd_display_dep = plymouthd_private_dep

plymouthd_sources = files(
  'main.c',
  'plugins/splash/details/plugin.c',
  'ply-boot-protocol.h',
  'ply-boot-server.h',
)

plymouthd_deps = [
  plymouthd_private_dep,
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
