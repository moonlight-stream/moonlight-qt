add_project_arguments('-D', 'QT_NO_SIGNALS_SLOTS_KEYWORDS', language: ['cpp'])

lib_args = ['-D', 'QMDNSENGINE_LIBRARY']
if get_option('default_library') != 'static'
  lib_args += ['-D', 'BUILD_SHARED_LIBS']
endif

inc = include_directories('.', 'include')

qt5_dep = dependency('Qt5', modules: ['Core', 'Gui', 'Widgets', 'Network'])

qt = import('qt5')

moc_files =  qt.compile_moc(
  headers: [
    'include' / 'qmdnsengine' / 'abstractserver.h',
    'include' / 'qmdnsengine' / 'browser.h',
    'include' / 'qmdnsengine' / 'cache.h',
    'include' / 'qmdnsengine' / 'hostname.h',
    'include' / 'qmdnsengine' / 'prober.h',
    'include' / 'qmdnsengine' / 'provider.h',
    'include' / 'qmdnsengine' / 'resolver.h',
    'include' / 'qmdnsengine' / 'server.h',

    'src' / 'bitmap_p.h',
    'src' / 'browser_p.h',
    'src' / 'cache_p.h',
    'src' / 'hostname_p.h',
    'src' / 'message_p.h',
    'src' / 'prober_p.h',
    'src' / 'provider_p.h',
    'src' / 'query_p.h',
    'src' / 'resolver_p.h',
    'src' / 'server_p.h',
    'src' / 'service_p.h',
  ],
  extra_args: lib_args,
  dependencies: [qt5_dep],
)

lib = library(
  'qmdnsengine',
  moc_files,
  'src' / 'abstractserver.cpp',
  'src' / 'bitmap.cpp',
  'src' / 'browser.cpp',
  'src' / 'cache.cpp',
  'src' / 'dns.cpp',
  'src' / 'hostname.cpp',
  'src' / 'mdns.cpp',
  'src' / 'message.cpp',
  'src' / 'prober.cpp',
  'src' / 'provider.cpp',
  'src' / 'query.cpp',
  'src' / 'record.cpp',
  'src' / 'resolver.cpp',
  'src' / 'server.cpp',
  'src' / 'service.cpp',
  cpp_args: lib_args,
  include_directories: inc,
  dependencies: [qt5_dep],
  gnu_symbol_visibility: 'hidden',
  install: true,
)

# Make this library usable as a Meson subproject.
qmdnsengine_dep = declare_dependency(
  include_directories: inc,
  link_with: lib,
)
meson.override_dependency('qmdnsengine', qmdnsengine_dep)

# Make this library usable from the system's
# package manager.
install_headers(
  'qmdnsengine_export.h',
  'include' / 'qmdnsengine' / 'abstractserver.h',
  'include' / 'qmdnsengine' / 'bitmap.h',
  'include' / 'qmdnsengine' / 'browser.h',
  'include' / 'qmdnsengine' / 'cache.h',
  'include' / 'qmdnsengine' / 'dns.h',
  'include' / 'qmdnsengine' / 'hostname.h',
  'include' / 'qmdnsengine' / 'mdns.h',
  'include' / 'qmdnsengine' / 'message.h',
  'include' / 'qmdnsengine' / 'prober.h',
  'include' / 'qmdnsengine' / 'provider.h',
  'include' / 'qmdnsengine' / 'query.h',
  'include' / 'qmdnsengine' / 'record.h',
  'include' / 'qmdnsengine' / 'resolver.h',
  'include' / 'qmdnsengine' / 'server.h',
  'include' / 'qmdnsengine' / 'service.h',
  subdir: 'qmdnsengine',
)

pkg_mod = import('pkgconfig')
pkg_mod.generate(
  lib,
  description: 'Multicast DNS library for Qt applications',
  subdirs: 'qmdnsengine',
)
