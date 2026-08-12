#!/bin/bash
set -euo pipefail

# install-qt-action 把 Qt 装在 workspace 下（不是 C:\Qt），并导出 QT_ROOT_DIR，
# 所以路径不能写死。本地跑时退回 PATH 里的 lrelease。
if [[ -n "${QT_ROOT_DIR:-}" && -x "$QT_ROOT_DIR/bin/lrelease" ]]; then
    LRELEASE="$QT_ROOT_DIR/bin/lrelease"
elif [[ -n "${QT_ROOT_DIR:-}" && -x "$QT_ROOT_DIR/bin/lrelease.exe" ]]; then
    LRELEASE="$QT_ROOT_DIR/bin/lrelease.exe"
else
    LRELEASE="$(command -v lrelease)"
fi
echo "Using $LRELEASE"

# 编译所有翻译文件
for f in app/languages/*.ts; do
    echo "Processing $f..."
    "$LRELEASE" "$f"
done

echo "Translation compilation completed!"
