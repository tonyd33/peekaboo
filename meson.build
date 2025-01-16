project(
  'peekaboo',
  'c',
  version: '0.9.1',
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
  add_project_arguments('-DDEBUG', language : 'c')
endif

config_location = join_paths(
  get_option('sysconfdir'),
  'xdg',
  'peekaboo'
)

add_project_arguments(
  [
    '-pedantic',
    '-Wshadow',
    '-Wno-unused-parameter',
    '-D_GNU_SOURCE',
    '-D_FORTIFY_SOURCE=2',
  ],
  language: 'c'
)

common_sources = files(
  'src/log.c',
  'src/shm.c',
  'src/surface.c',
  'src/preview.c',
  'src/wm_client/wm_client.c',
  'src/wm_client/hyprland.c',
)

cc = meson.get_compiler('c')
libm = cc.find_library('m', required: false)
cairo = dependency('cairo')
pangocairo = dependency('pangocairo')
wayland_client = dependency('wayland-client')
wayland_protocols = dependency('wayland-protocols', native: true)
wayland_scanner_dep = dependency('wayland-scanner', native: true)
xkbcommon = dependency('xkbcommon')
cjson = dependency('libcjson')
glib = dependency('glib-2.0')

if wayland_client.version().version_compare('<1.20.0')
  add_project_arguments(
    ['-DNO_WL_OUTPUT_NAME=1'],
    language: 'c'
    )
endif


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
  'protocols/wlr-screencopy-unstable-v1.xml',
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

executable(
  'peekaboo',
  files('src/main.c'), common_sources, wl_proto_src, wl_proto_headers,
  dependencies: [
    libm,
    cairo,
    pangocairo,
    wayland_client,
    xkbcommon,
    cjson,
  ],
  install: true
)

