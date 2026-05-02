test_inc = [
  config_h_inc,
  include_directories('../src'),
  include_directories('../src/libply'),
]

# Compile ply-boot-server.c directly into the test so we can verify the
# connection_hangup_handler callback mechanism without needing a full plymouthd.
test_server_connection_hangup = executable(
  'test-server-connection-hangup',
  files(
    'test-server-connection-hangup.c',
    '../src/ply-boot-server.c',
  ),
  include_directories: test_inc,
  c_args: plymouthd_cflags,
  dependencies: [libply_dep],
)

# Exit code 77 means SKIP (test requires root for SO_PEERCRED uid check).
test('server-connection-hangup',
  test_server_connection_hangup,
  timeout: 15,
)
