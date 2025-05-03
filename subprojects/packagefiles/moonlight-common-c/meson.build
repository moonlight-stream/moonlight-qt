project(
  'moonlight-common-c',
  'c',
  meson_version: '>= 1.3.0',
  default_options: ['warning_level=0', 'c_std=gnu11'],
)

inc = include_directories('reedsolomon', 'src')

libenet_dep = dependency('libenet', fallback: 'enet')
openssl_dep = dependency('openssl') # TODO does this lib support OpenSSL 3?

lib = library(
  'moonlight-common-c',
  'reedsolomon' / 'rs.c',
  'src' / 'AudioStream.c',
  'src' / 'ByteBuffer.c',
  'src' / 'Connection.c',
  'src' / 'ConnectionTester.c',
  'src' / 'ControlStream.c',
  'src' / 'FakeCallbacks.c',
  'src' / 'InputStream.c',
  'src' / 'LinkedBlockingQueue.c',
  'src' / 'Misc.c',
  'src' / 'Platform.c',
  'src' / 'PlatformCrypto.c',
  'src' / 'PlatformSockets.c',
  'src' / 'RecorderCallbacks.c',
  'src' / 'RtpAudioQueue.c',
  'src' / 'RtpVideoQueue.c',
  'src' / 'RtspConnection.c',
  'src' / 'RtspParser.c',
  'src' / 'SdpGenerator.c',
  'src' / 'SimpleStun.c',
  'src' / 'VideoDepacketizer.c',
  'src' / 'VideoStream.c',
  c_args: [
    '-D',
    'HAS_SOCKLEN_T',
  ],
  include_directories: inc,
  dependencies: [
    libenet_dep,
    openssl_dep,
  ],
  install: true,
)

# Make this library usable as a Meson subproject.
moonlight_common_c_dep = declare_dependency(
  include_directories: inc,
  link_with: lib,
)
meson.override_dependency('moonlight-common-c', moonlight_common_c_dep)
