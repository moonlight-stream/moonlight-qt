#!/bin/sh
#
# macOS 的应用内更新收尾脚本。
#
# 由 PortableUpdateInstaller 在退出前以 detached 方式启动：正在运行的 .app 不能自己
# 覆盖自己，所以下载、挂载、校验都在主进程里做完，只把「等进程退出 → 换掉 bundle →
# 重新拉起来」这三步留给这个脚本。
#
# 用法：install-dmg-update.sh <工作目录> <已安装的 bundle> <暂存好的新 bundle> <主进程 pid>
#
# 失败一律回滚，并把原因写到 ~/Library/Logs/Moonlight-update-error.log。

set -u

WORKSPACE="$1"
INSTALLED_APP="$2"
STAGED_APP="$3"
MAIN_PID="$4"

BACKUP_DIR="$WORKSPACE/backup"
LOG_FILE="$HOME/Library/Logs/Moonlight-update-error.log"

log_failure() {
    mkdir -p "$(dirname "$LOG_FILE")" 2>/dev/null
    printf '[%s] %s\n' "$(date '+%Y-%m-%d %H:%M:%S')" "$1" >> "$LOG_FILE"
}

cleanup() {
    [ -n "$WORKSPACE" ] && [ -d "$WORKSPACE" ] && rm -rf "$WORKSPACE"
}

# 等主进程退出。kill -0 只探测进程是否存在，不发信号。
# 30 秒还没退就放弃 —— 强杀一个可能正在串流的进程不值得。
waited=0
while kill -0 "$MAIN_PID" 2>/dev/null; do
    if [ "$waited" -ge 300 ]; then
        log_failure "Timed out waiting for Moonlight (pid $MAIN_PID) to exit; update not applied."
        cleanup
        exit 1
    fi
    sleep 0.1
    waited=$((waited + 1))
done

if [ ! -d "$STAGED_APP" ]; then
    log_failure "Staged app bundle is missing: $STAGED_APP"
    cleanup
    exit 1
fi

mkdir -p "$BACKUP_DIR" || {
    log_failure "Unable to create backup directory: $BACKUP_DIR"
    cleanup
    exit 1
}

# 旧 bundle 先挪进备份目录而不是直接删，这样换新失败时还能原样放回去
if [ -d "$INSTALLED_APP" ]; then
    if ! mv "$INSTALLED_APP" "$BACKUP_DIR/" 2>/dev/null; then
        log_failure "Unable to move the installed app aside: $INSTALLED_APP"
        cleanup
        exit 1
    fi
fi

# 暂存目录在缓存里，和 /Applications 可能不在同一个卷上，mv 会退化成复制 + 删除。
# 这一步不是原子的，所以上面留了备份。
if ! mv "$STAGED_APP" "$INSTALLED_APP" 2>/dev/null; then
    log_failure "Unable to move the new app into place: $INSTALLED_APP"

    # 跨卷时 mv 是复制 + 删除，失败可能已经在目标位置留下半个 bundle。不先清掉的话，
    # 下面这次 mv 会把备份塞进那个半成品里变成 Moonlight.app/Moonlight.app ——
    # 看着像回滚成功了，其实装的东西彻底坏掉。
    rm -rf "$INSTALLED_APP"

    BACKUP_APP="$BACKUP_DIR/$(basename "$INSTALLED_APP")"
    if [ -d "$BACKUP_APP" ] && mv "$BACKUP_APP" "$INSTALLED_APP" 2>/dev/null; then
        open -a "$INSTALLED_APP" 2>/dev/null
    else
        # 回滚也失败了：此时备份是唯一一份完整的安装，绝对不能跟着工作目录一起删。
        log_failure "Rollback failed. The previous version is still in $BACKUP_DIR — move it back manually."
        exit 1
    fi

    cleanup
    exit 1
fi

open -a "$INSTALLED_APP" 2>/dev/null || log_failure "Update installed but relaunch failed: $INSTALLED_APP"

cleanup
exit 0
