# gdte-2026-soccer

Robot soccer control, vision, and game-controller code based on ROS 2.

## Repository layout

- `src/`: ROS 2 packages and application source code
- `configs/`: runtime configuration files
- `scripts/`: build, start, stop, and calibration scripts
- `distribution/`: packaging and installation scripts
- `utils/`: service and auto-start helpers
- `backup/`: selected configuration backups

Build output (`build/`, `install/`, `log/`) and runtime logs are intentionally excluded from version control. Build the workspace on the target machine with the project's ROS 2 toolchain and the scripts in `scripts/`.

在pmx-2上追加修改：1.RLVisionKick 踢球左右补偿功能
                  RLVisionKick:
                  kick_direction_offset_left: 0.1   # 左脚踢球补偿（球在左侧，y>0）
                  kick_direction_offset_right: -0.1 # 右脚踢球补偿（球在右侧，y<0）
                  2.RLVisionKick 快速进入优化
                  auto_visual_kick_enable_angle: 2.0
                  auto_visual_kick_max_direction_error: 2.5
