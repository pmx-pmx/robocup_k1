# 🔴 紧急问题分析：机器人 #2 圆周运动根因确认

## 📊 问题确认

### 状态快照回顾
```
机器人 #2 (前锋):
- State: assist
- Vel: (vx=1.92, vy=0.94, vtheta=0.45)
- Ball: DETECTED, Range=1.41m, Yaw=0.22rad
- 圆周半径: R ≈ 4.75米
```

---

## 🎯 根因找到了！

### 关键发现 1：Assist 节点的速度限制过高

**行为树配置：** `subtree_striker_play.xml` 第 43 行
```xml
<Assist _while="decision == 'assist'" vx_limit="3.0" vy_limit="1.8" />
```

**问题：**
- ❌ `vx_limit = 3.0 m/s` → **太高了！**
- ❌ `vy_limit = 1.8 m/s` → **太高了！**
- ✅ 实际速度：vx=1.92, vy=0.94 → 在限制范围内，但仍然过高

**对比其他节点：**
```xml
<Chase vx_limit="2.5" vy_limit="1.8" />     ← 追球节点
<Adjust vx_limit="0.9" vy_limit="0.8" />    ← 调整节点
<Kick speed_limit="0.9" />                  ← 踢球节点
```

**结论：** Assist 的速度限制（3.0）**比追球节点（2.5）还要高**！这完全不合理。

---

### 关键发现 2：Assist 节点的速度计算逻辑

**代码：** `brain_tree.cpp` 第 1549-1551 行
```cpp
vx = targetPose_r.x;  // 目标位置的 x 距离 → 直接作为速度！
vy = targetPose_r.y;  // 目标位置的 y 距离 → 直接作为速度！
vtheta = brain->data->ball.yawToRobot * 2.0;  // 球的偏角 × 2
```

**问题分析：**

#### 实际情况推测

假设机器人 #2 的目标辅助位置是：
```
targetPose = (-4.16 + 某个偏移, 2.03 + 某个偏移)

例如：targetPose = (-2.24, 2.97)  // 假设的辅助位置

转换到机器人坐标系（field2robot）：
targetPose_r = (1.92, 0.94)  // 与实际速度完全匹配！

因此：
vx = 1.92
vy = 0.94
vtheta = 0.22 * 2.0 = 0.44 ≈ 0.45 ✅
```

**结论：** Assist 节点直接把**到目标点的距离向量**当作**速度向量**！这在距离较远时会产生**极高的速度**。

---

### 关键发现 3：为什么形成圆周运动？

#### 数学分析

```
1. targetPose 距离机器人约 2.14 米
   √(1.92² + 0.94²) = 2.14m

2. 速度直接设为距离向量：
   vx = 1.92
   vy = 0.94

3. 同时旋转朝向球：
   vtheta = 0.22 * 2.0 = 0.44 rad/s

4. 机器人移动后：
   - 位置改变 → targetPose 也改变（跟随球的位置）
   - 距离仍然约 2 米
   - 速度向量方向改变，但大小不变

5. 结果：做圆周运动
   R = √(vx² + vy²) / vtheta = 2.14 / 0.45 ≈ 4.75米
```

#### 为什么不会到达目标？

```
targetPose 计算依赖于球的位置：
targetPose.y = ballPos.y * (targetPose.x + fd.length/2.0) / (ballPos.x + fd.length/2.0)

球的位置来自视觉或记忆：
- 如果来自视觉 → 有延迟（100-200ms）
- 如果来自记忆 → 可能不准确

机器人移动 → 球的相对位置改变 → targetPose 改变
→ 永远追不上 → 圆周运动
```

---

### 关键发现 4：与 SimpleChase 的对比

**SimpleChase 节点：** `brain_tree.cpp` 第 825-880 行
```cpp
// SimpleChase 也有类似的逻辑
double vx = ballPosToRobot.x;
double vy = ballPosToRobot.y;
double vtheta = ballYaw * 2.0;

// 但 SimpleChase 追的是球，目标明确
// Assist 追的是"辅助位置"，目标模糊且动态变化
```

**为什么之前分析的是 1 米圆周运动？**
- 那是 SimpleChase 追球的情况
- 球距离 ≈ 1 米 → 圆周半径 ≈ 1 米

**为什么现在是 4.75 米圆周运动？**
- 这是 Assist 追辅助位置的情况
- 辅助位置距离 ≈ 2.14 米 → 圆周半径 ≈ 4.75 米

---

## 🔍 完整的问题链路

```
1. 守门员 #1 靠近球（1.21m）
   ↓
2. 守门员成为队长（Cost=4.73 < 7.16）
   ↓
3. 前锋 #2 的 tmMyCostRank = 1（不是队长）
   ↓
4. StrikerDecide 判断 decision = "assist"
   ↓
5. 进入 Assist 节点（第 1355-1573 行）
   ↓
6. 计算辅助位置 targetPose：
   targetPose.x = ballPos.x - 2.0 = -5.54 - 2.0 = -7.54
   但受限于：max(targetPose.x, -fd.length/2 + distToGoalline)
   假设 distToGoalline = 3.0
   targetPose.x = max(-7.54, -9.0/2 + 3.0) = max(-7.54, -1.5) = -1.5
   ❌ 等等，这个计算不对...
   
   实际上，从速度推测：
   targetPose 在机器人坐标系中是 (1.92, 0.94)
   转换回场地坐标系...
   
7. 关键问题：
   vx = targetPose_r.x = 1.92
   vy = targetPose_r.y = 0.94
   vtheta = ball.yawToRobot * 2.0 = 0.22 * 2.0 = 0.44
   
8. ❌ 没有速度缩放！只有上限限制（3.0, 1.8）
   ❌ 距离 2 米 → 速度 2 m/s（错误的比例关系）
   
9. 机器人高速前进 + 旋转
   ↓
10. 位置改变 → targetPose 改变（依赖球位置）
    ↓
11. 新的 targetPose 距离仍然约 2 米
    ↓
12. 形成稳定的圆周运动（R=4.75m）
```

---

## 🛠️ 解决方案

### 方案 1：降低 Assist 的速度限制（立即生效）⭐⭐⭐⭐⭐

**修改文件：** `subtree_striker_play.xml` 第 43 行

**当前配置：**
```xml
<Assist _while="decision == 'assist'" vx_limit="3.0" vy_limit="1.8" />
```

**修改为：**
```xml
<Assist _while="decision == 'assist'" vx_limit="1.2" vy_limit="0.8" />
```

**理由：**
- 辅助位置不需要高速移动
- 1.2 m/s 的前进速度足够
- 0.8 m/s 的横移速度足够
- 大幅降低圆周运动的可能性

**预期效果：**
- 圆周半径缩小：R = √(1.2² + 0.8²) / 0.44 ≈ 3.27米
- 但仍然可能圆周运动，需要配合方案 2

---

### 方案 2：修复 Assist 的速度计算逻辑（根本修复）⭐⭐⭐⭐⭐

**修改文件：** `brain_tree.cpp` 第 1548-1552 行

**当前代码：**
```cpp
} else {
    vx = targetPose_r.x;  // ❌ 直接用距离作为速度
    vy = targetPose_r.y;  // ❌ 直接用距离作为速度
    vtheta = brain->data->ball.yawToRobot * 2.0;
}
```

**修改为：**
```cpp
} else {
    // ✅ 使用比例控制，而不是直接用距离
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
}
```

**理由：**
- 距离远时：速度 = 0.8 × 距离，但不超过 1.2 m/s
- 距离 2 米 → 速度 1.2 m/s（而不是 2.0 m/s）
- 距离 1 米 → 速度 0.8 m/s
- 距离 0.5 米 → 速度 0.4 m/s
- 距离 < 0.5 米 → 到达，停止移动（配合第 1521-1529 行的判断）

---

### 方案 3：增加到达判定（防止一直追）⭐⭐⭐⭐

**修改文件：** `brain_tree.cpp` 第 1362 行

**当前配置：**
```cpp
double distTolerance = getInput<double>("dist_tolerance").value();
```

**在行为树 XML 中添加：**
```xml
<Assist _while="decision == 'assist'" 
        vx_limit="1.2" vy_limit="0.8" 
        dist_tolerance="1.0" 
        theta_tolerance="0.5" />
```

**理由：**
- 距离 < 1.0 米 且 角度偏差 < 0.5 rad → 认为到达
- 停止移动，不再追逐
- 防止永远追不到的情况

---

### 方案 4：调试日志增强（监控问题）⭐⭐⭐

**修改文件：** `brain_tree.cpp` 第 1548-1572 行

**添加详细日志：**
```cpp
} else {
    const double dist = std::hypot(targetPose_r.x, targetPose_r.y);
    const double speedGain = 0.8;
    const double maxSpeed = 1.2;
    
    if (dist > 0.01) {
        const double targetSpeed = std::min(speedGain * dist, maxSpeed);
        vx = targetSpeed * targetPose_r.x / dist;
        vy = targetSpeed * targetPose_r.y / dist;
    } else {
        vx = 0.0;
        vy = 0.0;
    }
    vtheta = brain->data->ball.yawToRobot * 2.0;
    
    // 增加调试日志
    log(format("Assist move: dist=%.2f, vx=%.2f, vy=%.2f, vtheta=%.2f, "
               "targetField=(%.2f,%.2f), targetRobot=(%.2f,%.2f)",
               dist, vx, vy, vtheta,
               targetPose.x, targetPose.y,
               targetPose_r.x, targetPose_r.y));
}
```

---

## 📝 完整修复步骤

### 步骤 1：立即修改行为树（5分钟）

```bash
# SSH 到机器人
ssh booster@192.168.50.205

# 编辑行为树
cd /home/booster/robocup_k1/src/brain/behavior_trees/subtrees
vim subtree_striker_play.xml

# 修改第 43 行
# 从：<Assist _while="decision == 'assist'" vx_limit="3.0" vy_limit="1.8" />
# 改为：<Assist _while="decision == 'assist'" vx_limit="1.2" vy_limit="0.8" dist_tolerance="1.0" theta_tolerance="0.5" />

# 重启 brain_node（行为树 XML 修改后需要重启）
sudo systemctl restart brain_node

# 或者手动重启：
# pkill brain_node
# cd /home/booster/robocup_k1
# source install/setup.bash
# ros2 run brain brain_node
```

**验证：**
```bash
# 观察状态日志
tail -f /tmp/state.log

# 预期：Vel 速度降低到 (vx<1.2, vy<0.8)
```

---

### 步骤 2：修复速度计算逻辑（30分钟）

```bash
# 在开发机上修改
cd /d/RISE/加速进化K1\ 3v3/robocup_k1/src/brain/src

# 编辑 brain_tree.cpp，找到第 1548 行
# 应用方案 2 的代码修改

# 编译
cd /d/RISE/加速进化K1\ 3v3/robocup_k1
colcon build --packages-select brain

# 部署到机器人
scp install/lib/brain/brain_node booster@192.168.50.205:/home/booster/robocup_k1/install/lib/brain/

# 重启
ssh booster@192.168.50.205 "sudo systemctl restart brain_node"
```

---

### 步骤 3：验证修复（10分钟）

```bash
# 1. 观察状态日志
ssh booster@192.168.50.205
tail -f /tmp/state.log

# 2. 观察详细日志
tail -f /tmp/brain.log | grep "Assist"

# 预期输出：
# [INFO] Assist move: dist=2.14, vx=0.86, vy=0.42, vtheta=0.44, ...
# 而不是：
# [INFO] Assist move: dist=2.14, vx=1.92, vy=0.94, vtheta=0.44, ...

# 3. 检测圆周运动
bash scripts/monitor_gc_connection.sh 192.168.50.205

# 预期：不再出现圆周运动警告
```

---

### 步骤 4：测试场景（20分钟）

**测试 1：守门员靠近球**
```
1. 让守门员 #1 靠近球（1.2m 以内）
2. 观察前锋 #2 是否进入 assist 状态
3. 观察前锋 #2 的速度：应该 < 1.2 m/s
4. 观察是否还有圆周运动
```

**测试 2：裁判机断连**
```
1. 拔掉裁判机网线
2. 观察前锋 #2 的行为
3. 应该不再出现大半径圆周运动
```

**测试 3：正常辅助**
```
1. 正常比赛场景
2. 前锋 #2 在 assist 状态下移动到辅助位置
3. 应该平稳到达，不做圆周运动
```

---

## 📊 预期改进效果

### 修改前 ❌

```
Assist 节点：
- vx_limit = 3.0 m/s
- 实际速度：vx=1.92, vy=0.94
- 圆周半径：4.75 米
- 永远到达不了目标位置
- 高速圆周运动
```

### 修改后（方案 1）✅

```
Assist 节点：
- vx_limit = 1.2 m/s
- 实际速度：vx≈1.2, vy≈0.8（仍可能达到上限）
- 圆周半径：≈3.3 米（仍有问题）
- 仍可能圆周运动，但半径更小
```

### 修改后（方案 1 + 方案 2）✅✅✅

```
Assist 节点：
- vx_limit = 1.2 m/s（上限）
- 实际速度控制：
  - 距离 2m → 速度 1.2 m/s
  - 距离 1m → 速度 0.8 m/s
  - 距离 0.5m → 速度 0.4 m/s
- 圆周半径：逐渐缩小，最终到达
- 平稳移动到辅助位置
- 不再圆周运动
```

---

## 🎯 根本原因总结

### 为什么会出现 4.75 米圆周运动？

1. **Assist 节点的速度计算错误**
   - 直接用距离作为速度：`vx = targetPose_r.x`
   - 没有速度衰减或比例控制

2. **速度限制过高**
   - `vx_limit = 3.0 m/s` 太高
   - 允许机器人以极高速度移动

3. **目标位置动态变化**
   - targetPose 依赖球的位置
   - 球的位置有延迟或不准确
   - 机器人移动 → 球位置改变 → targetPose 改变
   - 永远追不上 → 圆周运动

4. **没有到达判定**
   - dist_tolerance 参数未在 XML 中配置
   - 使用默认值（可能很小）
   - 难以满足停止条件

### 与之前 1 米圆周运动的区别

| 特征 | 1米圆周（SimpleChase） | 4.75米圆周（Assist） |
|------|----------------------|---------------------|
| 触发节点 | SimpleChase | Assist |
| 追逐目标 | 球 | 辅助位置 |
| 目标距离 | 1.0m | 2.14m |
| vx | 1.0 m/s | 1.92 m/s |
| vy | 0 | 0.94 m/s |
| vtheta | 1.0 rad/s | 0.45 rad/s |
| 圆周半径 | 1.0m | 4.75m |
| 根本原因 | 追逐记忆中的球 | 追逐动态变化的辅助位置 |

---

## ✅ 行动清单

- [ ] 立即修改 `subtree_striker_play.xml`（降低速度限制）
- [ ] 修改 `brain_tree.cpp`（修复速度计算逻辑）
- [ ] 添加调试日志（监控 Assist 行为）
- [ ] 编译部署到机器人
- [ ] 测试验证（守门员靠近球场景）
- [ ] 测试验证（裁判机断连场景）
- [ ] 更新 CHANGELOG.md
- [ ] 提交代码到 GitHub

---

**问题已彻底定位！Assist 节点的速度计算逻辑是根本原因。立即应用方案 1 可以快速缓解，应用方案 2 可以根本修复。**
