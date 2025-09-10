wl_mod = import('unstable-wayland')

xdg_shell_xml = wl_mod.find_protocol('xdg-shell')

xdg_shell = wl_mod.scan_xml(xdg_shell_xml)

wayland_plugin = shared_module('wayland',
  'plugin.c',
  xdg_shell,
  dependencies: [
    libply_dep,
    libply_splash_core_dep,
    wayland_dep,
  ],
  include_directories: config_h_inc,
  name_prefix: '',
  install: true,
  install_dir: plymouth_plugin_path / 'renderers',
)
