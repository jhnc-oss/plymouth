subdir('renderers')

fake_label_plugin = shared_module(
  'label-pango',
  'fake-label.c',
  c_args: test_c_args,
  dependencies: [
    libply_dep,
    libply_splash_core_dep,
    libply_splash_graphics_dep,
  ],
  include_directories: config_h_inc,
  name_prefix: '',
  install: false,
)

fake_splash_plugin = shared_module(
  'test-splash',
  'fake-splash.c',
  c_args: test_c_args,
  dependencies: [
    libply_dep,
    libply_splash_core_dep,
  ],
  include_directories: config_h_inc,
  name_prefix: '',
  install: false,
)
