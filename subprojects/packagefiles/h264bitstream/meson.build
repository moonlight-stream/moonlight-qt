project(
  'h264bitstream',
  'c',
  meson_version: '>= 1.3.0',
  default_options: ['warning_level=0', 'c_std=c99'],
)

lib = library(
  'h264bitstream',
  'h264_stream.c',
  'h264_sei.c',
  'h264_nal.c',
  install: true,
)

# Make this library usable as a Meson subproject.
h264bitstream_dep = declare_dependency(
  include_directories: include_directories('.'),
  link_with: lib,
)
meson.override_dependency('h264bitstream', h264bitstream_dep)

# Make this library usable from the system's
# package manager.
install_headers(
  'h264_stream.h',
  'h264_sei.h',
  'h264_avcc.h',
  'bs.h',
  subdir: 'h264bitstream',
)

pkg_mod = import('pkgconfig')
pkg_mod.generate(
  lib,
  description: 'H264 bit stream',
  subdirs: 'h264bitstream',
)
