# 界面字体

新粗野主义（neo-brutalism）视觉的一半靠字体：几何变宽的 Manrope 做正文和标题，
等宽的 DM Mono 做数字、状态徽标和宽字距微标签。两者都是 SIL Open Font License 1.1，
静态实例取自 [Google Fonts](https://fonts.google.com/)，文件内容未作任何修改。

| 文件 | 家族 / 字重 | 上游 |
| --- | --- | --- |
| `Manrope-Regular.ttf` | Manrope 400 | [sharanda/manrope](https://github.com/sharanda/manrope) |
| `Manrope-SemiBold.ttf` | Manrope 600 | 同上 |
| `Manrope-ExtraBold.ttf` | Manrope 800 | 同上 |
| `DMMono-Regular.ttf` | DM Mono 400 | [googlefonts/dm-mono](https://github.com/googlefonts/dm-mono) |

许可全文见 `OFL-Manrope.txt`、`OFL-DMMono.txt`。

用静态实例而不是可变字体（上游只提供 `Manrope[wght].ttf`），是为了让
`QFontDatabase::addApplicationFont()` 注册后字重解析是确定的 —— 三个静态文件都带
typographic family（name ID 16/17），Qt 会把它们归到同一个 `Manrope` 家族下的
400/600/800，QML 里直接写 `font.weight` 就能选中。

**不打包中文字体。** Manrope 和 DM Mono 都没有 CJK 字形，中文靠
`app/main.cpp` 里 `QFont::setFamilies()` 的回退链交给系统字体
（macOS 是 PingFang SC，Windows 是 Microsoft YaHei UI），
和参考站自己的 fallback 链一致；打包 Noto Sans SC 会让安装包大好几 MB。
