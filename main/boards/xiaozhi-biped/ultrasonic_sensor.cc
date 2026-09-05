/*
    HC-SR04 超声波测距传感器实现
*/

#include "ultrasonic_sensor.h"
#include <esp_log.h>
#include <esp_timer.h>
#include <rom/ets_sys.h>

#define TAG "Ultrasonic"

UltrasonicSensor::UltrasonicSensor(gpio_num_t trig_pin, gpio_num_t echo_pin)
    : trig_pin_(trig_pin), echo_pin_(echo_pin) {}

UltrasonicSensor::~UltrasonicSensor() {}

esp_err_t UltrasonicSensor::Begin() {
    gpio_config_t trig_config = {};
    trig_config.pin_bit_mask = (1ULL << trig_pin_);
    trig_config.mode = GPIO_MODE_OUTPUT;
    trig_config.pull_up_en = GPIO_PULLUP_DISABLE;
    trig_config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    trig_config.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&trig_config));
    gpio_set_level(trig_pin_, 0);

    gpio_config_t echo_config = {};
    echo_config.pin_bit_mask = (1ULL << echo_pin_);
    echo_config.mode = GPIO_MODE_INPUT;
    echo_config.pull_up_en = GPIO_PULLUP_DISABLE;
    echo_config.pull_down_en = GPIO_PULLDOWN_ENABLE;
    echo_config.intr_type = GPIO_INTR_DISABLE;
    ESP_ERROR_CHECK(gpio_config(&echo_config));

    return ESP_OK;
}

esp_err_t UltrasonicSensor::SendTriggerPulse_() {
    gpio_set_level(trig_pin_, 0);
    ets_delay_us(2);
    gpio_set_level(trig_pin_, 1);
    ets_delay_us(10);
    gpio_set_level(trig_pin_, 0);
    return ESP_OK;
}

uint32_t UltrasonicSensor::MeasureEchoPulseUs_(uint32_t timeout_us) {
    uint32_t start_us = esp_timer_get_time();
    // 等待echo变高 (起始)
    while (gpio_get_level(echo_pin_) == 0) {
        if ((esp_timer_get_time() - start_us) > timeout_us) return 0;
    }
    uint32_t echo_start_us = esp_timer_get_time();

    // 等待echo变低 (结束)
    while (gpio_get_level(echo_pin_) == 1) {
        if ((esp_timer_get_time() - echo_start_us) > timeout_us) return 0;
    }
    uint32_t echo_end_us = esp_timer_get_time();

    return echo_end_us - echo_start_us;
}

float UltrasonicSensor::GetDistanceCm(uint32_t timeout_us) {
    SendTriggerPulse_();
    // 等待传感器响应 (约20ms max)
    vTaskDelay(pdMS_TO_TICKS(20));
    uint32_t pulse_us = MeasureEchoPulseUs_(timeout_us);
    if (pulse_us == 0) return -1.0f;
    // 声速 0.0343 cm/us, 来回除2
    return pulse_us * 0.01715f;
}