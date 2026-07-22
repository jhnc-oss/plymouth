ply_bgrt = static_library(
  'ply-bgrt-private',
  'ply-bgrt.c',
  dependencies: libply_splash_core_dep,
  include_directories: config_h_inc,
  pic: true,
)

ply_bgrt_dep = declare_dependency(
  dependencies: libply_splash_core_dep,
  include_directories: include_directories('.'),
  link_with: ply_bgrt,
)

two_step_plugin = shared_module('two-step',
  'plugin.c',
  dependencies: [
    libply_splash_core_dep,
    libply_splash_graphics_dep,
    ply_bgrt_dep,
  ],
  c_args: [
    '-DPLYMOUTH_BACKGROUND_START_COLOR=@0@'.format(get_option('background-start-color-stop')),
    '-DPLYMOUTH_BACKGROUND_END_COLOR=@0@'.format(get_option('background-end-color-stop')),
  ],
  include_directories: config_h_inc,
  name_prefix: '',
  install: true,
  install_dir: plymouth_plugin_path,
)
