/*
    HC-SR04 超声波测距传感器
*/

#ifndef ULTRASONIC_SENSOR_H
#define ULTRASONIC_SENSOR_H

#include <driver/gpio.h>
#include <esp_err.h>

class UltrasonicSensor {
public:
    UltrasonicSensor(gpio_num_t trig_pin, gpio_num_t echo_pin);
    ~UltrasonicSensor();

    esp_err_t Begin();

    // 获取距离 (cm), 超时返回 -1
    float GetDistanceCm(uint32_t timeout_us = 30000);

private:
    gpio_num_t trig_pin_;
    gpio_num_t echo_pin_;

    esp_err_t SendTriggerPulse_();
    uint32_t MeasureEchoPulseUs_(uint32_t timeout_us);
};

#endif  // ULTRASONIC_SENSOR_H