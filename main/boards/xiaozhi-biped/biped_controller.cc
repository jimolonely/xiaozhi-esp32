/*
    双足机器人 - 步行控制器实现
    步态:
        Phase 0: 左腿向前摆 (右腿支撑), 左脚抬起
        Phase 1: 左腿落地 (右腿支撑)
        Phase 2: 右腿向前摆 (左腿支撑), 右脚抬起
        Phase 3: 右腿落地 (左腿支撑)
*/

#include "biped_controller.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <algorithm>

#define TAG "BipedController"

BipedController::BipedController(Pca9685* pca9685,
                                 uint8_t left_hip_ch,
                                 uint8_t left_knee_ch,
                                 uint8_t right_hip_ch,
                                 uint8_t right_knee_ch)
    : pca9685_(pca9685),
      left_hip_ch_(left_hip_ch),
      left_knee_ch_(left_knee_ch),
      right_hip_ch_(right_hip_ch),
      right_knee_ch_(right_knee_ch) {
    for (int i = 0; i < 4; i++) {
        servos_[i].current_angle = 90;
        servos_[i].target_angle = 90;
    }
}

BipedController::~BipedController() {}

void BipedController::Begin() {
    if (pca9685_ == nullptr) {
        ESP_LOGE(TAG, "PCA9685未初始化");
        return;
    }
    Home();
}

void BipedController::SetServoAngle_(int servo_index, uint8_t angle) {
    uint8_t channels[4] = {left_hip_ch_, left_knee_ch_, right_hip_ch_, right_knee_ch_};
    if (servo_index < 0 || servo_index >= 4) return;
    pca9685_->SetServoAngle(channels[servo_index], angle);
    servos_[servo_index].current_angle = angle;
    servos_[servo_index].target_angle = angle;
}

void BipedController::Interpolate_(int servo_index, uint8_t target, uint32_t duration_ms) {
    uint8_t start = servos_[servo_index].current_angle;
    if (start == target) {
        servos_[servo_index].target_angle = target;
        return;
    }
    if (duration_ms == 0) {
        SetServoAngle_(servo_index, target);
        return;
    }
    const uint32_t steps = std::max<uint32_t>(1, duration_ms / 20);
    int diff = static_cast<int>(target) - static_cast<int>(start);
    for (uint32_t i = 1; i <= steps; i++) {
        uint8_t angle = start + (diff * i) / steps;
        SetServoAngle_(servo_index, angle);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    servos_[servo_index].target_angle = target;
}

void BipedController::SetLeftHip(uint8_t angle) { SetServoAngle_(0, angle); }
void BipedController::SetLeftKnee(uint8_t angle) { SetServoAngle_(1, angle); }
void BipedController::SetRightHip(uint8_t angle) { SetServoAngle_(2, angle); }
void BipedController::SetRightKnee(uint8_t angle) { SetServoAngle_(3, angle); }

void BipedController::SetLeftLeg(uint8_t hip_angle, uint8_t knee_angle, uint32_t duration_ms) {
    Interpolate_(0, hip_angle, duration_ms);
    Interpolate_(1, knee_angle, duration_ms);
}

void BipedController::SetRightLeg(uint8_t hip_angle, uint8_t knee_angle, uint32_t duration_ms) {
    Interpolate_(2, hip_angle, duration_ms);
    Interpolate_(3, knee_angle, duration_ms);
}

void BipedController::Home() {
    SetLeftLeg(kHipCenterAngle, kKneeCenterAngle, 200);
    SetRightLeg(kHipCenterAngle, kKneeCenterAngle, 200);
}

void BipedController::Stand() {
    Home();
}

void BipedController::Sit() {
    // 双腿微弯, 类似坐下
    SetLeftLeg(kHipCenterAngle - 15, kKneeCenterAngle + 20, 400);
    SetRightLeg(kHipCenterAngle + 15, kKneeCenterAngle + 20, 400);
}

void BipedController::Bow() {
    // 髋关节向前倾
    SetLeftLeg(kHipCenterAngle + 25, kKneeCenterAngle - 10, 500);
    SetRightLeg(kHipCenterAngle - 25, kKneeCenterAngle - 10, 500);
    vTaskDelay(pdMS_TO_TICKS(800));
    Home();
}

void BipedController::ShakeHead() {
    // 没有脖子舵机, 用髋关节的左右摆动模拟
    for (int i = 0; i < 3; i++) {
        SetLeftHip(kHipCenterAngle - 15);
        SetRightHip(kHipCenterAngle + 15);
        vTaskDelay(pdMS_TO_TICKS(300));
        SetLeftHip(kHipCenterAngle + 15);
        SetRightHip(kHipCenterAngle - 15);
        vTaskDelay(pdMS_TO_TICKS(300));
    }
    SetLeftHip(kHipCenterAngle);
    SetRightHip(kHipCenterAngle);
}

void BipedController::Step(int step_index, uint32_t step_duration_ms) {
    // 步态相位 0-3
    // 0: 左腿后摆准备 (右腿支撑)
    // 1: 左腿前摆 (右腿支撑)
    // 2: 右腿后摆准备 (左腿支撑)
    // 3: 右腿前摆 (左腿支撑)

    switch (step_index % 4) {
        case 0:
            // 左腿抬起到最高, 准备前移
            SetLeftLeg(kHipCenterAngle - kHipSwingRange, kKneeCenterAngle + kKneeLiftRange,
                       step_duration_ms);
            SetRightLeg(kHipCenterAngle + kHipSwingRange / 2, kKneeCenterAngle,
                        step_duration_ms);
            break;
        case 1:
            // 左腿前移落地
            SetLeftLeg(kHipCenterAngle + kHipSwingRange, kKneeCenterAngle,
                       step_duration_ms);
            SetRightLeg(kHipCenterAngle + kHipSwingRange / 2, kKneeCenterAngle,
                        step_duration_ms);
            break;
        case 2:
            // 右腿抬起到最高, 准备前移
            SetRightLeg(kHipCenterAngle + kHipSwingRange, kKneeCenterAngle + kKneeLiftRange,
                        step_duration_ms);
            SetLeftLeg(kHipCenterAngle - kHipSwingRange / 2, kKneeCenterAngle,
                       step_duration_ms);
            break;
        case 3:
            // 右腿前移落地
            SetRightLeg(kHipCenterAngle - kHipSwingRange, kKneeCenterAngle,
                        step_duration_ms);
            SetLeftLeg(kHipCenterAngle - kHipSwingRange / 2, kKneeCenterAngle,
                       step_duration_ms);
            break;
    }
}

void BipedController::Walk(BipedDirection dir, int steps, uint32_t step_duration_ms) {
    if (steps <= 0) return;
    int phase = 0;
    int hip_offset = (dir == BipedDirection::BACKWARD) ? -kHipSwingRange : kHipSwingRange;

    for (int i = 0; i < steps; i++) {
        int p = phase % 4;
        switch (p) {
            case 0:
                SetLeftLeg(kHipCenterAngle - hip_offset,
                           kKneeCenterAngle + kKneeLiftRange, step_duration_ms);
                SetRightLeg(kHipCenterAngle + hip_offset / 2, kKneeCenterAngle,
                            step_duration_ms);
                break;
            case 1:
                SetLeftLeg(kHipCenterAngle + hip_offset, kKneeCenterAngle,
                           step_duration_ms);
                SetRightLeg(kHipCenterAngle + hip_offset / 2, kKneeCenterAngle,
                            step_duration_ms);
                break;
            case 2:
                SetRightLeg(kHipCenterAngle + hip_offset,
                            kKneeCenterAngle + kKneeLiftRange, step_duration_ms);
                SetLeftLeg(kHipCenterAngle - hip_offset / 2, kKneeCenterAngle,
                           step_duration_ms);
                break;
            case 3:
                SetRightLeg(kHipCenterAngle - hip_offset, kKneeCenterAngle,
                            step_duration_ms);
                SetLeftLeg(kHipCenterAngle - hip_offset / 2, kKneeCenterAngle,
                           step_duration_ms);
                break;
        }
        phase++;
    }
    Home();
}

void BipedController::Turn(BipedDirection dir, int steps, uint32_t step_duration_ms) {
    if (steps <= 0) return;
    int left_hip_offset = (dir == BipedDirection::LEFT) ? -kHipSwingRange : kHipSwingRange;
    int right_hip_offset = (dir == BipedDirection::LEFT) ? kHipSwingRange : -kHipSwingRange;

    for (int i = 0; i < steps; i++) {
        int p = i % 4;
        switch (p) {
            case 0:
                SetLeftLeg(kHipCenterAngle + left_hip_offset,
                           kKneeCenterAngle + kKneeLiftRange, step_duration_ms);
                SetRightLeg(kHipCenterAngle + right_hip_offset, kKneeCenterAngle,
                            step_duration_ms);
                break;
            case 1:
                SetLeftLeg(kHipCenterAngle - left_hip_offset, kKneeCenterAngle,
                           step_duration_ms);
                SetRightLeg(kHipCenterAngle + right_hip_offset, kKneeCenterAngle,
                            step_duration_ms);
                break;
            case 2:
                SetRightLeg(kHipCenterAngle - right_hip_offset,
                            kKneeCenterAngle + kKneeLiftRange, step_duration_ms);
                SetLeftLeg(kHipCenterAngle - left_hip_offset, kKneeCenterAngle,
                           step_duration_ms);
                break;
            case 3:
                SetRightLeg(kHipCenterAngle + right_hip_offset, kKneeCenterAngle,
                            step_duration_ms);
                SetLeftLeg(kHipCenterAngle - left_hip_offset, kKneeCenterAngle,
                           step_duration_ms);
                break;
        }
    }
    Home();
}