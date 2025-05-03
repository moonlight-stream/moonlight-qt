src += files(
  'appmodel.cpp',
  'computermodel.cpp',
  'sdlgamepadkeynavigation.cpp',
)

moc_sources += files(
  'computermodel.cpp',
)

moc_headers += files(
  'appmodel.h',
  'computermodel.h',
  'sdlgamepadkeynavigation.h',
)

qml_modules = qt.qml_module(
  'com.moonlight_stream.Moonlight',
  qml_sources: files(
    # 'main.qml',

    'AppView.qml',
    'PcView.qml',
    'SettingsView.qml',

    'NavigableDialog.qml',
    'NavigableItemDelegate.qml',
    'NavigableMenu.qml',
    'NavigableMenuItem.qml',
    'NavigableMessageDialog.qml',
    'NavigableToolButton.qml',

    'AutoResizingComboBox.qml',
    'CenteredGridView.qml',
    'ErrorMessageDialog.qml',
    'GamepadMapper.qml',

    'CliPair.qml',
    'StreamSegue.qml',
    'CliQuitStreamSegue.qml',
    'CliStartStreamSegue.qml',
    'QuitSegue.qml',
  ),
)
