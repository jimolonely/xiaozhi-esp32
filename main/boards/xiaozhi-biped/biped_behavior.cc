/*
    双足机器人 - 随机游走状态机实现
*/

#include "biped_behavior.h"
#include "config.h"
#include <esp_log.h>
#include <esp_random.h>
#include <algorithm>
#include <cstring>

#define TAG "BipedBehavior"

static const char* kForwardComments[] = {
    "这边走走",
    "看看前面有什么",
    "往前溜达溜达",
    "我往那边走走",
    "换个地方看看",
};
static const char* kBackwardComments[] = {
    "退后一步",
    "换个方向",
    "我去那边",
};
static const char* kTurnLeftComments[] = {
    "转个方向看看",
    "左看看",
    "那边好像有意思",
};
static const char* kTurnRightComments[] = {
    "换个方向",
    "右转一下",
    "我去右边看看",
};
static const char* kShakeHeadComments[] = {
    "嗯哼",
    "嗯?",
    "咦",
    "有点无聊啊",
    "我想静静",
};
static const char* kBowComments[] = {
    "你们好呀",
    "初次见面",
    "打招呼",
};
static const char* kObstacleComments[] = {
    "哎呀差点撞到",
    "前面有东西",
    "得绕一下",
    "小心小心",
};

BipedBehavior::BipedBehavior(BipedController* controller,
                             UltrasonicSensor* sensor,
                             BipedMemory* memory)
    : controller_(controller),
      sensor_(sensor),
      memory_(memory),
      task_handle_(nullptr),
      state_(BipedBehaviorState::IDLE),
      running_(false),
      immediate_trigger_(false),
      speak_cb_(nullptr),
      ask_ai_cb_(nullptr),
      min_interval_ms_(RANDOM_WALK_MIN_INTERVAL_MS),
      max_interval_ms_(RANDOM_WALK_MAX_INTERVAL_MS),
      consecutive_turn_count_(0) {}

BipedBehavior::~BipedBehavior() {
    Stop();
}

void BipedBehavior::SetState_(BipedBehaviorState new_state) {
    if (state_ != new_state) {
        ESP_LOGD(TAG, "状态切换: %d -> %d", (int)state_, (int)new_state);
        state_ = new_state;
    }
}

void BipedBehavior::Begin() {
    if (running_) return;
    running_ = true;
    xTaskCreate(TaskEntry_, "biped_behavior", 4096, this, 5, &task_handle_);
    ESP_LOGI(TAG, "随机游走状态机启动");
}

void BipedBehavior::Stop() {
    if (!running_) return;
    running_ = false;
    if (task_handle_ != nullptr) {
        vTaskDelete(task_handle_);
        task_handle_ = nullptr;
    }
    ESP_LOGI(TAG, "随机游走状态机停止");
}

void BipedBehavior::TriggerImmediateDecision() {
    immediate_trigger_ = true;
}

void BipedBehavior::TaskEntry_(void* arg) {
    BipedBehavior* self = static_cast<BipedBehavior*>(arg);
    self->TaskLoop_();
    vTaskDelete(nullptr);
}

uint32_t RandomInRange(uint32_t min, uint32_t max) {
    return min + (esp_random() % (max - min + 1));
}

const char* PickRandom(const char* const* arr, size_t n) {
    return arr[esp_random() % n];
}

BipedAction BipedBehavior::MakeLocalDecision_() {
    BipedAction action = {};

    // 1. 检查障碍物 - 优先避障
    if (sensor_ != nullptr) {
        float dist = sensor_->GetDistanceCm();
        if (dist > 0 && dist < OBSTACLE_DETECT_DISTANCE_CM) {
            // 检测到障碍, 必须转向避开
            bool turn_right = (esp_random() % 2) == 0;
            action.type = turn_right ? BipedAction::TURN_RIGHT : BipedAction::TURN_LEFT;
            action.steps = 3 + (esp_random() % 3);
            action.duration_ms = action.steps * GAIT_DEFAULT_SPEED_MS;
            action.comment = PickRandom(kObstacleComments,
                                        sizeof(kObstacleComments) / sizeof(kObstacleComments[0]));
            consecutive_turn_count_++;
            return action;
        }
    }

    // 2. 防止连续同方向转弯
    if (consecutive_turn_count_ > 2) {
        consecutive_turn_count_ = 0;
        action.type = BipedAction::WALK_FORWARD;
        action.steps = 4 + (esp_random() % 3);
        action.duration_ms = action.steps * GAIT_DEFAULT_SPEED_MS;
        action.comment = "我要往前走走了";
        return action;
    }

    // 3. 随机选择行为 (权重偏向行走)
    int choice = esp_random() % 100;
    if (choice < 60) {
        // 行走 (前/后)
        bool forward = (esp_random() % 3) != 0;  // 2/3概率前进
        action.type = forward ? BipedAction::WALK_FORWARD : BipedAction::WALK_BACKWARD;
        action.steps = 2 + (esp_random() % 4);
        action.duration_ms = action.steps * GAIT_DEFAULT_SPEED_MS;
        size_t n = sizeof(kForwardComments) / sizeof(kForwardComments[0]);
        action.comment = PickRandom(kForwardComments, n);
        consecutive_turn_count_ = 0;
    } else if (choice < 85) {
        // 转向
        bool turn_right = (esp_random() % 2) == 0;
        action.type = turn_right ? BipedAction::TURN_RIGHT : BipedAction::TURN_LEFT;
        action.steps = 2 + (esp_random() % 3);
        action.duration_ms = action.steps * GAIT_DEFAULT_SPEED_MS;
        size_t n = sizeof(kTurnLeftComments) / sizeof(kTurnLeftComments[0]);
        action.comment = PickRandom(kTurnLeftComments, n);
        consecutive_turn_count_++;
    } else if (choice < 95) {
        // 摇头
        action.type = BipedAction::SHAKE_HEAD;
        action.steps = 0;
        action.duration_ms = 1500;
        size_t n = sizeof(kShakeHeadComments) / sizeof(kShakeHeadComments[0]);
        action.comment = PickRandom(kShakeHeadComments, n);
        consecutive_turn_count_ = 0;
    } else {
        // 鞠躬
        action.type = BipedAction::BOW;
        action.steps = 0;
        action.duration_ms = 1500;
        size_t n = sizeof(kBowComments) / sizeof(kBowComments[0]);
        action.comment = PickRandom(kBowComments, n);
        consecutive_turn_count_ = 0;
    }
    return action;
}

BipedAction BipedBehavior::MakeAiDecision_(const std::string& env_context) {
    if (ask_ai_cb_ == nullptr) {
        return MakeLocalDecision_();
    }
    std::string ai_response = ask_ai_cb_(env_context);
    // 简化解析: 返回的字符串格式 "<action>:<comment>"
    BipedAction action = {};
    size_t colon = ai_response.find(':');
    std::string act_str = (colon != std::string::npos) ? ai_response.substr(0, colon) : ai_response;
    std::string comment = (colon != std::string::npos) ? ai_response.substr(colon + 1) : "";

    if (act_str == "forward") {
        action.type = BipedAction::WALK_FORWARD;
        action.steps = 3;
    } else if (act_str == "backward") {
        action.type = BipedAction::WALK_BACKWARD;
        action.steps = 2;
    } else if (act_str == "left") {
        action.type = BipedAction::TURN_LEFT;
        action.steps = 3;
    } else if (act_str == "right") {
        action.type = BipedAction::TURN_RIGHT;
        action.steps = 3;
    } else if (act_str == "shake") {
        action.type = BipedAction::SHAKE_HEAD;
    } else if (act_str == "bow") {
        action.type = BipedAction::BOW;
    } else {
        action.type = BipedAction::NO_ACTION;
    }
    action.comment = comment;
    action.duration_ms = action.steps > 0 ? action.steps * GAIT_DEFAULT_SPEED_MS : 1500;
    return action;
}

void BipedBehavior::ExecuteAction_(const BipedAction& action) {
    switch (action.type) {
        case BipedAction::WALK_FORWARD:
            controller_->Walk(BipedDirection::FORWARD, action.steps, GAIT_DEFAULT_SPEED_MS);
            break;
        case BipedAction::WALK_BACKWARD:
            controller_->Walk(BipedDirection::BACKWARD, action.steps, GAIT_DEFAULT_SPEED_MS);
            break;
        case BipedAction::TURN_LEFT:
            controller_->Turn(BipedDirection::LEFT, action.steps, GAIT_DEFAULT_SPEED_MS);
            break;
        case BipedAction::TURN_RIGHT:
            controller_->Turn(BipedDirection::RIGHT, action.steps, GAIT_DEFAULT_SPEED_MS);
            break;
        case BipedAction::SHAKE_HEAD:
            controller_->ShakeHead();
            break;
        case BipedAction::BOW:
            controller_->Bow();
            break;
        case BipedAction::SIT:
            controller_->Sit();
            break;
        case BipedAction::STAND:
        case BipedAction::NO_ACTION:
        default:
            break;
    }
}

void BipedBehavior::TaskLoop_() {
    SetState_(BipedBehaviorState::IDLE);
    while (running_) {
        // 阶段1: IDLE - 等待随机时间 + 原地小动作
        SetState_(BipedBehaviorState::IDLE);
        uint32_t wait_ms = immediate_trigger_ ? 0 : RandomInRange(min_interval_ms_, max_interval_ms_);
        immediate_trigger_ = false;

        // 分段等待, 每500ms做一次小动作
        const uint32_t segment_ms = 500;
        uint32_t elapsed = 0;
        bool small_motion_done = false;
        while (elapsed < wait_ms && running_ && !immediate_trigger_) {
            vTaskDelay(pdMS_TO_TICKS(segment_ms));
            elapsed += segment_ms;
            if (!small_motion_done && elapsed > wait_ms / 2) {
                // 中间做一次小动作 (像是环顾四周)
                controller_->ShakeHead();
                small_motion_done = true;
            }
        }
        if (!running_) break;

        // 阶段2: DECIDING - 决定动作
        SetState_(BipedBehaviorState::DECIDING);

        // 收集环境上下文
        char env_buf[128];
        float dist = sensor_ != nullptr ? sensor_->GetDistanceCm() : -1.0f;
        if (dist > 0) {
            snprintf(env_buf, sizeof(env_buf), "前方面障碍距离: %.1f厘米", dist);
        } else {
            snprintf(env_buf, sizeof(env_buf), "前方面无障碍检测");
        }
        std::string env_context = env_buf;

        // 加入最近记忆摘要 (帮助AI了解过去)
        if (memory_ != nullptr && memory_->Size() > 0) {
            auto recent = memory_->GetRecent(3);
            for (size_t i = 0; i < recent.size(); i++) {
                env_context += "; 过去: " + recent[i];
            }
        }

        // 决定: 30%概率问云端AI
        BipedAction action;
        bool use_ai = (esp_random() % 100) < 30;
        if (use_ai && ask_ai_cb_ != nullptr) {
            action = MakeAiDecision_(env_context);
        } else {
            action = MakeLocalDecision_();
        }

        // 阶段3: SPEAKING + MOVING - 语音 + 动作
        if (!action.comment.empty() && speak_cb_ != nullptr) {
            SetState_(BipedBehaviorState::SPEAKING);
            speak_cb_(action.comment);
            // 语音开始后才动作 (模拟人类边说边做)
            vTaskDelay(pdMS_TO_TICKS(300));
        }

        SetState_(BipedBehaviorState::MOVING);
        ExecuteAction_(action);

        // 阶段4: 记忆存储
        if (memory_ != nullptr && !action.comment.empty()) {
            char mem_buf[BIPED_MEMORY_ENTRY_MAX_LEN];
            snprintf(mem_buf, sizeof(mem_buf), "环境:%s 行动:%s",
                env_context.c_str(), action.comment.c_str());
            memory_->AddEntry(mem_buf);
        }

        // 给舵机一点时间稳定
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    SetState_(BipedBehaviorState::IDLE);
}