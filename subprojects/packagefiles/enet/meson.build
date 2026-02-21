project(
  'enet',
  'c',
  meson_version: '>= 1.3.0',
  default_options: ['warning_level=0', 'c_std=c99'],
)


# TODO: check portably using compiler
# cc = meson.get_compiler('c')
add_project_arguments('-D', 'HAS_FCNTL=1', language: ['c'])
add_project_arguments('-D', 'HAS_IOCTL=1', language: ['c'])
add_project_arguments('-D', 'HAS_POLL=1', language: ['c'])
add_project_arguments('-D', 'HAS_GETNAMEINFO=1', language: ['c'])
add_project_arguments('-D', 'HAS_GETADDRINFO=1', language: ['c'])
add_project_arguments('-D', 'HAS_GETHOSTBYNAME_R=1', language: ['c'])
add_project_arguments('-D', 'HAS_GETHOSTBYADDR_R=1', language: ['c'])
add_project_arguments('-D', 'HAS_INET_NTOP=1', language: ['c'])
add_project_arguments('-D', 'HAS_MSGHDR_FLAGS=1', language: ['c'])
add_project_arguments('-D', 'HAS_SOCKLEN_T=1', language: ['c'])
add_project_arguments('-D', 'HAS_QOS_FLOWID=1', language: ['c'])
add_project_arguments('-D', 'HAS_PQOS_FLOWID=1', language: ['c'])

inc = include_directories('include')

lib = library(
  'enet',
  'callbacks.c',
  'compress.c',
  'host.c',
  'list.c',
  'packet.c',
  'peer.c',
  'protocol.c',
  'unix.c',
  'win32.c',
  include_directories: inc,
  install: true,
)

# Make this library usable as a Meson subproject.
libenet_dep = declare_dependency(
  include_directories: inc,
  link_with: lib,
)
meson.override_dependency('libenet', libenet_dep)

# Make this library usable from the system's
# package manager.
install_headers(
  'include' / 'enet' / 'callbacks.h',
  'include' / 'enet' / 'enet.h',
  'include' / 'enet' / 'list.h',
  'include' / 'enet' / 'protocol.h',
  'include' / 'enet' / 'time.h',
  'include' / 'enet' / 'types.h',
  'include' / 'enet' / 'unix.h',
  'include' / 'enet' / 'utility.h',
  'include' / 'enet' / 'win32.h',
  subdir: 'enet',
)

pkg_mod = import('pkgconfig')
pkg_mod.generate(
  lib,
  description: 'A relatively thin, simple and robust network communication layer on top of UDP',
  subdirs: 'enet',
)
