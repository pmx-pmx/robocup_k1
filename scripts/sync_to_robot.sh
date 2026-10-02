#!/bin/bash

# 同步脚本：将本地代码同步到机器人
# 用法:
#   ./scripts/sync_to_robot.sh             # 同步到默认机器人 (192.168.50.205)
#   ./scripts/sync_to_robot.sh 143         # 同步到 192.168.50.143
#   ./scripts/sync_to_robot.sh 205         # 同步到 192.168.50.205
#   ./scripts/sync_to_robot.sh all         # 同步到所有机器人

ROBOT_USER="booster"
LOCAL_PATH="$(cd "$(dirname "$0")/.." && pwd)"

# 获取机器人路径的函数
get_robot_path() {
  local ip=$1
  case "$ip" in
    "192.168.50.205")
      echo "/home/booster/Robocup_bigkick/robocup_k1"
      ;;
    "192.168.50.143")
      echo "/home/booster/robocup_ws_replace/robocup_k1"
      ;;
    *)
      echo "/home/booster/robocup_k1"  # 默认路径
      ;;
  esac
}

# 同步函数
sync_to_robot() {
  local ROBOT_IP=$1
  local REMOTE_PATH=$(get_robot_path "$ROBOT_IP")

  echo ""
  echo "========================================="
  echo "正在同步代码到 ${ROBOT_USER}@${ROBOT_IP}:${REMOTE_PATH}"
  echo "本地路径: ${LOCAL_PATH}"
  echo "========================================="

  # 使用 rsync 同步，排除不需要的目录
  rsync -avz --progress \
    --exclude 'build/' \
    --exclude 'install/' \
    --exclude 'log/' \
    --exclude '.git/' \
    --exclude '*.pyc' \
    --exclude '__pycache__/' \
    --exclude '.vscode/' \
    --exclude '.idea/' \
    "${LOCAL_PATH}/" \
    "${ROBOT_USER}@${ROBOT_IP}:${REMOTE_PATH}/"

  if [ $? -eq 0 ]; then
    echo "✓ 同步到 ${ROBOT_IP} 完成！"
    echo ""
    echo "接下来可以 SSH 到机器人进行编译："
    echo "  ssh ${ROBOT_USER}@${ROBOT_IP}"
    echo "  cd ${REMOTE_PATH}"
    echo "  ./scripts/build.sh"
    return 0
  else
    echo "✗ 同步到 ${ROBOT_IP} 失败，请检查网络连接和 SSH 配置"
    return 1
  fi
}

# 主逻辑
if [ "$1" == "all" ]; then
  # 同步到所有机器人
  echo "将同步到所有机器人..."
  sync_to_robot "192.168.50.205"
  sync_to_robot "192.168.50.143"
elif [ "$1" == "143" ]; then
  # 同步到 192.168.50.143
  sync_to_robot "192.168.50.143"
elif [ "$1" == "205" ]; then
  # 同步到 192.168.50.205
  sync_to_robot "192.168.50.205"
elif [ -z "$1" ]; then
  # 默认同步到 192.168.50.205
  sync_to_robot "192.168.50.205"
else
  # 自定义 IP
  sync_to_robot "$1"
fi
