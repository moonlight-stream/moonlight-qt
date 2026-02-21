project(
  'moonlight-qt',
  'c',
  'cpp',
  meson_version: '>= 1.7.0',
  version: files('app' / 'version.txt'),
  default_options: [
    'warning_level=3',
    'c_std=gnu11',
    'cpp_std=c++17',
    'b_ndebug=if-release',
  ],
)

version_str = meson.project_version()
add_project_arguments('-D', 'VERSION_STR="@0@"'.format(version_str), language: ['c', 'cpp'])

add_project_arguments('-D', 'typeof=__typeof__', language: ['c', 'cpp'])

moonlight_common_c_dep = dependency(
  'moonlight-common-c',
  fallback: 'moonlight-common-c',
)
qmdnsengine_dep = dependency(
  'qmdnsengine',
  fallback: 'qmdnsengine',
)
soundio_dep = dependency(
  'soundio',
  static: true,
  fallback: 'libsoundio',
)
h264bitstream_dep = dependency('h264bitstream', fallback: 'h264bitstream')
ffnvcodec_dep = dependency('ffnvcodec', fallback: 'ffnvcodec')

subdir('app')
