#!/bin/bash
# 实时监控机器人 192.168.50.205 状态，分析 Assist 修复效果
# 使用方法: bash scripts/monitor_assist_fix.sh

ROBOT_IP="192.168.50.205"
ROBOT_USER="booster"

echo "=== 实时监控 Assist 节点修复效果 ==="
echo "机器人: ${ROBOT_IP}"
echo "按 Ctrl+C 停止监控"
echo ""
echo "时间戳    | 状态     | 速度(vx, vy, vθ)      | 圆周半径 | 警告"
echo "--------------------------------------------------------------------------"

ssh ${ROBOT_USER}@${ROBOT_IP} "tail -f /tmp/state.log" | awk '
BEGIN {
    assist_count = 0
    assist_overspeed_count = 0
    circular_motion_count = 0
}
{
    # 提取 State
    if ($0 ~ /State:/) {
        state = $0
        gsub(/.*State: */, "", state)
        gsub(/,.*/, "", state)
    }

    # 提取 Vel
    if ($0 ~ /Vel: *\(/) {
        vel = $0
        gsub(/.*Vel: *\(/, "", vel)
        gsub(/\).*/, "", vel)
        split(vel, v, ",")
        vx = v[1] + 0
        vy = v[2] + 0
        vtheta = v[3] + 0

        # 计算线速度
        linear_speed = sqrt(vx*vx + vy*vy)

        # 计算圆周半径
        if (vtheta > 0.01 || vtheta < -0.01) {
            radius = linear_speed / (vtheta < 0 ? -vtheta : vtheta)
        } else {
            radius = 0
        }

        # 时间戳
        cmd = "date +\"%H:%M:%S\""
        cmd | getline timestamp
        close(cmd)

        # 分析异常
        warning = ""

        # 检查 Assist 状态
        if (state == "assist") {
            assist_count++

            # 检查速度是否超限（修复后应该 vx≤1.2, vy≤0.8）
            if (vx > 1.3 || vx < -1.3 || vy > 0.9 || vy < -0.9) {
                warning = "❌ ASSIST速度超限"
                assist_overspeed_count++
            }

            # 检查是否形成大半径圆周运动（修复后应该 R<3.0m）
            if (radius > 3.5 && linear_speed > 0.5) {
                warning = warning " ❌ 大半径圆周(R>3.5m)"
                circular_motion_count++
            } else if (radius > 2.0 && linear_speed > 0.5) {
                warning = warning " ⚠️  圆周半径偏大(R>2.0m)"
            } else if (linear_speed > 0.1) {
                warning = warning " ✅ 速度正常"
            }
        }

        # 检查 Chase 状态
        if (state == "chase") {
            if (radius > 2.0 && linear_speed > 0.5) {
                warning = "⚠️  Chase圆周运动"
            }
        }

        # 输出
        printf "%s | %-8s | (%5.2f,%5.2f,%5.2f) | R=%5.2fm | %s\n",
               timestamp, state, vx, vy, vtheta, radius, warning
        fflush()

        # 每 50 条记录输出统计
        if (assist_count > 0 && assist_count % 50 == 0) {
            printf "\n--- 统计 (最近 %d 条 Assist 记录) ---\n", assist_count
            printf "  速度超限次数: %d (%.1f%%)\n", assist_overspeed_count, (assist_overspeed_count*100.0/assist_count)
            printf "  圆周运动次数: %d (%.1f%%)\n", circular_motion_count, (circular_motion_count*100.0/assist_count)
            if (assist_overspeed_count == 0 && circular_motion_count == 0) {
                printf "  ✅ 修复有效！\n"
            } else {
                printf "  ❌ 仍有问题\n"
            }
            printf "\n"
        }
    }
}
'
