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
