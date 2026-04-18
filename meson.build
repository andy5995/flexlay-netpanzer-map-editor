project('netpanzer-map-editor', 'cpp',
  version : '0.1',
  meson_version : '>= 0.61.0',
  default_options : ['cpp_std=c++17', 'warning_level=3']
)

qt5     = import('qt5')
qt5_dep = dependency('qt5', modules : ['Core', 'Gui', 'Widgets'])
qt5_test_dep = dependency('qt5', modules : ['Core', 'Test'])

# Sources that need MOC processing (contain Q_OBJECT)
moc_headers = [
  'src/mapview.h',
  'src/tilepanel.h',
  'src/tilebrowser.h',
  'src/stamppanel.h',
  'src/mainwindow.h',
  'src/minimap.h',
]
moc_files = qt5.preprocess(moc_headers : moc_headers)

editor_sources = [
  'src/main.cpp',
  'src/maploader.cpp',
  'src/mapview.cpp',
  'src/tlsloader.cpp',
  'src/tilepanel.cpp',
  'src/tilebrowser.cpp',
  'src/stamppanel.cpp',
  'src/mainwindow.cpp',
  'src/minimap.cpp',
]

executable('netpanzer-editor',
  sources             : editor_sources + moc_files,
  dependencies        : qt5_dep,
  include_directories : include_directories('src'),
  install             : true,
)

# ---------------------------------------------------------------------------
# Install — Linux only (Windows packaging is handled by the Inno Setup installer)

if host_machine.system() == 'linux'
  install_data('netpanzer-editor.desktop',
    install_dir : get_option('datadir') / 'applications')

  install_data('data/images/icons24/stock_save.png',
    install_dir : get_option('datadir') / 'pixmaps',
    rename      : 'netpanzer-editor.png')

  install_data(['COPYING', 'README'],
    install_dir : get_option('datadir') / 'doc' / 'netpanzer-editor')
endif

# ---------------------------------------------------------------------------
# Tests
# test_maploader.cpp uses Q_OBJECT (QTEST_MAIN expands to it), so we list it
# under moc_sources so Meson generates the .moc file included at the bottom.

test_moc = qt5.preprocess(moc_sources : ['tests/test_maploader.cpp'])

test_exe = executable('test_maploader',
  sources : ['tests/test_maploader.cpp', 'src/maploader.cpp'] + test_moc,
  dependencies : qt5_test_dep,
  include_directories : include_directories('src'),
)

test('maploader', test_exe,
  env : ['LSAN_OPTIONS=suppressions=' + meson.project_source_root() / 'asan.supp'])

tls_moc = qt5.preprocess(moc_sources : ['tests/test_tlsloader.cpp'])

qt5_test_gui_dep = dependency('qt5', modules : ['Core', 'Gui', 'Test'])

tls_exe = executable('test_tlsloader',
  sources : ['tests/test_tlsloader.cpp', 'src/tlsloader.cpp'] + tls_moc,
  dependencies : qt5_test_gui_dep,
  include_directories : include_directories('src'),
)

test('tlsloader', tls_exe,
  env : [
    'LSAN_OPTIONS=suppressions=' + meson.project_source_root() / 'asan.supp',
    'QT_QPA_PLATFORM=offscreen',
  ])
