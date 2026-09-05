/*
    PCA9685 - 16路PWM/舵机驱动板实现
*/

#include "pca9685.h"
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#define TAG "PCA9685"

// PCA9685 寄存器地址
#define PCA9685_REG_MODE1      0x00
#define PCA9685_REG_MODE2      0x01
#define PCA9685_REG_LED0_ON_L  0x06
#define PCA9685_REG_LED0_ON_H  0x07
#define PCA9685_REG_LED0_OFF_L 0x08
#define PCA9685_REG_LED0_OFF_H 0x09
#define PCA9685_REG_ALL_LED_ON_L  0xFA
#define PCA9685_REG_ALL_LED_ON_H  0xFB
#define PCA9685_REG_ALL_LED_OFF_L 0xFC
#define PCA9685_REG_ALL_LED_OFF_H 0xFD
#define PCA9685_REG_PRESCALE   0xFE

#define PCA9685_MODE1_RESTART  0x80
#define PCA9685_MODE1_SLEEP    0x10
#define PCA9685_MODE1_AI       0x20
#define PCA9685_MODE2_INVRT    0x10
#define PCA9685_MODE2_OUTDRV   0x04

// 内部时钟频率 25MHz
#define PCA9685_INTERNAL_OSC_HZ 25000000

Pca9685::Pca9685(i2c_master_bus_handle_t i2c_bus, uint8_t address)
    : i2c_bus_(i2c_bus),
      i2c_dev_(nullptr),
      address_(address),
      freq_hz_(50) {
    i2c_device_config_t dev_cfg = {};
    dev_cfg.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dev_cfg.device_address = address_;
    dev_cfg.scl_speed_hz = 400000;  // PCA9685支持最高1MHz
    esp_err_t ret = i2c_master_bus_add_device(i2c_bus_, &dev_cfg, &i2c_dev_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "添加I2C设备失败: %s", esp_err_to_name(ret));
    }
}

Pca9685::~Pca9685() {
    if (i2c_dev_ != nullptr) {
        i2c_master_bus_rm_device(i2c_dev_);
    }
}

void Pca9685::WriteReg_(uint8_t reg, uint8_t value) {
    if (i2c_dev_ == nullptr) return;
    uint8_t buffer[2] = {reg, value};
    i2c_master_transmit(i2c_dev_, buffer, 2, 100);
}

void Pca9685::WriteRegs_(uint8_t reg, const uint8_t* buffer, size_t length) {
    if (i2c_dev_ == nullptr) return;
    // 通过先发送寄存器地址+数据的方式批量写入
    uint8_t* tmp = new uint8_t[length + 1];
    tmp[0] = reg;
    memcpy(tmp + 1, buffer, length);
    i2c_master_transmit(i2c_dev_, tmp, length + 1, 100);
    delete[] tmp;
}

uint8_t Pca9685::ReadReg_(uint8_t reg) {
    if (i2c_dev_ == nullptr) return 0;
    uint8_t value = 0;
    i2c_master_transmit_receive(i2c_dev_, &reg, 1, &value, 1, 100);
    return value;
}

esp_err_t Pca9685::Begin(uint16_t freq_hz) {
    if (i2c_dev_ == nullptr) return ESP_FAIL;

    freq_hz_ = freq_hz;

    // 复位
    WriteReg_(PCA9685_REG_MODE1, PCA9685_MODE1_RESTART);
    vTaskDelay(pdMS_TO_TICKS(10));

    // 设置MODE2: 输出驱动为推挽 (适合舵机)
    WriteReg_(PCA9685_REG_MODE2, PCA9685_MODE2_OUTDRV);

    // 进入睡眠模式以修改预分频器
    uint8_t mode1 = ReadReg_(PCA9685_REG_MODE1);
    WriteReg_(PCA9685_REG_MODE1, (mode1 & ~PCA9685_MODE1_AI) | PCA9685_MODE1_SLEEP);
    vTaskDelay(pdMS_TO_TICKS(5));

    // 设置预分频器
    // prescale = round(osc_clock / (4096 * update_rate)) - 1
    uint8_t prescale = static_cast<uint8_t>(
        (PCA9685_INTERNAL_OSC_HZ / (4096UL * freq_hz_)) - 1);
    WriteReg_(PCA9685_REG_PRESCALE, prescale);
    vTaskDelay(pdMS_TO_TICKS(5));

    // 退出睡眠模式
    WriteReg_(PCA9685_REG_MODE1, PCA9685_MODE1_AI);
    vTaskDelay(pdMS_TO_TICKS(5));

    // 等待内部振荡器稳定
    vTaskDelay(pdMS_TO_TICKS(500));

    // 复位清除
    WriteReg_(PCA9685_REG_MODE1, PCA9685_MODE1_RESTART | PCA9685_MODE1_AI);

    ESP_LOGI(TAG, "PCA9685 初始化成功 (频率=%d Hz, 预分频=%d)", freq_hz_, prescale);
    return ESP_OK;
}

esp_err_t Pca9685::SetPwm(uint8_t channel, uint16_t on, uint16_t off) {
    if (channel > 15) return ESP_ERR_INVALID_ARG;
    if (i2c_dev_ == nullptr) return ESP_FAIL;

    uint8_t buffer[4];
    buffer[0] = on & 0xFF;
    buffer[1] = (on >> 8) & 0xFF;
    buffer[2] = off & 0xFF;
    buffer[3] = (off >> 8) & 0xFF;
    WriteRegs_(PCA9685_REG_LED0_ON_L + 4 * channel, buffer, 4);

    return ESP_OK;
}

esp_err_t Pca9685::SetServusPulseUs(uint8_t channel, uint16_t pulse_us) {
    if (channel > 15) return ESP_ERR_INVALID_ARG;
    // 一个PWM周期是 1e6 / freq_hz 微秒, 共4096份
    // 例如 50Hz -> 20000us周期, 每份约 4.88us
    uint32_t pulse = (pulse_us * 4096UL) / (1000000UL / freq_hz_);
    if (pulse > 4095) pulse = 4095;
    return SetPwm(channel, 0, static_cast<uint16_t>(pulse));
}

esp_err_t Pca9685::SetServoAngle(uint8_t channel, uint8_t angle) {
    if (angle > 180) angle = 180;
    // 500us -> 0度, 2500us -> 180度
    uint16_t pulse_us = 500 + (2000 * angle / 180);
    return SetServusPulseUs(channel, pulse_us);
}

esp_err_t Pca9685::Sleep() {
    if (i2c_dev_ == nullptr) return ESP_FAIL;
    uint8_t mode1 = ReadReg_(PCA9685_REG_MODE1);
    WriteReg_(PCA9685_REG_MODE1, mode1 | PCA9685_MODE1_SLEEP);
    return ESP_OK;
}

esp_err_t Pca9685::WakeUp() {
    if (i2c_dev_ == nullptr) return ESP_FAIL;
    uint8_t mode1 = ReadReg_(PCA9685_REG_MODE1);
    WriteReg_(PCA9685_REG_MODE1, mode1 & ~PCA9685_MODE1_SLEEP);
    vTaskDelay(pdMS_TO_TICKS(5));
    return ESP_OK;
}