/*
    双足机器人 - 主控制器实现
*/

#include "biped_main_controller.h"
#include "application.h"
#include "config.h"
#include "mcp_server.h"
#include <esp_log.h>

#define TAG "BipedMain"

BipedMainController* g_biped_controller = nullptr;

BipedMainController::BipedMainController()
    : pca9685_(nullptr),
      ultrasonic_(nullptr),
      biped_(nullptr),
      memory_(nullptr),
      behavior_(nullptr),
      initialized_(false),
      autonomous_enabled_(false) {
    g_biped_controller = this;
}

BipedMainController::~BipedMainController() {
    End();
    if (g_biped_controller == this) {
        g_biped_controller = nullptr;
    }
}

void BipedMainController::Begin() {
    if (initialized_) return;

    // 1. 初始化PCA9685 (使用独立I2C bus)
    i2c_master_bus_config_t pca_bus_cfg = {};
    pca_bus_cfg.i2c_port = I2C_NUM_1;  // 与OLED/ESP32其他外设分离
    pca_bus_cfg.sda_io_num = PCA9685_I2C_SDA_PIN;
    pca_bus_cfg.scl_io_num = PCA9685_I2C_SCL_PIN;
    pca_bus_cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    pca_bus_cfg.glitch_ignore_cnt = 7;
    pca_bus_cfg.trans_queue_depth = 0;
    pca_bus_cfg.flags.enable_internal_pullup = 1;

    i2c_master_bus_handle_t pca_bus = nullptr;
    esp_err_t ret = i2c_new_master_bus(&pca_bus_cfg, &pca_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "PCA9685 I2C总线初始化失败: %s", esp_err_to_name(ret));
        return;
    }

    pca9685_ = new Pca9685(pca_bus, PCA9685_I2C_ADDR);
    if (pca9685_->Begin(BIPED_SERVO_FREQ_HZ) != ESP_OK) {
        ESP_LOGE(TAG, "PCA9685初始化失败");
        return;
    }

    // 2. 初始化超声传感器
    ultrasonic_ = new UltrasonicSensor(ULTRASONIC_TRIG_PIN, ULTRASONIC_ECHO_PIN);
    ultrasonic_->Begin();

    // 3. 初始化舵机控制器
    biped_ = new BipedController(pca9685_,
                                 SERVO_LEFT_HIP_CHANNEL,
                                 SERVO_LEFT_KNEE_CHANNEL,
                                 SERVO_RIGHT_HIP_CHANNEL,
                                 SERVO_RIGHT_KNEE_CHANNEL);
    biped_->Begin();

    // 4. 初始化记忆存储
    memory_ = new BipedMemory(BIPED_MEMORY_NAMESPACE, BIPED_MEMORY_MAX_ENTRIES);
    memory_->Init();

    // 5. 初始化行为状态机 (默认不启动, 等待外部启用)
    behavior_ = new BipedBehavior(biped_, ultrasonic_, memory_);
    behavior_->SetSpeakCallback([this](const std::string& text) { OnSpeak_(text); });
    behavior_->SetAskAiCallback([this](const std::string& ctx) { return OnAskAi_(ctx); });

    initialized_ = true;
    ESP_LOGI(TAG, "双足机器人初始化完成");
}

void BipedMainController::End() {
    if (behavior_ != nullptr) {
        behavior_->Stop();
        delete behavior_;
        behavior_ = nullptr;
    }
    if (memory_ != nullptr) {
        delete memory_;
        memory_ = nullptr;
    }
    if (biped_ != nullptr) {
        delete biped_;
        biped_ = nullptr;
    }
    if (ultrasonic_ != nullptr) {
        delete ultrasonic_;
        ultrasonic_ = nullptr;
    }
    if (pca9685_ != nullptr) {
        delete pca9685_;
        pca9685_ = nullptr;
    }
    initialized_ = false;
}

void BipedMainController::SetAutonomousEnabled(bool enabled) {
    if (!initialized_) return;
    if (enabled == autonomous_enabled_) return;

    autonomous_enabled_ = enabled;
    if (enabled) {
        behavior_->Begin();
        behavior_->TriggerImmediateDecision();
    } else {
        behavior_->Stop();
        if (biped_ != nullptr) biped_->Stand();
    }
    ESP_LOGI(TAG, "自主行为 %s", enabled ? "启用" : "禁用");
}

void BipedMainController::OnSpeak_(const std::string& text) {
    if (text.empty()) return;
    // 通过Application调度到主循环 (避免任务阻塞)
    auto app = &Application::GetInstance();
    // 实际TTS输出在protocol层处理, 这里通过MCP或事件广播
    // 简化: 推送为系统消息
    // app->PostSystemMessage(text);
    ESP_LOGI(TAG, "TTS输出: %s", text.c_str());
}

std::string BipedMainController::OnAskAi_(const std::string& context) {
    // 通过 MCP 调用让 AI 决定下一步动作
    // 简化: 这里返回空, 让本地决策生效
    // 真实实现应通过应用层向云端发请求
    ESP_LOGD(TAG, "询问AI: %s", context.c_str());
    return std::string();
}

void BipedMainController::RegisterMcpTools() {
    if (!initialized_) return;
    auto& mcp = McpServer::GetInstance();

    // 启用/禁用自主行为
    mcp.AddTool(
        "self.biped.set_autonomous",
        "启用或禁用自主随机游走行为。启用后小智将每隔5-30秒随机决定行走/转向, "
        "并发出语音评论; 禁用后只响应直接指令。",
        PropertyList({
            Property("enabled", kPropertyTypeBoolean, true),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            bool enabled = properties["enabled"].value<bool>();
            SetAutonomousEnabled(enabled);
            return true;
        });

    // 立即触发一次决策
    mcp.AddTool(
        "self.biped.do_action",
        "立即决定并执行一个动作 (不用等待随机间隔)。",
        PropertyList(),
        [this](const PropertyList& properties) -> ReturnValue {
            (void)properties;
            if (behavior_ != nullptr) {
                behavior_->TriggerImmediateDecision();
            }
            return true;
        });

    // 直接行走
    mcp.AddTool(
        "self.biped.walk",
        "让机器人向某个方向走几步。direction: forward/backward/left/right; "
        "steps: 步数(1-10)。",
        PropertyList({
            Property("direction", kPropertyTypeString, "forward"),
            Property("steps", kPropertyTypeInteger, 3, 1, 10),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            std::string dir = properties["direction"].value<std::string>();
            int steps = properties["steps"].value<int>();
            BipedDirection d = BipedDirection::FORWARD;
            if (dir == "backward") d = BipedDirection::BACKWARD;
            else if (dir == "left") d = BipedDirection::LEFT;
            else if (dir == "right") d = BipedDirection::RIGHT;
            biped_->Walk(d, steps, GAIT_DEFAULT_SPEED_MS);
            return true;
        });

    // 直接转向
    mcp.AddTool(
        "self.biped.turn",
        "让机器人原地转向。direction: left/right; steps: 步数(1-10)。",
        PropertyList({
            Property("direction", kPropertyTypeString, "left"),
            Property("steps", kPropertyTypeInteger, 3, 1, 10),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            std::string dir = properties["direction"].value<std::string>();
            int steps = properties["steps"].value<int>();
            BipedDirection d = (dir == "right") ? BipedDirection::RIGHT : BipedDirection::LEFT;
            biped_->Turn(d, steps, GAIT_DEFAULT_SPEED_MS);
            return true;
        });

    // 站/坐/鞠躬
    mcp.AddTool(
        "self.biped.gesture",
        "执行一个简单动作: stand/sit/bow/shake_head。",
        PropertyList({
            Property("gesture", kPropertyTypeString, "stand"),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            std::string g = properties["gesture"].value<std::string>();
            if (g == "stand") biped_->Stand();
            else if (g == "sit") biped_->Sit();
            else if (g == "bow") biped_->Bow();
            else if (g == "shake_head") biped_->ShakeHead();
            else return "未知动作";
            return true;
        });

    // 获取距离
    mcp.AddTool(
        "self.biped.get_distance",
        "获取前方障碍物的距离 (cm), -1表示无障碍/超时。",
        PropertyList(),
        [this](const PropertyList& properties) -> ReturnValue {
            (void)properties;
            float dist = ultrasonic_->GetDistanceCm();
            char buf[32];
            snprintf(buf, sizeof(buf), "{\"distance_cm\":%.1f}", dist);
            return std::string(buf);
        });

    // 记忆管理
    mcp.AddTool(
        "self.biped.add_memory",
        "添加一条文本记忆(由AI总结)。",
        PropertyList({
            Property("entry", kPropertyTypeString, ""),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            std::string entry = properties["entry"].value<std::string>();
            if (entry.empty()) return "记忆为空";
            memory_->AddEntry(entry);
            return true;
        });

    mcp.AddTool(
        "self.biped.get_memories",
        "获取最近N条记忆 (按时间顺序)。",
        PropertyList({
            Property("count", kPropertyTypeInteger, 10, 1, 64),
        }),
        [this](const PropertyList& properties) -> ReturnValue {
            int count = properties["count"].value<int>();
            auto mems = memory_->GetRecent(count);
            std::string result = "[";
            for (size_t i = 0; i < mems.size(); i++) {
                if (i > 0) result += ",";
                result += "\"" + mems[i] + "\"";
            }
            result += "]";
            return result;
        });

    mcp.AddTool(
        "self.biped.clear_memories",
        "清空所有记忆。",
        PropertyList(),
        [this](const PropertyList& properties) -> ReturnValue {
            (void)properties;
            memory_->Clear();
            return true;
        });

    // 状态查询
    mcp.AddTool(
        "self.biped.get_status",
        "获取当前行为状态: idle/deciding/moving/speaking。",
        PropertyList(),
        [this](const PropertyList& properties) -> ReturnValue {
            (void)properties;
            const char* state_name = "unknown";
            switch (behavior_->GetState()) {
                case BipedBehaviorState::IDLE: state_name = "idle"; break;
                case BipedBehaviorState::DECIDING: state_name = "deciding"; break;
                case BipedBehaviorState::MOVING: state_name = "moving"; break;
                case BipedBehaviorState::SPEAKING: state_name = "speaking"; break;
            }
            return std::string(state_name);
        });
}