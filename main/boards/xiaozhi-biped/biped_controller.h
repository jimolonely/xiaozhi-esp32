/*
    双足机器人 - 步行控制器
    每条腿有2个舵机: 髋关节(前后摆) + 膝关节(抬脚)
    2条腿共4个舵机, 通过PCA9685驱动
*/

#ifndef BIPED_CONTROLLER_H
#define BIPED_CONTROLLER_H

#include "pca9685.h"
#include <functional>
#include <string>

enum class BipedDirection {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    STOP,
};

class BipedController {
public:
    BipedController(Pca9685* pca9685,
                    uint8_t left_hip_ch,
                    uint8_t left_knee_ch,
                    uint8_t right_hip_ch,
                    uint8_t right_knee_ch);
    ~BipedController();

    void Begin();
    void Home();

    // 设置舵机角度 (单舵机直接控制)
    void SetLeftHip(uint8_t angle);
    void SetLeftKnee(uint8_t angle);
    void SetRightHip(uint8_t angle);
    void SetRightKnee(uint8_t angle);

    // 平滑过渡到目标角度 (在duration_ms内)
    void SetLeftLeg(uint8_t hip_angle, uint8_t knee_angle, uint32_t duration_ms = 0);
    void SetRightLeg(uint8_t hip_angle, uint8_t knee_angle, uint32_t duration_ms = 0);

    // 执行一步 - 步态基本单元
    // step_index 0-3 循环 (走路/转向的相位)
    void Step(int step_index, uint32_t step_duration_ms);

    // 执行动作: 走/转
    void Walk(BipedDirection dir, int steps, uint32_t step_duration_ms);
    void Turn(BipedDirection dir, int steps, uint32_t step_duration_ms);

    // 简单动作
    void Stand();
    void Sit();
    void Bow();
    void ShakeHead();

private:
    Pca9685* pca9685_;
    uint8_t left_hip_ch_;
    uint8_t left_knee_ch_;
    uint8_t right_hip_ch_;
    uint8_t right_knee_ch_;

    struct ServoState {
        uint8_t current_angle;
        uint8_t target_angle;
    };
    ServoState servos_[4];  // 顺序: LH, LK, RH, RK

    // 静息位置角度 (与config.h常量保持一致)
    static constexpr uint8_t kHipCenterAngle = 90;
    static constexpr uint8_t kKneeCenterAngle = 90;
    static constexpr uint8_t kHipSwingRange = 25;  // 髋摆动范围
    static constexpr uint8_t kKneeLiftRange = 25;   // 抬膝范围

    void SetServoAngle_(int servo_index, uint8_t angle);
    void Interpolate_(int servo_index, uint8_t target, uint32_t duration_ms);
};

#endif  // BIPED_CONTROLLER_H