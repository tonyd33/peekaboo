project(
  'peekaboo',
  'c',
  version: '0.1.0',
  license: 'MIT',
  meson_version: '>=0.61.0',
  default_options: [
    'c_std=c2x',
    'optimization=3',
    'buildtype=debugoptimized',
    'warning_level=3',
    'b_lto=true',
    'b_lto_threads=-1',
    'b_pie=true',
    'prefix=/usr'
  ],
)

debug = get_option('buildtype').startswith('debug')
if debug
  add_project_arguments(
    '-DDEBUG',
    # this is pretty performance-heavy so turn it on only when trying to
    # actively catch bugs
    # '-fsanitize=address',
    # these are so noisy
    # '-DDEBUG_RENDERS',
    language : 'c'
  )
endif

config_location = join_paths(
  get_option('sysconfdir'),
  'xdg',
  'peekaboo'
)

add_project_arguments(
  [
    '-pedantic',
    # '-Werror',
    '-Wall',
    '-Wshadow',
    '-Wno-unused-parameter',
    '-D_GNU_SOURCE',
    '-D_FORTIFY_SOURCE=2',
  ],
  language: 'c'
)

sources = files(
  'src/log.c',
  'src/shm.c',
  'src/surface.c',
  'src/preview.c',
  'src/wm_client/wm_client.c',
  'src/wm_client/hyprland.c',
  'src/layout.c',
  'src/vec.c',
  'src/util.c',
  'src/config.c',
)

cc = meson.get_compiler('c')

math = cc.find_library('m')
cairo = dependency('cairo')
pangocairo = dependency('pangocairo')
xkbcommon = dependency('xkbcommon')
# TODO: Add these as subprojects rather than system dependencies.
# I don't know how to do that
cjson = dependency('libcjson')
libcyaml = dependency('libcyaml')
wayland_client = dependency('wayland-client')

# Wayland protocols {{{
wayland_protocols = dependency('wayland-protocols', native: true)
wayland_scanner_dep = dependency('wayland-scanner', native: true)

# Generate the necessary Wayland headers / sources with wayland-scanner
wayland_scanner = find_program(
  wayland_scanner_dep.get_variable(pkgconfig: 'wayland_scanner'),
  native: true
)

wayland_protocols_dir = wayland_protocols.get_variable(pkgconfig: 'pkgdatadir')

wl_proto_headers = []
wl_proto_src = []
wl_proto_xml = [
  wayland_protocols_dir + '/stable/xdg-shell/xdg-shell.xml',
  wayland_protocols_dir + '/stable/viewporter/viewporter.xml',
  'protocols/wlr-layer-shell-unstable-v1.xml',
  'protocols/fractional-scale-v1.xml',
  'protocols/xdg-output-unstable-v1.xml',
  'protocols/hyprland-toplevel-export-v1.xml',
  'protocols/wlr-foreign-toplevel-management-unstable-v1.xml',
]

foreach proto : wl_proto_xml
  wl_proto_headers += custom_target(
    proto.underscorify() + '_client_header',
    output: '@BASENAME@.h',
    input: proto,
    command: [wayland_scanner, 'client-header', '@INPUT@', '@OUTPUT@'])

  wl_proto_src += custom_target(
    proto.underscorify() + '_private_code',
    output: '@BASENAME@.c',
    input: proto,
    command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'])
endforeach
# }}}

executable(
  'peekaboo',
  [
    files('src/main.c'),
    sources,
    wl_proto_src,
    wl_proto_headers,
  ],
  dependencies: [
    math,
    cairo,
    pangocairo,
    wayland_client,
    xkbcommon,
    cjson,
    libcyaml,
  ],
  install: true
)
# vim:foldmethod=marker
