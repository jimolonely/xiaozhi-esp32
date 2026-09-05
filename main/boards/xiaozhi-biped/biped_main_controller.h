/*
    双足机器人 - 控制器 (统一封装 + MCP工具注册)
    提供 self.biped.* MCP 工具集
*/

#ifndef BIPED_CONTROLLER_MAIN_H
#define BIPED_CONTROLLER_MAIN_H

#include "biped_behavior.h"
#include "biped_controller.h"
#include "biped_memory.h"
#include "pca9685.h"
#include "ultrasonic_sensor.h"

class BipedMainController {
public:
    BipedMainController();
    ~BipedMainController();

    void Begin();
    void End();

    // 启用/禁用随机游走
    void SetAutonomousEnabled(bool enabled);
    bool IsAutonomousEnabled() const { return autonomous_enabled_; }

    // MCP 工具注册 (由 board 在初始化后调用)
    void RegisterMcpTools();

private:
    void OnSpeak_(const std::string& text);
    std::string OnAskAi_(const std::string& context);

    Pca9685* pca9685_;
    UltrasonicSensor* ultrasonic_;
    BipedController* biped_;
    BipedMemory* memory_;
    BipedBehavior* behavior_;

    bool initialized_;
    bool autonomous_enabled_;
};

extern BipedMainController* g_biped_controller;

#endif  // BIPED_CONTROLLER_MAIN_H