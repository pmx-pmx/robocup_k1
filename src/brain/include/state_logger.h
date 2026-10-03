#pragma once

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

// 单个机器人的状态信息
struct RobotState
{
    int playerId = 0;
    bool isSelf = false;
    bool isAlive = false;

    // 队长和控球相关
    double cost = 0.0;
    bool isLeader = false;
    bool inVisualKick = false;
    std::string currentBehavior = "UNKNOWN";
    std::string role = "UNKNOWN";  // 角色：striker 或 goal_keeper

    // 位置和速度（场地坐标系）
    double posX = 0.0;
    double posY = 0.0;
    double posTheta = 0.0;
    double velX = 0.0;
    double velY = 0.0;
    double velTheta = 0.0;

    // 球的信息（机器人视角）
    bool ballDetected = false;
    double ballRange = 0.0;
    double ballYaw = 0.0;
    double ballPosX = 0.0;  // 场地坐标系
    double ballPosY = 0.0;

    // 碰撞避让状态
    bool collisionAvoidanceActive = false;
    double avoidanceSpeedFactor = 1.0;
    double closestTeammateDistance = 999.0;
};

// 整个团队的状态快照
struct TeamStateSnapshot
{
    std::string timestamp;

    // 比赛状态
    std::string primaryState = "UNKNOWN";
    std::string secondaryState = "UNKNOWN";
    int ownScore = 0;
    int opponentScore = 0;

    // 所有机器人状态
    std::vector<RobotState> robots;
};

// 状态日志器
class StateLogger
{
public:
    explicit StateLogger(const std::string& logFilePath);
    ~StateLogger();

    void start();
    void stop();
    void updateState(const TeamStateSnapshot& snapshot);

private:
    void logLoop();
    std::string formatTimestamp();
    std::string formatHeader();
    std::string formatGameState(const TeamStateSnapshot& state);
    std::string formatRobotState(const RobotState& robot, bool isSelf);
    std::string formatFooter();

    std::string logFilePath_;
    std::atomic<bool> running_;
    std::thread logThread_;
    std::mutex stateMutex_;
    TeamStateSnapshot currentState_;
};
