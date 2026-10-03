#!/bin/bash
# 实时查看状态日志的脚本

STATE_LOG="/tmp/state.log"

# 检查日志文件是否存在
if [ ! -f "$STATE_LOG" ]; then
    echo "警告: 状态日志文件不存在: $STATE_LOG"
    echo "等待日志文件创建..."
    while [ ! -f "$STATE_LOG" ]; do
        sleep 1
    done
fi

# 清屏并开始实时显示
clear
echo "======================================================================"
echo "                    实时状态监控 - State Logger"
echo "======================================================================"
echo "日志文件: $STATE_LOG"
echo "按 Ctrl+C 退出"
echo "======================================================================"
echo ""

# 使用 tail -f 实时跟踪日志，并清屏显示
tail -f "$STATE_LOG" | while read line; do
    # 如果是分隔线，清屏重新显示
    if [[ "$line" == "=="* ]]; then
        clear
    fi
    echo "$line"
done
