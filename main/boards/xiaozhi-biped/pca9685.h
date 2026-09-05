/*
    PCA9685 - 16路PWM/舵机驱动板
    通过I2C控制16路PWM输出
*/

#ifndef PCA9685_H
#define PCA9685_H

#include <driver/i2c_master.h>
#include <esp_err.h>
#include <stdint.h>

class Pca9685 {
public:
    Pca9685(i2c_master_bus_handle_t i2c_bus, uint8_t address);
    ~Pca9685();

    // 初始化芯片 (设置PWM频率, 复位等)
    esp_err_t Begin(uint16_t freq_hz = 50);

    // 设置PWM脉冲 (0-4095对应0%-100%占空比)
    esp_err_t SetPwm(uint8_t channel, uint16_t on, uint16_t off);

    // 设置舵机角度 (0-180度)
    esp_err_t SetServoAngle(uint8_t channel, uint8_t angle);

    // 设置舵机脉冲宽度 (微秒)
    esp_err_t SetServusPulseUs(uint8_t channel, uint16_t pulse_us);

    // 让所有通道停止输出
    esp_err_t Sleep();
    esp_err_t WakeUp();

private:
    i2c_master_bus_handle_t i2c_bus_;
    i2c_master_dev_handle_t i2c_dev_;
    uint8_t address_;
    uint16_t freq_hz_;

    void WriteReg_(uint8_t reg, uint8_t value);
    void WriteRegs_(uint8_t reg, const uint8_t* buffer, size_t length);
    uint8_t ReadReg_(uint8_t reg);
};

#endif  // PCA9685_H