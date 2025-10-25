dep = [
  dependency('Qt6', modules: [
    'Core',
    'Gui',
    'Widgets',
    'Quick',
    'QuickControls2',
    'Network',
    'Svg',
  ]),
  dependency('openssl'),
  dependency('sdl2'),
  dependency('SDL2_ttf'),
  dependency('opus'),
  dependency('libavcodec'),
  dependency('libavutil'),
  dependency('libswscale'),
  dependency('libdrm'),
  dependency('libva'),
  dependency('libva-drm'),
  dependency('libva-wayland'),
  dependency('cuda'),
  dependency('libplacebo'),
  dependency('wayland-client'),
  dependency('egl'), # TODO: wrap
  dependency('opengl'),
  # TODO: maybe add mmal?
]

src = files(
  'main.cpp',
  'path.cpp',
  'wm.cpp',

  'cli' / 'commandlineparser.cpp',
  'cli' / 'pair.cpp',
  'cli' / 'listapps.cpp',
  'cli' / 'startstream.cpp',
  'cli' / 'quitstream.cpp',

  'backend' / 'computerseeker.cpp',
  'backend' / 'computermanager.cpp',
  'backend' / 'identitymanager.cpp',
  'backend' / 'boxartmanager.cpp',
  'backend' / 'richpresencemanager.cpp',
  'backend' / 'autoupdatechecker.cpp',
  'backend' / 'systemproperties.cpp',

  'backend' / 'nvapp.cpp',
  'backend' / 'nvaddress.cpp',
  'backend' / 'nvhttp.cpp',
  'backend' / 'nvcomputer.cpp',
  'backend' / 'nvpairingmanager.cpp',

  'settings' / 'compatfetcher.cpp',
  'settings' / 'mappingfetcher.cpp',
  'settings' / 'mappingmanager.cpp',
  'settings' / 'streamingpreferences.cpp',

  'streaming' / 'session.cpp',
  'streaming' / 'streamutils.cpp',

  'streaming' / 'video' / 'overlaymanager.cpp',

  'streaming' / 'audio' / 'audio.cpp',
  'streaming' / 'audio' / 'renderers' / 'sdlaud.cpp',

  'streaming' / 'input' / 'input.cpp',
  'streaming' / 'input' / 'gamepad.cpp',
  'streaming' / 'input' / 'keyboard.cpp',
  'streaming' / 'input' / 'mouse.cpp',
  'streaming' / 'input' / 'abstouch.cpp',
  'streaming' / 'input' / 'reltouch.cpp',
)
moc_sources = files(
  'backend' / 'computermanager.cpp',
  'backend' / 'boxartmanager.cpp',
)
moc_headers = files(
  'cli' / 'pair.h',
  'cli' / 'listapps.h',
  'cli' / 'startstream.h',
  'cli' / 'quitstream.h',

  'backend' / 'computerseeker.h',
  'backend' / 'computermanager.h',
  'backend' / 'boxartmanager.h',
  'backend' / 'autoupdatechecker.h',
  'backend' / 'nvhttp.h',
  'backend' / 'systemproperties.h',

  'settings' / 'compatfetcher.h',
  'settings' / 'mappingfetcher.h',
  'settings' / 'streamingpreferences.h',

  'streaming' / 'session.h',
)

# FFmpeg
add_project_arguments('-D', 'HAVE_FFMPEG', language: ['c', 'cpp'])
src += files(
  'streaming' / 'video' / 'ffmpeg.cpp',
  'streaming' / 'video' / 'ffmpeg-renderers' / 'genhwaccel.cpp',
  'streaming' / 'video' / 'ffmpeg-renderers' / 'sdlvid.cpp',
  'streaming' / 'video' / 'ffmpeg-renderers' / 'swframemapper.cpp',
  'streaming' / 'video' / 'ffmpeg-renderers' / 'pacer' / 'pacer.cpp',
)

# libva
add_project_arguments('-D', 'HAVE_LIBVA', language: ['c', 'cpp'])
src += files(
  'streaming' / 'video' / 'ffmpeg-renderers' / 'vaapi.cpp',
)
# libva-wayland
add_project_arguments('-D', 'HAVE_LIBVA_WAYLAND', language: ['c', 'cpp'])
# libva-drm
add_project_arguments('-D', 'HAVE_LIBVA_DRM', language: ['c', 'cpp'])

# libdrm
add_project_arguments('-D', 'HAVE_DRM', language: ['c', 'cpp'])
src += files(
  'streaming' / 'video' / 'ffmpeg-renderers' / 'drm.cpp',

  # linux hooks
  'masterhook.c',
  'masterhook_internal.c',
)

# CUDA
add_project_arguments('-D', 'HAVE_CUDA', language: ['c', 'cpp'])
src += files(
  'streaming' / 'video' / 'ffmpeg-renderers' / 'cuda.cpp',
)

# libplacebo
add_project_arguments('-D', 'HAVE_LIBPLACEBO_VULKAN', language: ['c', 'cpp'])
src += files(
  'streaming' / 'video' / 'ffmpeg-renderers' / 'plvk.cpp',
  'streaming' / 'video' / 'ffmpeg-renderers' / 'plvk_c.c',
)

# EGL
add_project_arguments('-D', 'HAVE_EGL', language: ['c', 'cpp'])
src += files(
  'streaming' / 'video' / 'ffmpeg-renderers' / 'eglvid.cpp',
  'streaming' / 'video' / 'ffmpeg-renderers' / 'egl_extensions.cpp',
  'streaming' / 'video' / 'ffmpeg-renderers' / 'eglimagefactory.cpp',
)

# TODO: maybe include Steam Link (config_SL)?

# soundio
add_project_arguments('-D', 'HAVE_SOUNDIO', language: ['c', 'cpp'])
add_project_arguments('-D', 'SOUNDIO_STATIC_LIBRARY', language: ['c', 'cpp'])
src += files(
  'streaming' / 'audio' / 'renderers' / 'soundioaudiorenderer.cpp',
)

# Wayland
# add_project_arguments('-D', 'HAVE_WAYLAND', language: ['c', 'cpp'])
add_project_arguments('-D', 'HAS_WAYLAND', language: ['c', 'cpp']) # a typo?
src += files(
  'streaming' / 'video' / 'ffmpeg-renderers' / 'pacer' / 'waylandvsyncsource.cpp',
)

qt = import('qt6')

subdir('gui')

moc_files = qt.compile_moc(
  sources: moc_sources,
  headers: moc_headers,
  dependencies: dep,
)

qrc = qt.compile_resources(sources: [
  'resources.qrc',
  'qml.qrc',
])

executable(
  'moonlight',
  src,
  qml_modules,
  moc_files,
  qrc,
  dependencies: [
    dep,
    moonlight_common_c_dep,
    qmdnsengine_dep,
    soundio_dep,
    h264bitstream_dep,
    ffnvcodec_dep,
  ],
  pie: true,
  win_subsystem: 'windows',
)

# desktop files
install_data(
  'deploy' / 'linux' / 'com.moonlight_stream.Moonlight.desktop',
  install_dir: get_option('datadir') / 'applications',
)
# icons
install_data(
  'res' / 'moonlight.svg',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
)
# appstream
install_data(
  'deploy' / 'linux' / 'com.moonlight_stream.Moonlight.appdata.xml',
  install_dir: get_option('datadir') / 'metainfo',
)
