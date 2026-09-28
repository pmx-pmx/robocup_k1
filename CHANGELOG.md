# CHANGELOG

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

---

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
