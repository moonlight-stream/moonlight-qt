#!/usr/bin/env bash
# 列出上游（moonlight-stream/moonlight-qt）有、我们还没处理的提交。
#
# 判重不能用 git cherry —— 它比的是 patch-id，而我们的文件和上游有分叉，
# 同一个改动落到我们这边上下文不同，patch-id 就对不上，已经合入的提交会被
# 报成新的。这里改用 cherry-pick -x 在提交信息里留下的
# 「(cherry picked from commit <sha>)」做锚点，所以**摘上游提交时必须带 -x**。
#
# 用法：
#   scripts/upstream-status.sh              拉取上游后列出
#   scripts/upstream-status.sh --no-fetch   跳过拉取，用本地已有的 upstream/master

set -euo pipefail

UPSTREAM_URL="https://github.com/moonlight-stream/moonlight-qt.git"
BASE_BRANCH="master"
SKIP_FILE="$(dirname "$0")/upstream-skip.txt"

if [ "${1:-}" != "--no-fetch" ]; then
    if ! git remote get-url upstream > /dev/null 2>&1; then
        echo "添加 upstream remote..."
        git remote add upstream "$UPSTREAM_URL"
    fi
    # 防手滑往上游推。放在 if 外面：remote 可能是别人先手工加的，那种情况下
    # push url 还是继承 fetch url，有上游写权限的人一个 git push upstream 就出去了。
    git remote set-url --push upstream DISABLED
    echo "拉取上游..."
    git fetch upstream --quiet
fi

# 已经落进 master 的：提交信息里带 cherry-pick 来源
merged=$(git log "$BASE_BRANCH" --format=%B | grep -oE "cherry picked from commit [0-9a-f]{40}" | awk '{print $5}' || true)
# 还在别的分支上（PR 进行中）。只看本地分支和 origin 的远端分支 —— 用 --all 会把
# upstream/* 和 tag 也算进来，上游自己在分支间 cherry-pick 过的提交就会被误判成
# 「我们正在处理」，从待评估里凭空消失。
inflight_refs=$(git for-each-ref --format='%(refname)' refs/heads refs/remotes/origin || true)
if [ -n "$inflight_refs" ]; then
    inflight=$(git log $inflight_refs --not "$BASE_BRANCH" --format=%B | grep -oE "cherry picked from commit [0-9a-f]{40}" | awk '{print $5}' || true)
else
    inflight=""
fi

pending=0
while read -r full short subj; do
    [ -z "$full" ] && continue

    if grep -q "$full" <<< "$merged"; then
        printf '  已合入   %s  %s\n' "$short" "$subj"
        continue
    fi

    if grep -q "$full" <<< "$inflight"; then
        printf '  进行中   %s  %s\n' "$short" "$subj"
        continue
    fi

    # 主动决定不跟的，原因记在 upstream-skip.txt
    if [ -f "$SKIP_FILE" ] && reason=$(grep "^$short" "$SKIP_FILE" | head -1 | cut -d' ' -f2-); [ -n "${reason:-}" ]; then
        printf '  已跳过   %s  %s\n           └─ %s\n' "$short" "$subj" "$reason"
        continue
    fi

    printf '★ 待评估   %s  %s\n' "$short" "$subj"
    pending=$((pending + 1))
done < <(git log --no-merges --format="%H %h %s" "$BASE_BRANCH..upstream/$BASE_BRANCH")

echo
if [ "$pending" -eq 0 ]; then
    echo "没有待评估的上游提交。"
else
    echo "$pending 个待评估。跟进后用 git cherry-pick -x <sha>（-x 不能省，判重靠它）；"
    echo "决定不跟的话把 <短 sha> 和原因写进 $SKIP_FILE。"
fi
