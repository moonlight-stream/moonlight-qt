#!/usr/bin/env bash
# QML 类型解析门禁。
#
# 只盯一类问题：类型解析失败（「X was not found」/「X is not a type」）。
# 这一类是运行期致命的 —— qmlcachegen 不做完整类型解析，编译能过、CI 全绿，
# 但那个 .qml 在运行期根本加载不了。#153 就是这么漏出去的：AppView.qml 写着
# import QtQuick 2.9 却用了 2.15 才有的 HoverHandler，结果点 PC 完全没反应。
#
# 其它 qmllint 警告（unqualified access、deprecated 之类）不在门禁范围内，
# 那些是风格问题，仓库里存量不少，混进来会让这个检查失去意义。
#
# 用法：scripts/qmllint-check.sh

set -euo pipefail

cd "$(dirname "$0")/.."

# 找 qmllint：CI 上 install-qt-action 会设 QT_ROOT_DIR
if [ -n "${QT_ROOT_DIR:-}" ] && [ -x "$QT_ROOT_DIR/bin/qmllint" ]; then
    QMLLINT="$QT_ROOT_DIR/bin/qmllint"
elif command -v qmllint > /dev/null 2>&1; then
    QMLLINT="$(command -v qmllint)"
else
    echo "找不到 qmllint（设 QT_ROOT_DIR 或把它放进 PATH）" >&2
    exit 1
fi

echo "qmllint: $QMLLINT"

# 不用 mapfile：macOS 自带的 bash 是 3.2，没有这个内建
QML_FILES=()
while IFS= read -r f; do
    QML_FILES+=("$f")
done < <(git ls-files 'app/gui/*.qml' 'app/gui/**/*.qml')
if [ "${#QML_FILES[@]}" -eq 0 ]; then
    echo "没找到 QML 文件" >&2
    exit 1
fi
echo "检查 ${#QML_FILES[@]} 个 QML 文件"

# C++ 注册给 QML 的类型 qmllint 看不见，只能豁免。名单连同模块名和主版本号
# 一起从注册处现推，别硬编码 —— 将来注册了新类型不用回来改这个脚本。
# 每行形如：<QML 类型名> <模块 URI> <主版本号>
cpp_types=$(grep -rhoE "qmlRegister(Singleton|Uncreatable)?Type<[A-Za-z_]+>\(\"[A-Za-z_.]+\", *[0-9]+, *[0-9]+" app \
            | sed -E 's/^qmlRegister(Singleton|Uncreatable)?Type<([A-Za-z_]+)>\("([A-Za-z_.]+)", *([0-9]+).*/\2 \3 \4/' \
            | sort -u)
if [ -z "$cpp_types" ]; then
    echo "没能从 C++ 里推出注册类型名单，检查一下 grep 模式是不是过时了" >&2
    exit 1
fi
echo "豁免的 C++ 注册类型："
echo "$cpp_types" | sed 's/^/  /'

# -I app/gui 让本地组件（Panel / MicroLabel / NavigableDialog 这些）能被解析到。
# 不给这个参数的话本地组件会刷满同一类警告，真问题就被埋掉了 —— #153 当初
# 就是这么被我从输出里 grep 掉的。
raw=$("$QMLLINT" -I app/gui "${QML_FILES[@]}" 2>&1 || true)

resolution_failures=$(echo "$raw" | grep -E "was not found|is not a type" || true)

# 豁免要卡到「这个文件确实 import 了那个模块」为止。只按类型名放行的话，
# 某个 .qml 忘了写 import AppModel 1.0 也会被一起放过 —— 而那同样是运行期
# 加载失败。
remaining=""
while IFS= read -r line; do
    [ -z "$line" ] && continue

    file=$(echo "$line" | sed -E 's/^[A-Za-z]+: ([^:]+):[0-9]+:[0-9]+:.*/\1/')
    type=$(echo "$line" | sed -E 's/.*: ([A-Za-z_]+) (was not found|is not a type).*/\1/')

    exempt=0
    if [ -f "$file" ]; then
        reg=$(echo "$cpp_types" | awk -v t="$type" '$1 == t {print; exit}')
        if [ -n "$reg" ]; then
            uri=$(echo "$reg" | awk '{print $2}')
            major=$(echo "$reg" | awk '{print $3}')
            # 接受 import <URI> <major>.<minor>，也接受无版本号的写法
            if grep -qE "^import ${uri}( ${major}\.[0-9]+)?[[:space:]]*$" "$file"; then
                exempt=1
            fi
        fi
    fi

    if [ "$exempt" -eq 0 ]; then
        remaining="${remaining}${line}
"
    fi
done <<< "$resolution_failures"
remaining=$(echo "$remaining" | sed '/^$/d')

if [ -n "$remaining" ]; then
    echo
    echo "==> 有类型解析不了。这些 .qml 在运行期会加载失败："
    echo
    echo "$remaining"
    echo
    echo "常见原因：用了比文件顶部 import 版本更新的类型（例如 import QtQuick 2.9"
    echo "配 HoverHandler，那个类型要 2.15）。改成不带版本号的 import 通常就好了。"
    exit 1
fi

echo "类型解析检查通过。"
