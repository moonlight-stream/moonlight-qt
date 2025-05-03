project(
  'libsoundio',
  'c',
  meson_version: '>= 1.3.0',
  default_options: ['warning_level=0', 'c_std=gnu11', 'default_library=static'],
)

inc = include_directories('.', 'src')

dep = [
  dependency('threads'),
  dependency('jack'),
  dependency('libpulse'),
  dependency('alsa')
]

src = files(
  'src' / 'soundio.c',
  'src' / 'util.c',
  'src' / 'os.c',
  'src' / 'dummy.c',
  'src' / 'channel_layout.c',
  'src' / 'ring_buffer.c',
  'src' / 'soundio.c',
  'src' / 'soundio.c',
  'src' / 'soundio.c',
  'src' / 'soundio.c',
  'src' / 'soundio.c',
  'src' / 'soundio.c',
)

# JACK
src += files(
  'src' / 'jack.c',
)

# PulseAudio
src += files(
  'src' / 'pulseaudio.c',
)

# ALSA
src += files(
  'src' / 'alsa.c',
)

lib = library(
  'soundio',
  src,
  include_directories: inc,
  dependencies: dep,
  gnu_symbol_visibility: 'hidden',
  install: true,
)

# Make this library usable as a Meson subproject.
libsoundio_dep = declare_dependency(
  include_directories: inc,
  link_with: lib,
)
meson.override_dependency('soundio', libsoundio_dep)

# Make this library usable from the system's
# package manager.
install_headers(
  'soundio' / 'soundio.h',
  'soundio' / 'endian.h',
  subdir: 'soundio',
)

pkg_mod = import('pkgconfig')
pkg_mod.generate(
  lib,
  description: 'cross-platform audio input and output',
  subdirs: 'soundio',
)
