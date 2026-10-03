#include "state_logger.h"
#include <fstream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <ctime>
#include <iostream>

StateLogger::StateLogger(const std::string& logFilePath)
    : logFilePath_(logFilePath), running_(false)
{
}

StateLogger::~StateLogger()
{
    stop();
}

void StateLogger::start()
{
    if (running_) return;
    running_ = true;
    logThread_ = std::thread(&StateLogger::logLoop, this);
}

void StateLogger::stop()
{
    if (!running_) return;
    running_ = false;
    if (logThread_.joinable()) {
        logThread_.join();
    }
}

void StateLogger::updateState(const TeamStateSnapshot& snapshot)
{
    std::lock_guard<std::mutex> lock(stateMutex_);
    currentState_ = snapshot;
}

std::string StateLogger::formatTimestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::tm tm_now;
    localtime_r(&time_t_now, &tm_now);

    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%H:%M:%S")
        << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

std::string StateLogger::formatHeader()
{
    std::ostringstream oss;
    oss << "\n" << std::string(200, '=') << "\n";
    oss << formatTimestamp() << " | TEAM STATE MONITOR\n";
    oss << std::string(200, '=') << "\n";
    return oss.str();
}

std::string StateLogger::formatGameState(const TeamStateSnapshot& state)
{
    std::ostringstream oss;
    oss << "┌─ GAME STATE ─────────────────────────────────────────────────────┐\n";
    oss << "│ Primary:   " << std::left << std::setw(20) << state.primaryState
        << " │ Secondary: " << std::setw(20) << state.secondaryState << " │\n";
    oss << "│ Score:     " << state.ownScore << " - " << state.opponentScore
        << std::setw(48) << "" << " │\n";
    oss << "└──────────────────────────────────────────────────────────────────┘\n";
    return oss.str();
}

std::string StateLogger::formatRobotState(const RobotState& robot, bool isSelf)
{
    std::ostringstream oss;

    // 标题行
    if (isSelf) {
        oss << "┌─ ROBOT #" << robot.playerId << " (SELF) ────────────────────────────────────────────────┐\n";
    } else {
        oss << "├─ ROBOT #" << robot.playerId << " (TEAMMATE) ─────────────────────────────────────────────┤\n";
    }

    // 第一行：角色和基本状态
    oss << "│ Role: " << std::left << std::setw(12) << robot.role
        << " │ Cost: " << std::fixed << std::setprecision(2) << std::setw(6) << robot.cost
        << " │ Leader: " << (robot.isLeader ? "YES" : "NO ")
        << " │ VisionKick: " << (robot.inVisualKick ? "YES" : "NO ") << " │\n";

    // 第二行：当前行为和位置
    oss << "│ State: " << std::left << std::setw(15) << robot.currentBehavior
        << " │ Pos: (" << std::fixed << std::setprecision(2) << std::setw(6) << robot.posX
        << ", " << std::setw(6) << robot.posY
        << ", " << std::setw(5) << robot.posTheta << ")                │\n";

    // 第三行：速度
    oss << "│ Vel: (" << std::setw(5) << robot.velX
        << ", " << std::setw(5) << robot.velY
        << ", " << std::setw(5) << robot.velTheta << ")"
        << std::setw(45) << "" << "│\n";

    // 第四行：球的信息
    if (robot.ballDetected) {
        oss << "│ Ball: DETECTED │ Range: " << std::setw(5) << robot.ballRange
            << "m │ Yaw: " << std::setw(6) << robot.ballYaw
            << "rad │ Pos: (" << std::setw(6) << robot.ballPosX
            << ", " << std::setw(6) << robot.ballPosY << ")     │\n";
    } else {
        oss << "│ Ball: NOT DETECTED" << std::setw(51) << "" << "│\n";
    }

    // 第五行：避让状态
    if (robot.collisionAvoidanceActive) {
        oss << "│ ⚠️  COLLISION AVOIDANCE ACTIVE │ Factor: " << std::setw(4) << robot.avoidanceSpeedFactor
            << " │ Closest: " << std::setw(5) << robot.closestTeammateDistance << "m"
            << std::setw(20) << "" << "│\n";
    } else {
        oss << "│ Collision Avoidance: INACTIVE" << std::setw(40) << "" << "│\n";
    }

    return oss.str();
}

std::string StateLogger::formatFooter()
{
    return "└────────────────────────────────────────────────────────────────────┘\n";
}

void StateLogger::logLoop()
{
    std::ofstream logFile(logFilePath_, std::ios::out | std::ios::app);

    if (!logFile.is_open()) {
        std::cerr << "Failed to open state log file: " << logFilePath_ << std::endl;
        return;
    }

    while (running_) {
        TeamStateSnapshot snapshot;
        {
            std::lock_guard<std::mutex> lock(stateMutex_);
            snapshot = currentState_;
        }

        // 构建完整的状态输出
        std::ostringstream output;
        output << formatHeader();
        output << formatGameState(snapshot);

        // 输出自己的状态
        for (const auto& robot : snapshot.robots) {
            if (robot.isSelf) {
                output << formatRobotState(robot, true);
                break;
            }
        }

        // 输出队友状态
        for (const auto& robot : snapshot.robots) {
            if (!robot.isSelf && robot.isAlive) {
                output << formatRobotState(robot, false);
            }
        }

        output << formatFooter();

        // 写入文件
        logFile << output.str() << std::flush;

        // 100ms 更新一次
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    logFile.close();
}
