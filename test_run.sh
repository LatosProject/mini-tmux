#!/bin/bash

# 清理旧进程和文件
pkill -9 mini-tmux 2>/dev/null
rm -rf /tmp/mini-tmux-* 2>/dev/null

# 在 script 环境中运行（提供 TTY）
timeout 2 script -q -c "./mini-tmux" /dev/null &
PID=$!

# 等待进程启动
sleep 1

# 检查日志
echo "=== Server Log ==="
cat /tmp/mini-tmux-*/server.log 2>/dev/null || echo "No server log"

echo ""
echo "=== Client Log ==="
cat /tmp/mini-tmux-*/client.log 2>/dev/null || echo "No client log"

# 清理
wait $PID 2>/dev/null
pkill -9 mini-tmux 2>/dev/null
