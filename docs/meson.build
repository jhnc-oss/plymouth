man_conf = configuration_data()
man_conf.set('version', meson.project_version())

man_pages = {
  'plymouth.1.in': 'plymouth.1',
  'plymouth.8.in': 'plymouth.8',
  'plymouthd.8.in': 'plymouthd.8',
  'plymouth-set-default-theme.1.in': 'plymouth-set-default-theme.1',
}

foreach man_input, man_output : man_pages
  configure_file(
    configuration: man_conf,
    input: man_input,
    install: true,
    install_dir: get_option('mandir') / 'man' + man_output.substring(-1),
    output: man_output,
  )
endforeach
