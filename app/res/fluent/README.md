# Fluent UI System Icons

设置页分类导航和顶部工具栏用的图标，取自 [microsoft/fluentui-system-icons](https://github.com/microsoft/fluentui-system-icons)，
MIT License, Copyright (c) 2020 Microsoft Corporation.

用的是 24px Regular 变体，唯一的改动是把 `fill="#212121"` 换成 `fill="#FFFFFF"`，
因为我们的界面是深色底，图标在 QML 里靠 `opacity` 区分选中态。

工具栏原来混用的是 Material 的 48px filled 图标（`ic_*_white_48px.svg`、`settings.svg` 等），
实心笔画又粗又满，和这套线性几何图标放在一条 bar 上明显不是一个语言，所以整排换掉了。

| 文件 | 上游资源 |
| --- | --- |
| `cat-basic.svg` | `Options/SVG/ic_fluent_options_24_regular.svg` |
| `cat-audio.svg` | `Speaker 2/SVG/ic_fluent_speaker_2_24_regular.svg` |
| `cat-host.svg` | `Desktop/SVG/ic_fluent_desktop_24_regular.svg` |
| `cat-ui.svg` | `Color/SVG/ic_fluent_color_24_regular.svg` |
| `cat-input.svg` | `Keyboard/SVG/ic_fluent_keyboard_24_regular.svg` |
| `cat-gamepad.svg` | `Xbox Controller/SVG/ic_fluent_xbox_controller_24_regular.svg` |
| `cat-display.svg` | `Full Screen Maximize/SVG/ic_fluent_full_screen_maximize_24_regular.svg` |
| `cat-advanced.svg` | `Wrench/SVG/ic_fluent_wrench_24_regular.svg` |
| `tb-back.svg` | `Arrow Left/SVG/ic_fluent_arrow_left_24_regular.svg` |
| `tb-add-pc.svg` | `Desktop Arrow Right/SVG/ic_fluent_desktop_arrow_right_24_regular.svg` |
| `tb-update.svg` | `Arrow Sync/SVG/ic_fluent_arrow_sync_24_regular.svg` |
| `tb-help.svg` | `Question Circle/SVG/ic_fluent_question_circle_24_regular.svg` |
| `tb-gamepad.svg` | `Xbox Controller/SVG/ic_fluent_xbox_controller_24_regular.svg` |
| `tb-network.svg` | `Plug Connected/SVG/ic_fluent_plug_connected_24_regular.svg` |
| `tb-display.svg` | `Desktop/SVG/ic_fluent_desktop_24_regular.svg` |
| `tb-settings.svg` | `Settings/SVG/ic_fluent_settings_24_regular.svg` |
