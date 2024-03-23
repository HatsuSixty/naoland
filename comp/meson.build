naoland_comp_sources = [
  'main.cpp',
  'foreign_toplevel.cpp',
  'output.cpp',
  'server.cpp',
  'xwayland.cpp',
  'config.cpp',
  'input/constraint.cpp',
  'input/cursor.cpp',
  'input/keyboard.cpp',
  'input/seat.cpp',
  'surface/layer.cpp',
  'surface/popup.cpp',
  'surface/view.cpp',
  'surface/xdg_view.cpp',
  'surface/xwayland_view.cpp',
  protocols_server_header['content-type-v1'],
  protocols_server_header['cursor-shape-v1'],
  protocols_server_header['xdg-shell'],
  protocols_server_header['wlr-layer-shell-unstable-v1'],
  protocols_server_header['wlr-output-power-management-unstable-v1'],
  protocols_server_header['pointer-constraints-unstable-v1'],
  protocols_server_header['xdg-decoration-unstable-v1'],
]

exe = executable(
  'naoland-comp',
  sources: naoland_comp_sources,
  dependencies: [
    dependency('argparse', version: '>= 3.0', fallback: ['argparse']),
    meson.get_compiler('cpp').find_library('m', required: false),
    dependency('wayland-server'),
    wlroots_dep,
    dependency('xcb'),
    dependency('xkbcommon')
  ],
  cpp_args: '-DPROJECT_VERSION="' + meson.project_version() + '"',
  install: true
)
