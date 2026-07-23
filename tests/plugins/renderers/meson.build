fake_renderer_plugin = shared_module(
  'frame-buffer',
  'fake-renderer.c',
  c_args: test_c_args,
  dependencies: [
    libply_dep,
    libply_splash_core_dep,
  ],
  include_directories: config_h_inc,
  name_prefix: '',
  install: false,
)
