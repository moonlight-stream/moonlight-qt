project(
  'ffnvcodec',
  meson_version: '>= 1.3.0',
  default_options: ['warning_level=0', 'c_std=c99'],
)

inc = include_directories('include')

ffnvcodec_dep = declare_dependency(
    include_directories: inc,
)
meson.override_dependency('ffnvcodec', ffnvcodec_dep)

install_headers(
    'include' / 'ffnvcodec' / 'dynlink_cuda.h',
    'include' / 'ffnvcodec' / 'dynlink_cuviddec.h',
    'include' / 'ffnvcodec' / 'dynlink_loader.h',
    'include' / 'ffnvcodec' / 'dynlink_nvcuvid.h',
    'include' / 'ffnvcodec' / 'nvEncodeAPI.h',
    subdir: 'ffnvcodec',
)

pkg_mod = import('pkgconfig')
pkg_mod.generate(
    name: 'ffnvcodec',
    description: 'FFmpeg version of Nvidia Codec SDK headers',
    subdirs: 'ffnvcodec',
)
