/*
    双足机器人 - 随机游走状态机
    在后台任务中循环:
        1. 等待 5-30秒随机时间 (原地小动作 + 监听环境)
        2. 决定下一步行动 (本地决策 或 调用云端AI)
        3. 执行动作 (走/转/摇头等)
        4. 同时发出语音评价
        5. 写入记忆

    决策规则 (本地 + 云端混合):
        - 检测到障碍 (<20cm) -> 必须转向避开
        - 一般情况 -> 70%本地随机决策, 30%云端AI决策
        - 决策包括: 行动 + 一句语音评价
*/

#ifndef BIPED_BEHAVIOR_H
#define BIPED_BEHAVIOR_H

#include "biped_controller.h"
#include "biped_memory.h"
#include "ultrasonic_sensor.h"
#include <freertos/FreeRTOS.h>
#include <functional>
#include <string>

enum class BipedBehaviorState {
    IDLE,        // 等待状态 (原地小动作)
    DECIDING,    // 决定下一步动作
    MOVING,      // 行走/转向
    SPEAKING,    // 语音输出
};

struct BipedAction {
    enum Type {
        WALK_FORWARD,
        WALK_BACKWARD,
        TURN_LEFT,
        TURN_RIGHT,
        SHAKE_HEAD,
        BOW,
        SIT,
        STAND,
        NO_ACTION,
    };
    Type type;
    int steps;       // 行走步数
    int duration_ms; // 总时长
    std::string comment;  // 同步播放的语音评价
};

class BipedBehavior {
public:
    using SpeakCallback = std::function<void(const std::string& text)>;
    using AskAiCallback = std::function<std::string(const std::string& context)>;

    BipedBehavior(BipedController* controller,
                  UltrasonicSensor* sensor,
                  BipedMemory* memory);
    ~BipedBehavior();

    void Begin();
    void Stop();

    // 设置语音回调 (连接到 Application 的TTS功能)
    void SetSpeakCallback(SpeakCallback cb) { speak_cb_ = cb; }

    // 设置云端AI询问回调 (连接到协议层)
    void SetAskAiCallback(AskAiCallback cb) { ask_ai_cb_ = cb; }

    BipedBehaviorState GetState() const { return state_; }

    // 强制立即决策 (例如外部打断或新提示)
    void TriggerImmediateDecision();

private:
    void TaskLoop_();
    static void TaskEntry_(void* arg);

    BipedAction MakeLocalDecision_();
    BipedAction MakeAiDecision_(const std::string& env_context);
    void ExecuteAction_(const BipedAction& action);
    void SetState_(BipedBehaviorState new_state);

    BipedController* controller_;
    UltrasonicSensor* sensor_;
    BipedMemory* memory_;

    TaskHandle_t task_handle_;
    BipedBehaviorState state_;
    bool running_;
    bool immediate_trigger_;

    SpeakCallback speak_cb_;
    AskAiCallback ask_ai_cb_;

    uint32_t min_interval_ms_;
    uint32_t max_interval_ms_;

    int consecutive_turn_count_;  // 连续同方向转次数 (防止原地打转)
};

#endif  // BIPED_BEHAVIOR_H