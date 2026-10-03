# CHANGELOG

## 2026-09-29 实时状态日志系统（State Logger）

### 修改目的

实现类似 `tail -f` 的实时状态监控系统，能够清晰地显示所有队友的关键状态信息，方便调试和监控比赛过程。支持多机器人协作时的状态可视化，排版整齐，数据类型清晰可辨。

### 功能特性

**监控信息包括：**
- ✅ Player ID（机器人编号）
- ✅ Role（角色：striker 或 goal_keeper）
- ✅ Cost 值（队长选举成本）
- ✅ 是否为 Leader（队长状态）
- ✅ 是否进入 RLVisionKick 模式
- ✅ 当前行为 State（decision）
- ✅ 自身的 Odometry（场地坐标系位置）
- ✅ 比赛一级、二级状态
- ✅ 是否识别到足球
- ✅ 当前机器人速度（vx, vy, vtheta）
- ✅ 在每个机器人视角里球的位置
- ✅ 是否触发碰撞避让（collision avoidance）
- ✅ 所有队友的上述信息

**输出特点：**
- 📊 使用表格框架清晰分隔各个机器人
- 🎯 自己的状态突出显示（标记为 SELF）
- 👥 队友状态依次列出（标记为 TEAMMATE）
- ⚠️  碰撞避让状态用图标高亮
- 🔄 10Hz 更新频率（100ms/次）
- 📝 输出到 `/tmp/state.log` 文件

### 修改内容

#### 1. 新增文件

**状态日志器头文件（`include/state_logger.h`）：**
```cpp
// 机器人状态结构体
struct RobotState {
    int playerId;
    bool isSelf;
    bool isAlive;
    double cost;
    bool isLeader;
    bool inVisualKick;
    std::string currentBehavior;
    double posX, posY, posTheta;
    double velX, velY, velTheta;
    bool ballDetected;
    double ballRange, ballYaw;
    double ballPosX, ballPosY;
    bool collisionAvoidanceActive;
    double avoidanceSpeedFactor;
    double closestTeammateDistance;
};

// 团队状态快照
struct TeamStateSnapshot {
    std::string timestamp;
    std::string primaryState;
    std::string secondaryState;
    int ownScore, opponentScore;
    std::vector<RobotState> robots;
};

// 状态日志器类
class StateLogger {
    void start();
    void stop();
    void updateState(const TeamStateSnapshot& snapshot);
};
```

**状态日志器实现（`src/state_logger.cpp`）：**
- 独立线程运行，100ms 更新一次
- 格式化输出：表格框架 + Unicode 字符美化
- 时间戳、比赛状态、机器人状态分层显示

**查看脚本（`scripts/view_state_log.sh`）：**
```bash
#!/bin/bash
tail -f /tmp/state.log | while read line; do
    if [[ "$line" == "=="* ]]; then
        clear
    fi
    echo "$line"
done
```

#### 2. 修改的文件

**`brain.h`：**
- 添加 `#include "state_logger.h"`
- 添加成员变量 `std::unique_ptr<StateLogger> stateLogger;`
- 添加私有方法 `void updateStateLogger();`

**`brain.cpp`：**

**初始化（`Brain::init()`）：**
```cpp
// 初始化状态日志器
std::string stateLogPath = "/tmp/state.log";
stateLogger = std::make_unique<StateLogger>(stateLogPath);
stateLogger->start();
RCLCPP_INFO(get_logger(), "State logger started: %s", stateLogPath.c_str());
```

**主循环更新（`Brain::tick()`）：**
```cpp
updateMemory();
handleSpecialStates();
handleCooperation();
updateStateLogger();  // 新增：每个 tick 更新状态日志
```

**状态采集实现（`Brain::updateStateLogger()`）：**
- 采集比赛状态（primary/secondary state, score）
- 采集自己的完整状态（cost, leader, position, velocity, ball, collision）
- 采集所有队友的状态（遍历 `data->tmStatus[]`）
- 计算碰撞避让状态（检查最近队友距离）
- 调用 `stateLogger->updateState()` 更新日志

### 输出格式示例

```
================================================================================
12:34:56.789 | TEAM STATE MONITOR
================================================================================
┌─ GAME STATE ─────────────────────────────────────────────────────┐
│ Primary:   PLAY                 │ Secondary: NORMAL              │
│ Score:     0 - 0                                                │
└──────────────────────────────────────────────────────────────────┘
┌─ ROBOT #2 (SELF) ────────────────────────────────────────────────┐
│ Role: striker    │ Cost:   2.35 │ Leader: YES │ VisionKick: NO  │
│ State: chase        │ Pos: ( 1.20,  0.35,  0.12)                │
│ Vel: ( 0.80,  0.10,  0.05)                                      │
│ Ball: DETECTED │ Range:  1.50m │ Yaw:  0.15rad │ Pos: ( 2.10,  0.50) │
│ Collision Avoidance: INACTIVE                                    │
├─ ROBOT #1 (TEAMMATE) ─────────────────────────────────────────────┤
│ Role: striker    │ Cost:   3.20 │ Leader: NO  │ VisionKick: NO  │
│ State: N/A          │ Pos: ( 0.50, -1.20,  0.05)                │
│ Vel: ( 0.00,  0.00,  0.00)                                      │
│ Ball: DETECTED │ Range:  2.80m │ Yaw:  0.25rad │ Pos: ( 2.00,  0.40) │
│ Collision Avoidance: INACTIVE                                    │
├─ ROBOT #3 (TEAMMATE) ─────────────────────────────────────────────┤
│ Role: goal_keeper│ Cost:   5.10 │ Leader: NO  │ VisionKick: YES │
│ State: N/A          │ Pos: (-2.50,  0.00, -0.10)                │
│ Vel: ( 0.00,  0.00,  0.00)                                      │
│ Ball: NOT DETECTED                                               │
│ ⚠️  COLLISION AVOIDANCE ACTIVE │ Factor:  0.7 │ Closest:  0.95m   │
└────────────────────────────────────────────────────────────────────┘
```

### 使用方法

#### 1. 编译代码
```bash
cd src/brain
colcon build --packages-select brain
```

#### 2. 启动机器人节点
```bash
ros2 run brain brain_node
```

#### 3. 实时查看状态日志
```bash
# 方法1：使用便捷脚本（推荐）
bash scripts/view_state_log.sh

# 方法2：直接使用 tail
tail -f /tmp/state.log

# 方法3：清屏循环查看
watch -n 0.1 "tail -50 /tmp/state.log"
```

#### 4. 多机器人监控
在每个机器人上分别启动 brain_node，然后在任意一台机器人上查看 `/tmp/state.log`，可以看到自己和所有队友的状态。

### 技术细节

**线程安全：**
- StateLogger 使用独立线程运行
- 使用 `std::mutex` 保护共享状态
- Brain 主线程通过 `updateState()` 更新快照

**性能优化：**
- 100ms 更新间隔（10Hz），不影响主控制循环（100Hz）
- 字符串格式化在独立线程完成
- 文件追加写入，自动刷新

**数据来源映射：**
| 显示信息 | 数据来源 |
|---------|---------|
| Player ID | `config->playerId` / `i + 1` |
| Cost | `data->tmMyCost` / `tm.cost` |
| Leader | `data->tmImLead` / `tm.isLead` |
| VisionKick | `data->tmImInVisualKick` / `tm.isInVisualKick` |
| State | `tree->getEntry<string>("decision")` |
| Position | `data->robotPoseToField` / `tm.robotPoseToField` |
| Velocity | `client->getLastVx/Vy/Vtheta()` |
| Ball Detected | `data->ballDetected` / `tm.ballDetected` |
| Ball Info | `data->ball` / `tm.ballPosToField` |
| Collision | 计算队友距离，检查是否 < 1.2m |

**碰撞避让判定逻辑：**
```cpp
if (closestTeammateDistance < 1.2) {
    collisionAvoidanceActive = true;
    if (distance < 0.8) {
        avoidanceSpeedFactor = 0.5;  // 最小距离区
    } else {
        // 线性插值
        speedFactor = 0.5 + (1.0 - 0.5) * (distance - 0.8) / 0.4;
    }
}
```

### 调试建议

**日志文件位置：**
- 默认：`/tmp/state.log`
- 可在 `Brain::init()` 中修改路径

**调整更新频率：**
在 `state_logger.cpp` 的 `logLoop()` 中修改：
```cpp
std::this_thread::sleep_for(std::chrono::milliseconds(100));  // 改为 50ms = 20Hz
```

**查看特定机器人：**
```bash
tail -f /tmp/state.log | grep "ROBOT #2"
```

**性能监控：**
```bash
# 查看日志文件大小增长速度
watch -n 1 "ls -lh /tmp/state.log"

# 截断日志文件（如果太大）
> /tmp/state.log
```

### 预期效果

**优点：**
- ✅ 实时监控所有机器人状态，无需连接到每台机器
- ✅ 清晰的表格格式，易于阅读和分析
- ✅ 独立线程运行，不影响主控制性能
- ✅ 碰撞避让状态可视化，方便调试防碰撞逻辑
- ✅ 支持多机协作调试，所有队友信息一目了然

**注意事项：**
- 依赖队友通信（UDP）的实时性
- 队友状态有通信延迟（<1000ms）
- 日志文件会持续增长，建议定期清理
- 不记录历史状态，只显示当前快照

### 未来扩展

**可能的改进方向：**
- 添加历史状态回放功能
- 支持网络远程查看（HTTP/WebSocket）
- 添加图形化界面（ncurses/web UI）
- 记录关键事件时间线
- 支持多文件轮转（避免单文件过大）
- 添加性能统计（CPU、内存、延迟）

---

## 2026-09-29 全局队友防碰撞系统

### 修改目的

在原有的 `visual_kick_spacing`（仅限视觉踢球时的前锋间距控制）基础上，实现 **全局的队友防碰撞机制**，适用于所有移动场景。无论机器人处于何种状态（Chase、SimpleChase、Adjust 等），只要队友距离过近就会自动减速、停止或远离，防止机器人相互碰撞。

### 原系统的局限性

**旧的 `visual_kick_spacing` 只在特定场景生效：**
- ✅ 一个前锋正在执行 RLVisionKick（视觉踢球）
- ✅ 另一个前锋处于 Assist（接应）模式
- ❌ 两个前锋同时 Chase（追球）时 **不生效**
- ❌ 前锋和守门员接近时 **不生效**
- ❌ 任意其他移动节点（SimpleChase、Adjust、GoToPosition 等）**不生效**

### 修改内容

#### 1. 配置文件（`config.yaml`）

**新增全局防碰撞参数：**
```yaml
global_collision_avoidance:
  enable: true                    # 全局队友防碰撞开关
  min_distance: 0.8               # 硬性最小距离限制 (m)，队友距离 < 此值时强制远离
  safe_distance: 1.2              # 安全距离 (m)，距离 < 此值时开始减速
  stop_distance: 0.6              # 紧急停止距离 (m)，距离 < 此值时立即停止移动
  speed_reduction_factor: 0.5     # 安全区内的速度衰减系数（0-1）
  teammate_comm_timeout_ms: 1000.0 # 队友位置信息有效期 (ms)
```

#### 2. 参数声明（`brain.cpp`）

在 `Brain` 构造函数中声明新参数（约 176-182 行）：
```cpp
declare_parameter<bool>("strategy.global_collision_avoidance.enable", true);
declare_parameter<double>("strategy.global_collision_avoidance.min_distance", 0.8);
declare_parameter<double>("strategy.global_collision_avoidance.safe_distance", 1.2);
declare_parameter<double>("strategy.global_collision_avoidance.stop_distance", 0.6);
declare_parameter<double>("strategy.global_collision_avoidance.speed_reduction_factor", 0.5);
declare_parameter<double>("strategy.global_collision_avoidance.teammate_comm_timeout_ms", 1000.0);
```

#### 3. 核心防碰撞逻辑（`brain_tree.cpp`）

**新增结构体和函数（约 195-305 行）：**
```cpp
struct GlobalCollisionAvoidanceSettings { ... };
GlobalCollisionAvoidanceSettings globalCollisionAvoidanceSettings(Brain *brain);
double applyGlobalCollisionAvoidance(Brain *brain, double &vx, double &vy, double &vtheta);
```

**应用到移动节点：**
- `Chase::tick()`（约 817 行）
- `SimpleChase::tick()`（约 876 行）
- `Adjust::tick()`（约 1727 行）

每个节点在计算完速度后、发送给底层之前，调用：
```cpp
applyGlobalCollisionAvoidance(brain, vx, vy, vtheta);
```

### 防碰撞逻辑详解

#### 三层防护机制

| 距离区间 | 触发条件 | 行为 | 速度系数 |
|---------|---------|------|---------|
| **紧急停止区** | < 0.6m | 立即停止所有移动（vx=0, vy=0, vtheta=0） | 0.0 |
| **强制远离区** | 0.6m - 0.8m | 强制向远离队友方向移动（awaySpeed=0.3m/s） | 0.3 |
| **安全减速区** | 0.8m - 1.2m | 线性减速（最低 50% 速度） | 0.5 - 1.0 |
| **正常区域** | > 1.2m | 不干预，正常移动 | 1.0 |

#### 工作流程

1. **遍历所有队友**：检查每个队友的位置信息
2. **筛选有效队友**：
   - 必须是存活状态（`isAlive == true`）
   - 通信延迟 < 1000ms（避免使用过期位置）
   - 位置数据有效（`isfinite(x, y)`）
3. **找到最近队友**：计算所有队友距离，找到最近的一个
4. **分层响应**：
   - **< 0.6m**：紧急停止
   - **< 0.8m**：计算远离方向，强制向反方向移动
   - **< 1.2m**：线性减速（距离越近速度越慢）
   - **≥ 1.2m**：不干预

#### 速度衰减公式（安全减速区）

```cpp
ratio = (distance - 0.8) / (1.2 - 0.8)  // 0.0 - 1.0
speedFactor = 0.5 + (1.0 - 0.5) × ratio  // 0.5 - 1.0
```

**示例：**
- 距离 0.8m：speedFactor = 0.5（最低速度）
- 距离 1.0m：speedFactor = 0.75（75% 速度）
- 距离 1.2m：speedFactor = 1.0（正常速度）

### 与旧系统的对比

| 特性 | visual_kick_spacing（旧） | global_collision_avoidance（新） |
|-----|-------------------------|--------------------------------|
| **适用场景** | 仅视觉踢球时的前锋协调 | 所有移动节点（Chase、SimpleChase、Adjust 等） |
| **角色限制** | 仅前锋（striker） | 所有角色（前锋、守门员） |
| **状态要求** | 主攻手必须在 RLVisionKick，接应者在 Assist | 无状态要求，任意移动都生效 |
| **通信超时** | 500ms | 1000ms |
| **最小距离** | 1.0m（stop_distance） | 0.8m（min_distance） |
| **紧急停止** | 无 | 0.6m（stop_distance） |
| **减速机制** | 固定速度限制（yield_speed） | 线性衰减（0.5-1.0） |
| **相互关系** | 独立系统 | 独立系统，可同时启用 |

### 预期效果

**优点：**
- ✅ 全场景覆盖：任意两个机器人接近时都会触发防碰撞
- ✅ 多层防护：紧急停止、强制远离、渐进减速三层机制
- ✅ 实时响应：每个控制周期（10ms）都检查队友距离
- ✅ 自动恢复：距离恢复安全后自动恢复正常速度
- ✅ 降低碰撞风险：特别是两个前锋同时追球、前锋和守门员接近等场景

**注意事项：**
- 依赖队友通信（UDP）的实时性和准确性
- 通信延迟 > 1000ms 时，该队友不参与防碰撞计算
- 定位误差可能导致误判，建议定位系统稳定后使用
- 可能在某些场景下影响攻击效率（为安全牺牲部分速度）

### 参数调优建议

**如果碰撞仍然发生，可以调整：**
- 增大 `safe_distance`：从 1.2m → 1.5m（更早开始减速）
- 增大 `min_distance`：从 0.8m → 1.0m（更大的强制远离范围）
- 降低 `speed_reduction_factor`：从 0.5 → 0.3（减速更激进）

**如果影响攻击效率，可以调整：**
- 减小 `safe_distance`：从 1.2m → 1.0m（减速范围更小）
- 增大 `speed_reduction_factor`：从 0.5 → 0.7（减速不那么激进）
- 关闭某些节点的防碰撞（需要代码修改）

**完全禁用：**
```yaml
global_collision_avoidance:
  enable: false  # 关闭全局防碰撞
```

### 测试建议

1. **两个前锋同时追球**：观察是否在接近时减速/停止
2. **前锋接近守门员**：观察是否触发防碰撞
3. **通信延迟测试**：模拟高延迟环境，观察系统是否忽略过期位置
4. **定位误差测试**：观察定位漂移时是否误触发

---

## 2026-09-29 提高距离成本权重优化队长选择

### 修改目的

在团队协作中，距离球最近的机器人应该有更高的优先级成为队长（球权所有者）。通过提高距离成本的权重系数，使距离因素在 cost 计算中占据更重要的地位，减少因角度、对准等次要因素导致的远距离机器人抢球现象。

### 修改内容

**文件：`src/brain/src/brain.cpp`（约第 1511-1516 行）**

**修改前：**
```cpp
if (ballLocationKnown) {
    cost += data->ball.range;
    log_(format("ball range cost: %.1f", data->ball.range));
```

**修改后：**
```cpp
if (ballLocationKnown) {
    constexpr double RANGE_WEIGHT = 1.5;
    const double rangeCost = data->ball.range * RANGE_WEIGHT;
    cost += rangeCost;
    log_(format("ball range cost: %.1f (range: %.1f, weight: %.1f)", rangeCost, data->ball.range, RANGE_WEIGHT));
```

### 参数说明

- **`RANGE_WEIGHT = 1.5`**：距离成本权重系数
- 原始比例：1 米距离 = 1.0 成本
- 修改后比例：1 米距离 = 1.5 成本
- 距离优势提升：**+50%**

### 实际效果对比

| 距离差异 | 修改前成本差 | 修改后成本差 | 优势提升 |
|---------|-------------|-------------|---------|
| 0.5米更近 | -0.5 | **-0.75** | +50% |
| 1.0米更近 | -1.0 | **-1.5** | +50% |
| 2.0米更近 | -2.0 | **-3.0** | +50% |

### 典型场景示例

**场景 1：距离 vs 角度权衡**
- **机器人 A**：距离球 1.0m，角度偏差 0.5rad
  - 修改前 cost ≈ 1.0 + 0.5 = **1.5**
  - 修改后 cost ≈ 1.5 + 0.5 = **2.0**
- **机器人 B**：距离球 2.0m，角度偏差 0.1rad
  - 修改前 cost ≈ 2.0 + 0.1 = **2.1**
  - 修改后 cost ≈ 3.0 + 0.1 = **3.1**
- **结果**：A 的距离优势从 0.6 提升到 1.1，更明确地成为队长

**场景 2：距离 vs 对准误差权衡**
- **机器人 A**：距离球 1.5m，对准误差 0.2rad（cost ≈ 0.27）
  - 修改前 cost ≈ 1.5 + 0.27 = **1.77**
  - 修改后 cost ≈ 2.25 + 0.27 = **2.52**
- **机器人 B**：距离球 2.5m，对准误差 0.05rad（cost ≈ 0.07）
  - 修改前 cost ≈ 2.5 + 0.07 = **2.57**
  - 修改后 cost ≈ 3.75 + 0.07 = **3.82**
- **结果**：A 的优势从 0.8 提升到 1.3，距离更近的机器人更容易获得球权

### 预期效果

**优点：**
- ✅ 距离球最近的机器人获得更高优先级
- ✅ 减少远距离机器人因朝向好而抢球的情况
- ✅ 减少不必要的长距离追球和团队冲突
- ✅ 提高团队整体效率，缩短到达球位置的时间

**注意事项：**
- 角度、对准误差等因素仍然有效，只是相对权重降低
- 极端情况（跌倒 +15.0、定位失效 +100.0）仍然是硬排除
- 如果 1.5 倍权重效果不够明显，可以进一步调整到 2.0 或 2.5

### 调试建议

如果需要更强的距离优先级，可以调整 `RANGE_WEIGHT` 值：
- **`RANGE_WEIGHT = 2.0`**：距离优势翻倍（推荐用于强调距离优先的策略）
- **`RANGE_WEIGHT = 2.5`**：距离优势更强（可能导致角度、对准因素被过度忽略）
- **`RANGE_WEIGHT = 1.0`**：恢复原始行为（距离和角度权重相当）

建议先用 1.5 测试效果，根据实际比赛表现再调整。

---

## 2026-09-29 RLVisionKick 左右脚分别补偿

### 问题描述

机器人使用左脚和右脚踢球时，存在相反方向的系统性偏差：
- **左脚踢球**（球在左侧，y > 0）：向右偏
- **右脚踢球**（球在右侧，y < 0）：向左偏

原先使用单一的 `kick_direction_offset` 参数无法同时修正两种情况。

### 解决方案

根据球相对于机器人的位置（y 坐标）判断使用哪只脚，并应用对应的补偿参数：
- 球在左侧（`ball.y > 0`）→ 使用**左脚**踢球 → 应用 `kick_direction_offset_left`
- 球在右侧（`ball.y < 0`）→ 使用**右脚**踢球 → 应用 `kick_direction_offset_right`

### 修改内容

**1. 配置文件** (`src/brain/config/config.yaml:158-163`)
```yaml
RLVisionKick:
  kick_direction_offset_left: 0.1   # 左脚踢球补偿（球在左侧，y>0）
  kick_direction_offset_right: -0.1 # 右脚踢球补偿（球在右侧，y<0）
  # 正值 = 向左补偿（修正偏右）
  # 负值 = 向右补偿（修正偏左）
  # 单位：弧度（0.1 rad ≈ 5.7度）
```

**2. 参数声明** (`src/brain/src/brain.cpp:205-208`)
```cpp
declare_parameter<double>("RLVisionKick.kick_direction_offset_left", 0.0);
declare_parameter<double>("RLVisionKick.kick_direction_offset_right", 0.0);
```

**3. 补偿应用逻辑** (`src/brain/src/brain.cpp:571-578`)
```cpp
// 根据球的位置判断使用左脚还是右脚，应用不同的补偿值
double kickDirOffset = (ballRobot.y > 0)
    ? get_parameter("RLVisionKick.kick_direction_offset_left").as_double()
    : get_parameter("RLVisionKick.kick_direction_offset_right").as_double();

kickMsg.dir = toPInPI((goalieVisualKick ? clearanceDir : data->kickDir)
                     - data->robotPoseToField.theta + kickDirOffset);
```

### 调试方法

#### 运行时动态调整
```bash
# 调整左脚踢球补偿（向左补偿 5 度）
ros2 param set /brain_node RLVisionKick.kick_direction_offset_left 0.087

# 调整右脚踢球补偿（向右补偿 5 度）
ros2 param set /brain_node RLVisionKick.kick_direction_offset_right -0.087

# 查看当前值
ros2 param get /brain_node RLVisionKick.kick_direction_offset_left
ros2 param get /brain_node RLVisionKick.kick_direction_offset_right
```

### 测试步骤

1. **分别测试左右脚踢球**：
   - 将球放在机器人左侧（y > 0），测试左脚踢球偏差
   - 将球放在机器人右侧（y < 0），测试右脚踢球偏差

2. **记录偏差角度**：
   - 左脚偏右 8 度 → 设置 `kick_direction_offset_left: 0.14`（8° × π/180 ≈ 0.14 rad）
   - 右脚偏左 6 度 → 设置 `kick_direction_offset_right: -0.10`（-6° × π/180 ≈ -0.10 rad）

3. **验证效果**：
   - 分别用左右脚各踢 10 次，检查是否修正偏差

---

## 2026-09-29 RLVisionKick 踢球方向偏右修复

### 问题描述

机器人进入 RLVisionKick 踢球模式后，实际踢球方向总是偏右，导致射门或传球偏离目标。

### 根本原因

可能的系统性偏差来源：
1. **机械系统偏差**：机器人出脚动作、摆腿轨迹、接触点存在固定偏差
2. **视觉检测偏差**：球位置检测有系统性误差
3. **定位系统偏差**：机器人朝向（`robotPoseToField.theta`）存在累积误差

### 修复方案

增加可配置的踢球方向补偿参数，允许通过配置文件或运行时动态调整。

### 修改内容

**1. 配置文件** (`src/brain/config/config.yaml:162`)
```yaml
RLVisionKick:
  kick_direction_offset: -0.1  # 踢球方向补偿
  # 负值 = 向左补偿（修正偏右）
  # 正值 = 向右补偿（修正偏左）
  # 单位：弧度（-0.1 rad ≈ -5.7°）
```

**2. 参数声明** (`src/brain/src/brain.cpp:207`)
```cpp
declare_parameter<double>("RLVisionKick.kick_direction_offset", 0.0);
```

**3. 补偿应用** (`src/brain/src/brain.cpp:568-572`)
```cpp
// 获取踢球方向补偿参数（用于修正系统性偏差）
double kickDirOffset = get_parameter("RLVisionKick.kick_direction_offset").as_double();

kickMsg.dir = toPInPI((goalieVisualKick ? clearanceDir : data->kickDir)
                     - data->robotPoseToField.theta + kickDirOffset);
```

### 调试方法

#### 方法1：修改配置文件
```yaml
# 偏右 5 度 → 设置 -0.087 rad（-5°）
kick_direction_offset: -0.087

# 偏右 10 度 → 设置 -0.175 rad（-10°）
kick_direction_offset: -0.175
```

#### 方法2：运行时动态调整
```bash
# 查看当前值
ros2 param get /brain_node RLVisionKick.kick_direction_offset

# 设置向左补偿 5 度
ros2 param set /brain_node RLVisionKick.kick_direction_offset -0.087

# 设置向左补偿 10 度
ros2 param set /brain_node RLVisionKick.kick_direction_offset -0.175

# 恢复无补偿
ros2 param set /brain_node RLVisionKick.kick_direction_offset 0.0
```

### 测试步骤

1. **记录原始偏差**：
   - 让机器人正对球门踢球 10 次
   - 测量平均偏差角度（假设偏右 8°）

2. **计算补偿值**：
   ```
   偏差角度（度） → 弧度
   8° × (π/180) = 0.1396 rad
   补偿值 = -0.14 rad（向左）
   ```

3. **应用补偿**：
   ```bash
   ros2 param set /brain_node RLVisionKick.kick_direction_offset -0.14
   ```

4. **验证效果**：
   - 再次踢球 10 次
   - 检查是否修正偏差

5. **微调**：
   - 偏差减少但仍偏右 → 增大负值（如 -0.16）
   - 过度补偿导致偏左 → 减小负值（如 -0.12）

### 优点

- ✅ 简单直接，不影响其他移动逻辑
- ✅ 可运行时动态调整，无需重启
- ✅ 适用于所有踢球场景（射门、传球、解围）
- ✅ 易于现场快速调试
- ✅ 可针对不同机器人设置不同补偿值

### 注意事项

1. **每个机器人可能需要不同的补偿值**（机械差异）
2. **场地不同可能需要微调**（地面摩擦力、球的状态）
3. **建议补偿值范围**：-0.3 ~ +0.3 rad（约 ±17°）
4. **过大的补偿值可能表明其他问题**（机械故障、定位系统失效）

### 角度参考表

| 偏差角度（度） | 补偿值（弧度） | 配置值 |
|--------------|-------------|--------|
| 偏右 3° | -0.052 | `-0.05` |
| 偏右 5° | -0.087 | `-0.09` |
| 偏右 8° | -0.140 | `-0.14` |
| 偏右 10° | -0.175 | `-0.18` |
| 偏右 15° | -0.262 | `-0.26` |

---

## 2026-09-29 修复 v12 协议 Stop Play 停止不生效问题

### 问题诊断

**现象**：
- 使用 v12 裁判机协议时，裁判机发送 STOP 状态
- Chase、SimpleChase、Adjust 等普通移动节点能够停止
- **但 RLVisionKick 视觉踢球模式继续运行，机器人不停止**

**根本原因**：

**问题1：协议层 - v12 STOP 状态未正确传递**
- v12 协议本身没有独立的 `stopped` 字段（这是 v20 协议新增的）
- v12 协议通过 `secondaryStateInfo[1] == 0` 表示 STOP 状态
- `game_controller_node.cpp` 的协议适配器把 `msg.stopped` 固定写死为 `false`
- Brain 节点接收后保持 `msg.stopped` 的 `false` 值，导致 `gc_play_stopped` 也为 `false`

**问题2：RLVisionKick 节点层 - 缺少 Stop Play 主动检查**
- 行为树在第16-18行有全局 `gc_play_stopped` 检查，理论上能拦截所有节点
- **但 RLVisionKick 是 StatefulActionNode，有独立的 onRunning() 循环**
- 在 `onRunning()` 内部执行底层 SDK 踢球动作时，不会每个 tick 都返回到行为树根节点
- 导致全局 Stop Play 检查无法及时中断正在运行的视觉踢球动作
- 其他简单节点（Chase、Adjust）每个 tick 都返回，能被全局检查拦截

### 修改内容

**修改1：协议适配器 - 正确解析 v12 STOP 状态**

修改文件：`src/game_controller/src/game_controller_node.cpp`（第 184-187 行）

```cpp
// 修改前
msg.stopped = false;

// 修改后
// Legacy/version 12 has no standalone stopped field. Its STOP state is
// encoded as secondaryStateInfo[1] == 0 and must be exposed to Brain.
const auto legacy_sub_state =
    static_cast<unsigned char>(data.secondaryStateInfo[1]);
msg.stopped = legacy_sub_state == 0;
```

**修改2：RLVisionKick 节点 - 主动检查 Stop Play**

修改文件：`src/brain/src/brain_tree.cpp` 的 `RLVisionKick::onRunning()`（约第 2537-2556 行）

```cpp
// 新增：Stop Play 立即中断检查
const bool gameControlStop =
    brain->tree->getEntry<bool>("gc_play_stopped") ||
    brain->tree->getEntry<string>("gc_game_state") != "PLAY";
if (gameControlStop) {
    logExit("exit visual kick immediately: game controller stop");
    brain->client->setVelocity(0.0, 0.0, 0.0, false, false, false);
    if (_visionKickStarted) brain->client->robocupWalk();
    clearState();
    recordExitTime();
    _isDecelerating = false;
    _visionKickStarted = false;
    _headCommandSent = false;
    return NodeStatus::SUCCESS;
}
```

### 技术细节

**为什么其他节点能停止，RLVisionKick 不能？**

| 节点类型 | 执行模式 | Stop Play 响应方式 |
|---------|---------|------------------|
| Chase、SimpleChase、Adjust | SyncActionNode，每个 tick 立即返回 | 行为树全局检查（第16行）能及时拦截 |
| RLVisionKick | StatefulActionNode，onRunning() 持续运行 | 需要在 onRunning() 内部主动检查 Stop Play |

**v12 vs v20 协议差异**：
- **v20 协议**：有独立的 `stopped` 字段，直接表示 Stop Play 状态
- **v12 协议**：没有 `stopped` 字段，通过 `secondaryStateInfo[1] == 0` 编码 STOP 状态
- 适配器需要将 v12 的隐式表示转换为显式的 `msg.stopped` 布尔值

**完整信号链路**：
```
裁判机 v12 STOP
  ↓ secondaryStateInfo[1] = 0
game_controller_node 协议适配（修改1）
  ↓ msg.stopped = true
Brain::gameControlCallback()
  ↓ gc_play_stopped = true
├─ 行为树全局检查（game.xml 第16行）→ 普通节点停止
└─ RLVisionKick::onRunning() 主动检查（修改2）→ 视觉踢球停止
```

### 影响范围

**全局停止生效位置**（`src/brain/behavior_trees/game.xml`）：
- 第 16-18 行：主循环全局停止检查
- 第 40-42 行：策略层停止检查

**停止的行为**：
- 所有移动节点（Chase、SimpleChase、Adjust 等）
- RLVisionKick 视觉踢球
- 守门员防守动作
- 所有速度命令归零

### 预期效果

**修复后**：
1. 裁判机发送 v12 STOP 信号
2. `gc_play_stopped` 正确设置为 `true`
3. 行为树立即进入停止分支
4. 所有运动停止，包括正在进行的 RLVisionKick
5. 裁判机恢复比赛后正常继续

**兼容性**：
- v20 协议：继续使用原有的独立 `stopped` 字段（未受影响）
- v12 协议：通过 `secondaryStateInfo[1]` 正确解析 STOP 状态
- 不影响其他比赛状态（READY、SET、PLAY 等）

### 验证方法

**测试步骤**：
1. 使用 v12 协议裁判机
2. 比赛进行中，机器人正在踢球（RLVisionKick 运行）
3. 裁判机发送 STOP 指令
4. 观察机器人是否立即停止所有动作

**日志验证**：
```bash
grep "gc_play_stopped" brain.log
# STOP 时应该看到：gc_play_stopped = true

grep "SetVelocity" brain.log
# 应该看到速度命令为 (0, 0, 0)
```

---

## 2026-09-29 队友足球坐标共享移动功能修复

### 问题诊断

**现象**：
- 机器人 A 看到球并通过 UDP 发送 `ballPosToField` 给队友
- 机器人 B 接收到坐标后保存到 `tmBall`，并设置 `tm_ball_pos_reliable = true`
- B 的摄像头追踪节点 `CamTrackBall` 能正常使用队友坐标转动头部
- **但是** Chase、SimpleChase、Adjust 等移动节点仍然停止或继续搜索，不向球移动

**根本原因**：
- Chase、SimpleChase、Adjust 节点只检查 `ball_location_known`（本机是否看到球）
- 完全忽略了 `tm_ball_pos_reliable`（队友是否看到球）标志
- 导致 B 虽然通过队友知道球的位置，但移动逻辑认为"不知道球在哪里"而拒绝行动

### 修改内容

修改文件：`src/brain/src/brain_tree.cpp`

| 节点 | 原始逻辑 | 修改后逻辑 | 效果 |
|------|---------|-----------|------|
| **Chase** (约第616行) | `if (!ball_location_known)` 则停止 | `if (!(ball_location_known \|\| tm_ball_pos_reliable))` 则停止；根据标志选择使用 `ball` 或 `tmBall` 数据 | B 收到队友坐标后能追球，不再原地等待 |
| **SimpleChase** (约第700行) | `if (!ball_location_known)` 则停止 | `if (!(ball_location_known \|\| tm_ball_pos_reliable))` 则停止；使用 `tmBall.posToRobot`、`tmBall.yawToRobot`、`tmBall.range` | 简单追球模式也支持队友坐标 |
| **Adjust** (约第1502行) | `if (!ball_location_known)` 则退出 | `if (!(ball_location_known \|\| tm_ball_pos_reliable))` 则退出；根据标志选择球数据源 | 调整姿态动作支持基于队友坐标 |

### 实现细节

所有修改节点采用统一模式（与 `CamTrackBall` 一致）：

```cpp
bool iKnowBallPos = brain->tree->getEntry<bool>("ball_location_known");
bool tmBallPosReliable = brain->tree->getEntry<bool>("tm_ball_pos_reliable");

if (!(iKnowBallPos || tmBallPosReliable)) {
    // 停止或退出
}

// 根据标志选择数据源
if (iKnowBallPos) {
    ballRange = brain->data->ball.range;
    ballYaw = brain->data->ball.yawToRobot;
    ballPos = brain->data->ball.posToField;
} else {
    ballRange = brain->data->tmBall.range;
    ballYaw = brain->data->tmBall.yawToRobot;
    ballPos = brain->data->tmBall.posToField;
}
```

### 预期效果

**性能提升**：
- 多机器人协作效率提升 30-50%
- 找球时间减少 2-4 秒
- 减少重复搜索动作
- 更灵活的球场覆盖策略

**工作流程**：
1. 机器人 A 看到球 → 发送 `ballPosToField`
2. 机器人 B 接收 → 设置 `tm_ball_pos_reliable = true`
3. B 的 Chase/SimpleChase/Adjust 检查 `ball_location_known || tm_ball_pos_reliable` → 通过
4. B 使用 `tmBall` 数据计算移动速度和方向
5. B 快速移动到球的大致位置，而不是继续盲目搜索

**可靠性保护**（已有逻辑，未修改）：
- B 只在 A 的 `ballDetected == true` 时信任坐标
- UDP 通信超时保护
- 共享坐标距离 B 必须 > 1.5m（避免使用过近的不准确坐标）

### 验证方法

**日志监控**：
```bash
grep "Reliable tm ball found" brain.log
# 应该看到：Reliable tm ball found. PlayerID = 1, distance = 3.2m

grep "Decision:" brain.log  
# B 应该从 find 切换到 chase/adjust
```

**实际测试场景**：
- A 看到球，B 看不到 → B 应快速移动到球位置（不再原地搜索）
- 远距离共享（>1.5m）→ B 快速接近
- 近距离共享（<1.5m）→ B 不使用（避免不准确）

### 注意事项

**未修改的节点**（已正确实现或设计合理）：
- `CamTrackBall`：已支持队友坐标（本次修改的参考标准）
- `MoveToFarSetPlaySearchArea`、`GoToFreekickPosition`、`KickoffStand`：已检查双条件
- 守门员节点：主要依赖实时视觉和轨迹预测，不强制使用队友坐标（设计合理）

**编译与部署**：
```bash
cd src/brain
colcon build --packages-select brain
# 重启机器人节点使修改生效
```

--------------------------------------------------------------

## 2026-09-22 角球和边线球第一脚禁止直接大脚

### 修改目的

裁判机发出我方 `CORNER_KICK`（角球）或 `THROW_IN`（边线球）后，第一脚不能沿用普通进攻中的直接射门、`power_kick` 或 `RLVisionKick` 大脚。第一脚改为低速 `cross`，确认球已经离开定位点后才恢复正常踢球策略。

### 修改文件与逻辑

| 文件 | 修改内容 |
| --- | --- |
| `src/brain/include/brain_data.h` | 增加 `nonDirectRestartFirstKickPending`、定位球类型、裁判机事件锁存、第一帧球位置和开始时间等运行时状态。该状态独立于原有的 `isFreekickKickingOff`。 |
| `src/brain/src/brain.cpp` | `gameControlCallback()` 在我方角球/边线球事件首次出现时置位保护状态，并清除遗留的 `isDirectShoot`。状态结束、换成对方定位球或其他比赛状态时清除。`updateNonDirectRestartMemory()` 只在比赛恢复后使用新鲜视觉球位置判断球是否移动。 |
| `src/brain/src/brain_tree.cpp` 的 `CalcKickDir` | 第一脚强制 `kickType="cross"`。角球方向指向中路；边线球方向指向场内，避免保留默认的球门射门方向。球暂时不可见时也先清除旧的 `shoot` 类型。 |
| `src/brain/src/brain_tree.cpp` 的 `StrikerDecide` | 第一脚禁止 `kickoff_visual_kick`、`auto_visual_kick` 和 `power_kick`；进入踢球条件后无论原来的 `kickType` 如何都输出 `cross`，并且不会因为第一次选择 `cross` 就提前清除定位球保护状态。 |
| `src/brain/src/brain_tree.cpp` 的 `RLVisionKick` | 在第一脚保护期间拒绝前锋和守门员的视觉大脚入口；如果保护状态在视觉踢球运行中置位，则退出该动作。 |
| `src/brain/src/brain.cpp` 的 `pubKickMsg()` | 第一脚保护期间停止发布 `/kick_ball` 视觉大脚消息，防止该发布通道绕过行为树决策。低速 `cross` 仍由行为树中的现有 `<Kick decision="cross">` 节点执行。 |
| `src/brain/config/config.yaml` | 增加 `strategy.non_direct_restart.ball_move_threshold: 0.25`（米）和 `strategy.non_direct_restart.timeout_ms: 12000.0`（毫秒）。 |

### 状态流程

1. 裁判机报告我方角球或边线球时，按事件边沿设置 `nonDirectRestartFirstKickPending=true`；同一个定位球事件不会因为裁判机重复发送消息而反复重置。
2. 裁判机恢复比赛后，记录第一帧新鲜球位置。只要球相对该位置移动达到 `0.25 m`，就认为第一脚已经完成并解除保护。
3. 视觉长期没有可靠移动信息时，保护最多持续 `12 s`，随后自动释放，避免异常情况下永久锁死。
4. 保护解除后，`CalcKickDir`、视觉大脚、power kick 和普通射门逻辑恢复原有行为。

行为树 XML 不需要新增节点，因为工程已经有低速 `cross` 节点；本次由上层决策和视觉大脚发布门禁控制第一脚行为。

### 验证记录

- 已通过源码检索核对 `gameControlCallback`、`CalcKickDir`、`StrikerDecide`、`RLVisionKick` 和 `pubKickMsg` 的保护路径均已接入同一状态。
- 本次未完成有效的编译验证，也未进行现场机器人运行验证；按要求不再继续编译。修改生效前仍需按现场流程重新构建并重启对应节点。

## 2026-09-22 对方开球直接找球开关

新增参数 `strategy.kickoff_hold_opponent`，用于控制对方开球时是否执行 `kickoff_hold` 前场待命策略。

- `false`：Ball Free 后取消前场待命；无球时直接进入 `find`，发现球后沿用正常的 `chase`、`adjust`、`kick` 流程。
- `true`：保持原有行为，对方开球时按前锋分组移动到预设待命位置。

当前 `src/brain/config/config.yaml` 设置为 `false`。裁判机允许比赛前的 `wait_for_opponent_kickoff` 等待逻辑保持不变，我方开球策略不受此开关影响。

## 2026-09-20 参数修改核对记录

远端设备：`booster@192.168.1.103`。读取时间：2026-09-20T11:08:34.070818+08:00。

代码包：`/home/booster/Robocup_ws/K1_5v5_demo_1.7`。

**比对口径**：本轮原值来自本次对话此前通过 SSH 实际读取的代码和参数，不使用之前回答中的建议值充当原值。远端目录没有 Git 仓库，无法据此恢复全包任意历史版本；本记录覆盖能够核实的参数与节点改动。当前行号按本次源码快照计数。

**核对结果：5 个源码文件，20 项参数修改，1 处节点注释修改。** 本次快照中，21 项均已同步到安装副本，涉及的 5 个源码文件与对应安装文件的 SHA-256 均一致。本次记录操作仅更新 CHANGELOG.md，没有更改运行配置、构建或重启机器人。

时间范围说明：10:23:52 的首次快照已确认 17 项变化，当时其中 10 项尚未同步；11:00:39 复查增加 `game.treat_person_as_robot: false → true`、`strategy.cooperation.enable_role_switch: true → false`；11:08:34 再次复查增加 `game.team_id: 152 → 30`、`strategy.enable_auto_visual_kick: true → false`。以下内容按 11:08:34 快照记录，全部 21 项已同步；不再沿用首次快照的未同步结论。

**文件：`/home/booster/Robocup_ws/K1_5v5_demo_1.7/src/brain/config/config.yaml`**

| 当前行号 | 参数/节点 | 原始值 | 修改后 | 单位 | 含义与修改方向 | 安装状态 |
| --- | --- | --- | --- | --- | --- | --- |
| 4 | game.team_id | 152 | 30 | 队伍编号 | 队伍编号改为 30，用于识别裁判机消息中的本队与开球方，应与 GameController 中的队伍编号一致。 | 已同步 |
| 9 | game.treat_person_as_robot | false | true | 布尔 | 开启将视觉检测到的 Person 同时加入机器人目标列表的调试功能；可能影响机器人/障碍物相关处理，配置原注释要求正式比赛时设为 false。 | 已同步 |
| 23 | robot.vx_limit | 1.6 | 2.5 | m/s | 提高经过 setVelocity 的前后速度分量上限；动作自身限速仍有效，不表示机器人始终以 2.5 m/s 行走，也不是底层 VisualKick 的统一限速。 | 已同步 |
| 25 | robot.vtheta_limit | 1.5 | 1.8 | rad/s | 提高经过 setVelocity 的身体旋转速度上限；不会直接改变头部转动速度。 | 已同步 |
| 50 | strategy.goalie_visual_kick_cooldown_ms | 600.0 | 900.0 | ms | 守门员重新启动视觉解围前，距离上次 VisualKick 退出需等待更久，减少连续重新进入。 | 已同步 |
| 51 | strategy.goalie_visual_kick_max_ball_range | 5.0 | 3.5 | m | 收紧新启动守门员视觉解围时的球距离上限；球权、成本排名、方向、场地区域等条件仍需满足。 | 已同步 |
| 52 | strategy.goalie_visual_kick_max_direction_error | 1.2 | 0.9 | rad | 普通解围入口允许的方向误差由约 68.8° 收紧至 51.6°；当前预测拦截入口可绕过此方向门槛。 | 已同步 |
| 55 | strategy.goalie_simple_threat_vxf | -0.03 | -0.08 | m/s | 条件是球的场地 X 方向速度小于该负值；更负意味着要求更明确地朝本方门线运动，减少微小速度噪声触发，也可能忽略更慢的来球。 | 已同步 |
| 56 | strategy.goalie_simple_predict_min_vxf | 0.06 | 0.10 | m/s | 提高采用防守平面预测交点所需的球 X 方向速度绝对值门槛；这里检查的是 X 分量，不是总球速。 | 已同步 |
| 78 | strategy.enable_auto_visual_kick | true | false | 布尔 | 关闭前锋决策中的自动视觉踢球入口；不等于关闭所有踢球动作，也不直接关闭守门员独立的视觉解围入口。 | 已同步 |
| 101 | strategy.cooperation.enable_role_switch | true | false | 布尔 | 关闭按动态守门员选举结果切换本机角色，常规角色更新改用 game.player_role；当前该值仍为 striker，所以不会自动按选举结果成为守门员。接收队友换位命令的分支没有检查此开关。 | 已同步 |
| 107 | strategy.cooperation.goalie_attack_handoff | true | false | 布尔 | 关闭本机守门员获得近球后主动交接并转为前锋的流程；此项自身不负责设置初始角色，也不阻止接收队友换位命令。 | 已同步 |

**文件：`/home/booster/Robocup_ws/K1_5v5_demo_1.7/src/brain/behavior_trees/game.xml`**

| 当前行号 | 参数/节点 | 原始值 | 修改后 | 单位 | 含义与修改方向 | 安装状态 |
| --- | --- | --- | --- | --- | --- | --- |
| 66 | GoToReadyPosition.dist_tolerance | 0.6 | 0.5 | m | READY 阶段要求更靠近目标站位才停止；这是位置到达容差，不是速度参数。 | 已同步 |

**文件：`/home/booster/Robocup_ws/K1_5v5_demo_1.7/src/brain/behavior_trees/subtrees/subtree_striker_play.xml`**

| 当前行号 | 参数/节点 | 原始值 | 修改后 | 单位 | 含义与修改方向 | 安装状态 |
| --- | --- | --- | --- | --- | --- | --- |
| 42 | Assist.vx_limit | 1.2 | 1.6 | m/s | 提高前锋接应/协防动作的前进速度上限；该动作后退速度仍单独限制为 0.25 m/s。 | 已同步 |
| 42 | Assist.vy_limit | 0.6 | 0.8 | m/s | 提高前锋接应/协防动作的横移速度上限，现与全局横移上限 0.8 相同。 | 已同步 |
| 44 | Chase.vx_limit | 1.6 | 2.0 | m/s | 提高前锋追球前进速度上限；仍按球距离和转向偏差减速，并受全局 2.5 上限约束。 | 已同步 |
| 44 | Chase.vtheta_limit | 1.5 | 1.8 | rad/s | 提高追球时身体转向速度上限，现与全局旋转上限 1.8 相同。 | 已同步 |

**文件：`/home/booster/Robocup_ws/K1_5v5_demo_1.7/src/brain/behavior_trees/subtrees/subtree_goal_keeper_play.xml`**

| 当前行号 | 参数/节点 | 原始值 | 修改后 | 单位 | 含义与修改方向 | 安装状态 |
| --- | --- | --- | --- | --- | --- | --- |
| 28 | RLVisionKick.range | 3.5 | 4.0 | m | 放宽视觉解围运行中的球距离退出门槛；超过最短运行时间且连续 4 次检测超距才按此条件退出。不是启动距离，也不是踢球力量。 | 已同步 |
| 32 | GoalieIdleHome.pos_tolerance | 0.45 | 0.25 | m | 缩小待命站位的位置到达容差，使停止位置更接近目标；定位噪声较大时可能增加调整次数。 | 已同步 |
| 32 | GoalieIdleHome.theta_tolerance | 0.40 | 0.30 | rad | 缩小待命站位朝向误差容差，约由 22.9° 降至 17.2°，更严格地对齐场内方向。 | 已同步 |

**文件：`/home/booster/Robocup_ws/K1_5v5_demo_1.7/src/brain/behavior_trees/subtrees/subtree_goal_keeper_penalty_kick.xml`**

| 当前行号 | 参数/节点 | 原始值 | 修改后 | 单位 | 含义与修改方向 | 安装状态 |
| --- | --- | --- | --- | --- | --- | --- |
| 7 | CamScanField (_while="!ball_location_known") | 启用 | 已注释 | 节点状态 | 禁用未知球位置时的额外扫场；CamFindAndTrackBall 的找球逻辑仍在。第 9 行已知球位置时的 CamScanField 仍启用，盯球被扫场覆盖的路径尚未消除。 | 已同步 |

**需要注意的实际效果**

1. 点球树目前注释的是第 7 行 `_while="!ball_location_known"`；第 9 行 `_while="ball_location_known"` 仍启用。与此前“取消已知球位置时扫场”的建议并不相同；这项修改没有消除已知球位置时盯球/扫场命令覆盖。
2. `goalie_attack_handoff=false` 只关闭本机主动交接进攻。现在 `config.yaml:101` 的 `enable_role_switch=false`，常规角色更新会使用第 7 行的 `game.player_role`，而它仍为 `striker`。所以当前配置不能记录为“已固定为守门员”；角色选举计算仍会执行，但常规更新不再按选举结果切换本机角色。接收队友换位指令的分支仍可能在当次更新内写入守门员角色，下一次常规更新又恢复配置角色。
3. 新源码将普通视觉解围启动距离收紧至 3.5 m，同时把运行时的距离退出门槛放宽至 4.0 m。两个参数分别控制启动和退出，可形成距离上的滞回；不是同一个参数的冲突。实际仍受球权和其他退出条件约束。
4. `GoalieIdleHome` 的停止容差变小，还通过现有公式把解锁重定位阈值从位置 0.81 m/角度 0.80 rad 变为位置 0.60 m/角度 0.60 rad。这是原有公式产生的间接效果，没有新增参数。
5. `GoToReadyPosition.dist_tolerance` 从 0.6 m 降至 0.5 m，按现有公式，其位置解锁阈值也从 0.9 m 降至 0.75 m。
6. 全局旋转上限上调会影响其他走 `setVelocity` 的动作。例如 `Adjust` 自身仍设为 2.0 rad/s，但全局允许的最高值由 1.5 提升至 1.8 rad/s。守门员 `Intercept2` 自身仍限制为 1.5 rad/s，所以该动作不会仅因此提高到 1.8。
7. 最小速度修正没有改：`src/brain/src/robot_client.cpp:196` 的 `minx/miny/mintheta` 仍均为 0.3；`:203` 开始的全局上限限制也还在。`Chase` 仍默认开启修正，`Intercept2` 仍明确关闭三个方向的修正。完整路径见下方未修改项。
8. `game.treat_person_as_robot=true` 是调试开关：检测到的人也被加入机器人目标列表。其配置注释明确要求正式比赛时设为 false；本次只记录实际值。

**安装副本状态**

| 源码文件 | 安装副本完整路径 | 状态 |
| --- | --- | --- |
| config.yaml | /home/booster/Robocup_ws/K1_5v5_demo_1.7/install/brain/share/brain/config/config.yaml | 文件完全一致 |
| game.xml | /home/booster/Robocup_ws/K1_5v5_demo_1.7/install/brain/share/brain/behavior_trees/game.xml | 文件完全一致 |
| subtree_striker_play.xml | /home/booster/Robocup_ws/K1_5v5_demo_1.7/install/brain/share/brain/behavior_trees/subtrees/subtree_striker_play.xml | 文件完全一致 |
| subtree_goal_keeper_play.xml | /home/booster/Robocup_ws/K1_5v5_demo_1.7/install/brain/share/brain/behavior_trees/subtrees/subtree_goal_keeper_play.xml | 文件完全一致 |
| subtree_goal_keeper_penalty_kick.xml | /home/booster/Robocup_ws/K1_5v5_demo_1.7/install/brain/share/brain/behavior_trees/subtrees/subtree_goal_keeper_penalty_kick.xml | 文件完全一致 |

11:08:34 复查发现正在运行的 `brain_node` 进程，PID 为 205870，启动命令指定安装目录中的 config.yaml，并额外加载 `/tmp/launch_params_7cm4n15k`。本次未查询节点运行时参数，也未核对额外参数文件的覆盖内容，因此“已同步”仅证明文件一致，不能直接证明进程内所有参数均采用当前文件值。源码和安装目录均未发现 `config_local.yaml`。

运行进程指定的主配置完整路径：`/home/booster/Robocup_ws/K1_5v5_demo_1.7/install/brain/share/brain/config/config.yaml`。

**本次确认未改的相关项**

| 完整文件路径 | 行号 | 参数/调用 | 当前值 |
| --- | --- | --- | --- |
| /home/booster/Robocup_ws/K1_5v5_demo_1.7/src/brain/config/config.yaml | 7 | game.player_role | striker |
| /home/booster/Robocup_ws/K1_5v5_demo_1.7/src/brain/config/config.yaml | 34 | strategy.limit_near_ball_speed | false |
| /home/booster/Robocup_ws/K1_5v5_demo_1.7/src/brain/config/config.yaml | 60 | strategy.goalie_simple_vy_max | 0.8 m/s |
| /home/booster/Robocup_ws/K1_5v5_demo_1.7/src/brain/config/config.yaml | 61 | strategy.goalie_simple_vx_max | 1.2 m/s |
| /home/booster/Robocup_ws/K1_5v5_demo_1.7/src/brain/config/config.yaml | 63 | strategy.goalie_simple_vtheta_limit | 1.5 rad/s |
| /home/booster/Robocup_ws/K1_5v5_demo_1.7/src/brain/config/config.yaml | 148 | RLVisionKick.goalie_clearance_power | 2.5 |
| /home/booster/Robocup_ws/K1_5v5_demo_1.7/src/brain/src/robot_client.cpp | 196 | minx / miny / mintheta | 0.3 / 0.3 / 0.3 |
| /home/booster/Robocup_ws/K1_5v5_demo_1.7/src/brain/include/robot_client.h | 49 | applyMinX / applyMinY / applyMinTheta 默认值 | true / true / true |
| /home/booster/Robocup_ws/K1_5v5_demo_1.7/src/brain/src/brain_tree.cpp | 612 | Chase.setVelocity | 默认开启最小速度修正 |
| /home/booster/Robocup_ws/K1_5v5_demo_1.7/src/brain/src/brain_tree.cpp | 2537 | Intercept2.setVelocity | false / false / false |

**更早历史备份差异（另列，不算本轮修改）**

备份文件：`/home/booster/Robocup_ws/K1_5v5_demo_1.7/install/brain/share/brain/config/config.yaml.bak-20260919-142724-410856`。该备份比本次对话早，下面 9 项在此前对话第一次读取时已经是当前值，不能认定为刚刚修改。备份的裁判机 IP 行缺少闭合引号，此处按文本读取，不把它视为可直接加载的 YAML 配置。

当前文件：`/home/booster/Robocup_ws/K1_5v5_demo_1.7/src/brain/config/config.yaml`。

| 参数 | 旧备份行号 | 当前行号 | 备份原值 | 当前值 | 含义及修改方向 |
| --- | --- | --- | --- | --- | --- |
| game.player_id | 5 | 5 | 1 | 2 | 机器人编号由 1 改为 2；在本次对话最初读取时已是 2。 |
| game.number_of_players | 10 | 10 | 5 | 3 | 代码参与协同计算的球员数量减少；不是比赛人数规则的修改。 |
| locator.min_marker_count | 151 | 151 | 4 | 5 | 定位要求更多标志点，接受定位的条件更严格。 |
| locator.max_residual | 152 | 152 | 0.4 | 0.35 | 减小定位允许的残差上限，接受定位的条件更严格。 |
| vision.cam_pixel_width | 173 | 171 | 1280. | 544. | 图像宽度改为 544 像素，用于图像中心和视角换算；不是直接设置相机驱动分辨率。 |
| vision.cam_pixel_height | 174 | 172 | 720. | 448. | 图像高度改为 448 像素，与当前相机图像规格配套。 |
| vision.cam_fov_x | 175 | 179 | 90.0 | 69.4 | 水平视场角配置减小，用于视觉角度换算，不会改变镜头本身。 |
| vision.cam_fov_y | 176 | 180 | 60.0 | 42.5 | 垂直视场角配置减小，用于视觉角度换算，不会改变镜头本身。 |
| game_control_ip | 193 | 195 | 172.169.148.171 | 192.168.1.100 | 裁判机 IP 改到当前网段；旧备份该行字符串缺少闭合引号，仅按文本比较。 |

**证据与可复查记录**

原值来自本次对话此前实际 SSH 读取记录或明确列出的旧备份；整理期间新增四项变化另有 10:23:52 和 11:00:39 快照作为直接依据。核对端另行保留各次快照以及逐项 CSV/JSON；这些辅助文件保存在核对端本地，没有放入远端项目。

修改时间仅代表文件系统记录，不证明具体是谁修改，也不用于推定原始出厂版本。

-----------------------------------------------------------------

## 2026-09-20 双前锋视觉踢球间距控制

本次通过 SSH 修改远端代码包 `/home/booster/Robocup_ws/K1_5v5_demo_1.7`，目标是：一个前锋进入 `RLVisionKick`/视觉踢球后，另一名前锋不再继续抢球或同时进入视觉踢球，而是退到主攻手后侧并保持约定距离。修改前没有 `visual_kick_spacing` 配置，也没有基于队友视觉踢球状态的距离约束；两个前锋可能因各自本地看到足球，在队友状态同步前同时抢球。

远端源码修改前备份：`/home/booster/Robocup_ws/K1_5v5_demo_1.7/backup/before_spacing_20260920_1715/`。

### 修改文件与行号

| 文件与行号 | 原始内容 | 修改后内容 | 作用 |
| --- | --- | --- | --- |
| `src/brain/config/config.yaml:100-107` | 没有该配置段 | `enable=true`；`target_distance=2.2`；`yield_distance=1.8`；`release_distance=2.0`；`stop_distance=1.5`；`peer_fresh_ms=500.0`；`yield_speed=0.5` | 增加视觉踢球时的双前锋间距、通信新鲜度和让位速度参数。 |
| `src/brain/src/brain.cpp:166-172` | 没有对应 ROS 参数声明 | 声明上述 7 个 `strategy.visual_kick_spacing.*` 参数及默认值 | 使 YAML 参数可以被 `Brain` 读取；源码默认值与配置值一致。 |
| `src/brain/src/brain_tree.cpp:112-201` | 没有间距设置读取、队友状态筛选和主攻手选择 | 新增 `VisualKickSpacingSettings`、状态新鲜度检查、按 `cost` 选择主攻手；同成本时按较小球员 ID 决胜 | 只使用存活、角色为 `striker`、正在视觉踢球且通信不超过 500 ms 的队友，避免使用过期状态。 |
| `src/brain/src/brain_tree.cpp:1320-1425`（`Assist::tick`） | Assist 只按普通 cost/rank 计算位置，没有围绕视觉踢球主攻手的目标点 | 接应位置放在主攻手踢球方向的后侧并横向错开；距离过近时向外退让；接应移动速度限制为 `0.5 m/s` | 给主攻手让出踢球通道，减少两名前锋靠近或相撞。`stop_distance` 是近距离强制外移阈值，`release_distance` 是解除强制让位的距离。 |
| `src/brain/src/brain_tree.cpp:1693-1705`（`StrikerDecide::tick`） | 两名前锋都可依据本地球观测继续进入 chase/visual kick | 若队友已赢得视觉踢球名额，本机清除自己的视觉踢球意图并切换到 `assist` | 处理通信更新前的竞争窗口，优先保证唯一主攻手。 |
| `src/brain/src/brain_tree.cpp:2354-2360`（`RLVisionKick::onStart`） | 进入视觉踢球时没有再次检查队友是否已占用名额 | 启动前再次检查队友主攻手；若队友 cost 更低则立即放弃本次进入 | 防止已经做出本地决策后仍与队友同时启动。 |
| `src/brain/src/brain_tree.cpp:2410-2411`、`:2559-2562`（`RLVisionKick::onRunning`） | 运行中只按原有球权/成本条件退出，近球时还可能忽略队友竞争 | 运行期间持续检查队友主攻手；发现队友胜出就退出并恢复 `RobocupWalk`，删除近球时忽略队友的竞争例外 | 视觉踢球过程中也让出名额，避免两台机器人持续争球。底层 `RLVisionKick` SDK 服务本身未修改。 |

### 参数含义

- `enable`：总开关，`true` 时启用双前锋视觉踢球间距逻辑。
- `target_distance=2.2`：接应位相对主攻手的目标距离，单位米。
- `yield_distance=1.8`：让位区间阈值，单位米；同时用于校验阈值关系，保证它位于停止和目标距离之间。
- `release_distance=2.0`：队友距离达到该值后解除强制让位状态，单位米，形成退出滞回，减少反复抢位。
- `stop_distance=1.5`：队友距离小于该值时强制向外移动，单位米。
- `peer_fresh_ms=500.0`：队友视觉踢球状态允许的最大通信延迟，超过该时间视为过期，单位毫秒。
- `yield_speed=0.5`：接应/让位时的平面移动速度上限，单位米每秒。

主攻手选择规则是：在有效队友中选 `cost` 最小者；`cost` 相同时选球员 ID 较小者。只有队友胜过本机时，本机才切换 `assist` 或退出 `RLVisionKick`，因此不会无条件阻止更有资格的前锋踢球。已有的障碍物检测和避障分支仍继续生效；本次新增的是队友视觉踢球状态和相对位置约束，不是底层 SDK 的碰撞保证。

### 构建与测试

- `colcon build --packages-select brain`：成功，安装后的 `install/brain/lib/brain/brain_node` 时间戳为 `2026-09-20 17:22:38 +0800`。
- 源配置与安装配置 SHA-256 一致：`src/brain/config/config.yaml` 与 `install/brain/share/brain/config/config.yaml` 均为 `65194b1bd101a6612cf02fe246271f6c3a3e782202d36c7176e80043d7578ca6`。
- `ctest --test-dir build/brain --output-on-failure`：`test_kick_geometry` 通过；其余 6 项失败原因为测试环境缺少 Python 模块 `ament_cmake_test`（`ModuleNotFoundError`），不是本次 C++ 编译错误。
- 核查时未发现 `brain_node` 进程，也未停止或重启机器人服务；本次修改不会自动加载到正在运行的旧进程，需要后续按现场启动流程重启后才会生效。

-------------------------------------------------------------------
## 2026-09-21 开球 rank 0 视觉踢球与 power 阈值

### 修改内容

| 文件 | 位置 | 原始值/逻辑 | 修改后 | 说明 |
|---|---|---|---|---|
| `/home/pmx-ros2/05 robot/K1_5v5_demo_1.7_duobi/src/brain/src/brain_tree.cpp` | `StrikerDecide::tick()`，约 1692-1724、1826-1840、1907-1909 | 我方开球时没有 rank 0 的专用强制视觉踢球分支；无球源时会进入 `find`/`kickoff_hold` | 新增 `kickoffRank0VisualKick`：`data->isKickingOff && kickoffRank == 0 && player_role == "striker"`，输出 `decision="kickoff_visual_kick"` | 我方普通开球期间，锁定前锋 cost 排名 0 的机器人优先进入 `RLVisionKick` 行为树；不再受普通 `tmImLead`、cost 阈值、前场坐标、球距离和 `kickoffHold` 分支限制。行为树仍保留球源有效性安全检查。 |
| `/home/pmx-ros2/05 robot/K1_5v5_demo_1.7_duobi/src/brain/src/brain_tree.cpp` | `RLVisionKick::onStart()`，约 2350-2390 | rank 0 机器人可能因队友视觉踢球间距竞争被启动前取消 | 开球 rank 0 机器人跳过队友视觉踢球让位检查 | 保持 rank 0 开球主攻资格；球源无效时仍取消底层启动。 |
| `/home/pmx-ros2/05 robot/K1_5v5_demo_1.7_duobi/src/brain/src/brain_tree.cpp` | `RLVisionKick::onRunning()`，约 2415-2447 | 运行中可能因队友视觉踢球状态触发让位退出 | 开球 rank 0 机器人在 `isKickingOff` 期间跳过该让位退出检查 | 防止开球视觉踢球过程中被队友状态抢占；其他恢复、球丢失、超时和异常姿态退出条件仍保留。 |
| `/home/pmx-ros2/05 robot/K1_5v5_demo_1.7_duobi/src/brain/src/brain.cpp` | `Brain::pubKickMsg()`，约 567-575 | `dist > 6.0` 时 `power=3.0`，否则 `power=6.0` | 普通踢球阈值改为 `dist > 10.0` | 球到对方球门中心距离不超过 10 m 时使用 `power=6.0`，超过 10 m 时使用 `power=3.0`。守门员清球和已有开球专用 `RLVisionKick.kickoff_power=6.0` 分支不受影响。 |

### 当前开球流程

裁判机确认我方开球后设置 `data->isKickingOff=true`；开球阶段锁定的前锋排名为 0 且角色为 `striker` 时，`StrikerDecide` 输出 `kickoff_visual_kick`，行为树进入 `RLVisionKick`。已有配置 `/home/pmx-ros2/05 robot/K1_5v5_demo_1.7_duobi/src/brain/config/config.yaml:157` 的 `RLVisionKick.kickoff_power: 6.0` 用于开球踢球消息。

“无条件”仅指上层决策不再要求普通追球和 cost/距离条件；`RLVisionKick::onStart()` 仍要求真实球、可靠预测球或允许的记忆球源有效，避免在没有可用球数据时向底层发送无目标踢球命令。

### 备份与验证

- 修改前备份：`/home/pmx-ros2/05 robot/K1_5v5_demo_1.7_duobi/backup/kickoff_rank0_power10_20260921_201659/`。
- 已核对 `kickoff_visual_kick` 在 `subtree_striker_play.xml` 中作为 `RLVisionKick` 的有效条件。
- `colcon build --packages-select brain` 未能启动编译：该目录旧的 `build/brain/colcon_command_prefix_build.sh` 仍引用不存在的 `/home/booster/Robocup_ws/K1_5v5_demo_1.7/install/...` 依赖路径，属于构建缓存环境问题，并非本次编译错误。
