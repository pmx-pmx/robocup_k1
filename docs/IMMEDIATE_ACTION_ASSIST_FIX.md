# 🚨 立即行动：修复 Assist 节点圆周运动问题

## 📊 问题确认

通过 `bash scripts/view_state_log.sh` 的实时状态日志分析，已确认：

**机器人 #2 的实际状态：**
```
State: assist  ← 关键！不是 chase
Vel: (vx=1.92, vy=0.94, vtheta=0.45)
圆周半径: R = √(1.92² + 0.94²) / 0.45 ≈ 4.75 米
```

**问题根因：** Assist 节点的速度计算错误，直接用距离作为速度。

---

## ⚡ 立即修复（5分钟）

### 步骤 1：SSH 到机器人

```bash
ssh booster@192.168.50.205
```

### 步骤 2：修改行为树配置

```bash
cd /home/booster/robocup_k1/src/brain/behavior_trees/subtrees
nano subtree_striker_play.xml
```

### 步骤 3：找到第 43 行

**原始配置：**
```xml
<Assist _while="decision == 'assist'" vx_limit="3.0" vy_limit="1.8" />
```

**修改为：**
```xml
<Assist _while="decision == 'assist'" vx_limit="1.2" vy_limit="0.8" dist_tolerance="1.0" theta_tolerance="0.5" />
```

### 步骤 4：保存并重启

```bash
# Ctrl+O 保存, Ctrl+X 退出

# 重启 brain_node
sudo systemctl restart brain_node

# 或手动重启
pkill brain_node
cd /home/booster/robocup_k1
source install/setup.bash
ros2 run brain brain_node &
```

### 步骤 5：验证修复

```bash
# 观察状态日志
tail -f /tmp/state.log

# 预期：
# - State: assist 时
# - Vel: (vx < 1.2, vy < 0.8) ← 速度明显降低
# - 不再做大半径圆周运动
```

---

## 🔧 根本修复（30分钟）

在开发机上修改代码，修复速度计算逻辑。

### 修改文件：`brain_tree.cpp`

**位置：** 第 1548-1552 行

**原始代码：**
```cpp
} else {
    vx = targetPose_r.x;  // ❌ 错误
    vy = targetPose_r.y;  // ❌ 错误
    vtheta = brain->data->ball.yawToRobot * 2.0;
}
```

**修改为：**
```cpp
} else {
    // ✅ 使用比例控制
    const double dist = std::hypot(targetPose_r.x, targetPose_r.y);
    const double speedGain = 0.8;  // 速度增益
    const double maxSpeed = 1.2;   // 最大速度
    
    if (dist > 0.01) {
        const double targetSpeed = std::min(speedGain * dist, maxSpeed);
        vx = targetSpeed * targetPose_r.x / dist;
        vy = targetSpeed * targetPose_r.y / dist;
    } else {
        vx = 0.0;
        vy = 0.0;
    }
    vtheta = brain->data->ball.yawToRobot * 2.0;
    
    // 调试日志
    log(format("Assist move: dist=%.2f, speed=%.2f, vx=%.2f, vy=%.2f, vtheta=%.2f",
               dist, targetSpeed, vx, vy, vtheta));
}
```

### 编译部署

```bash
# 在开发机
cd /d/RISE/加速进化K1\ 3v3/robocup_k1
colcon build --packages-select brain

# 部署
scp install/lib/brain/brain_node booster@192.168.50.205:/home/booster/robocup_k1/install/lib/brain/

# 重启
ssh booster@192.168.50.205 "sudo systemctl restart brain_node"
```

---

## 📋 测试验证

### 测试 1：守门员靠近球

```
场景：
1. 守门员 #1 靠近球（< 1.2m）
2. 守门员成为队长
3. 前锋 #2 进入 assist 状态

预期：
- 前锋 #2 平稳移动到辅助位置
- 速度 < 1.2 m/s
- 不做圆周运动
- 到达后停止（dist < 1.0m）
```

### 测试 2：裁判机断连

```
场景：
1. 正常比赛
2. 拔掉裁判机网线
3. 观察前锋行为

预期：
- 即使进入 assist 状态
- 也不会做大半径圆周运动
- 速度受限 < 1.2 m/s
```

### 测试 3：多机配合

```
场景：
1. 3台机器人同时在场
2. 其中一台靠近球（队长）
3. 其他两台进入 assist

预期：
- 各自移动到辅助位置
- 不相互碰撞
- 到达后静止
```

---

## 📊 预期效果

### 修复前 ❌

```
Assist 速度限制: vx_limit=3.0, vy_limit=1.8
实际速度: vx=1.92, vy=0.94
圆周半径: 4.75米
结果: 高速圆周运动，永远到不了目标
```

### 立即修复后 ✅

```
Assist 速度限制: vx_limit=1.2, vy_limit=0.8
实际速度: vx≤1.2, vy≤0.8
圆周半径: ≈3.3米（仍可能）
结果: 速度降低，圆周半径减小
```

### 根本修复后 ✅✅✅

```
Assist 速度控制: 比例控制 (gain=0.8)
- 距离 2m → 速度 1.2 m/s
- 距离 1m → 速度 0.8 m/s
- 距离 0.5m → 速度 0.4 m/s
- 距离 < 1m → 停止
结果: 平稳到达，不再圆周运动
```

---

## 🎯 关键要点

### 为什么是 Assist 而不是 SimpleChase？

之前的分析基于**假设**（裁判机断连 → decision="chase"），但实际状态日志显示：

| 分析 | 假设的场景 | 实际的场景 |
|------|----------|----------|
| 节点 | SimpleChase | **Assist** |
| decision | chase | **assist** |
| 触发原因 | 裁判机断连 → 追球 | **守门员成为队长 → 辅助** |
| 圆周半径 | 1米 | **4.75米** |
| 根本原因 | 追记忆中的球 | **追动态变化的辅助位置** |

### Assist 速度计算的错误

```cpp
// ❌ 错误：直接用距离作为速度
vx = targetPose_r.x;  // 2米 → 2 m/s
vy = targetPose_r.y;  // 1米 → 1 m/s

// ✅ 正确：比例控制
speed = min(0.8 * dist, 1.2)  // 2米 → 1.2 m/s
vx = speed * cos(angle)
vy = speed * sin(angle)
```

### 为什么会形成圆周运动？

```
1. 计算辅助位置 targetPose（依赖球位置）
2. 转换到机器人坐标系 targetPose_r
3. 设置速度 vx=targetPose_r.x (错误！)
4. 机器人移动
5. 球的相对位置改变
6. targetPose 重新计算（位置改变了）
7. 新的 targetPose_r 距离仍然约 2 米
8. 回到步骤 3，永远循环
→ 圆周运动
```

---

## ✅ 完成清单

- [ ] **立即修复：** 修改 `subtree_striker_play.xml`（降低速度限制）
- [ ] **验证：** 观察状态日志，确认速度降低
- [ ] **根本修复：** 修改 `brain_tree.cpp`（比例控制）
- [ ] **测试：** 守门员靠近球场景
- [ ] **测试：** 裁判机断连场景
- [ ] **测试：** 多机配合场景
- [ ] **文档：** 更新 CHANGELOG.md ✅（已完成）
- [ ] **提交：** 推送到 GitHub pmx-3 分支

---

## 📞 相关文档

| 文档 | 路径 | 用途 |
|------|------|------|
| **根因分析** | `docs/assist_circular_motion_root_cause.md` | 完整技术分析 |
| **变更日志** | `CHANGELOG.md` | 问题摘要和修复说明 |
| **立即行动** | `docs/IMMEDIATE_ACTION_ASSIST_FIX.md` | 本文件 |
| **历史分析** | `ANALYSIS_GC_DISCONNECT_SPINNING.md` | SimpleChase 的分析（参考） |

---

**现在就开始修复！立即修改 XML 文件，5分钟内可以看到效果。** 🚀
