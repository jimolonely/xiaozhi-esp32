/*
    双足机器人记忆存储实现
    使用 ESP-IDF NVS API
*/

#include "biped_memory.h"
#include <nvs.h>
#include <nvs_flash.h>
#include <esp_log.h>
#include <cstdio>
#include <cstring>

#define TAG "BipedMemory"

BipedMemory::BipedMemory(const char* ns, uint16_t max_entries)
    : namespace_(ns), max_entries_(max_entries), next_index_(0) {}

BipedMemory::~BipedMemory() {}

esp_err_t BipedMemory::Init() {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(namespace_.c_str(), NVS_READONLY, &handle);
    if (ret == ESP_OK) {
        uint16_t saved_index = 0;
        nvs_get_u16(handle, "_next_index", &saved_index);
        next_index_ = saved_index;
        nvs_close(handle);
        ESP_LOGI(TAG, "记忆已加载, 当前索引=%u", next_index_);
    } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
        // 第一次启动, 索引从0开始
        next_index_ = 0;
        ESP_LOGI(TAG, "首次初始化记忆, 索引从0开始");
        ret = ESP_OK;
    } else {
        ESP_LOGE(TAG, "打开NVS失败: %s", esp_err_to_name(ret));
        return ret;
    }
    return ESP_OK;
}

std::string BipedMemory::MakeKey_(uint16_t index) const {
    char buf[16];
    snprintf(buf, sizeof(buf), "m_%05u", index % max_entries_);
    return std::string(buf);
}

uint16_t BipedMemory::GetNextIndex() const {
    return next_index_;
}

esp_err_t BipedMemory::AddEntry(const std::string& entry) {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(namespace_.c_str(), NVS_READWRITE, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "打开NVS失败: %s", esp_err_to_name(ret));
        return ret;
    }

    // 写入环形索引
    std::string key = MakeKey_(next_index_);
    ret = nvs_set_str(handle, key.c_str(), entry.c_str());
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "写入记忆失败: %s", esp_err_to_name(ret));
        nvs_close(handle);
        return ret;
    }

    next_index_++;
    if (next_index_ >= 65535) next_index_ = 0;  // 防止溢出

    nvs_set_u16(handle, "_next_index", next_index_);
    nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGD(TAG, "已添加记忆[%u]: %s", next_index_ - 1, entry.c_str());
    return ESP_OK;
}

std::vector<std::string> BipedMemory::GetAllEntries() const {
    std::vector<std::string> result;
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(namespace_.c_str(), NVS_READONLY, &handle);
    if (ret != ESP_OK) return result;

    for (uint16_t i = 0; i < max_entries_; i++) {
        std::string key = MakeKey_(i);
        size_t length = 0;
        if (nvs_get_str(handle, key.c_str(), nullptr, &length) == ESP_OK && length > 0) {
            std::string value(length, '\0');
            if (nvs_get_str(handle, key.c_str(), value.data(), &length) == ESP_OK) {
                value.resize(length - 1);  // 去掉末尾的\0
                result.push_back(value);
            }
        }
    }

    nvs_close(handle);
    return result;
}

std::vector<std::string> BipedMemory::GetRecent(size_t count) const {
    std::vector<std::string> all = GetAllEntries();
    if (all.size() <= count) return all;
    return std::vector<std::string>(all.end() - count, all.end());
}

size_t BipedMemory::Size() const {
    return GetAllEntries().size();
}

esp_err_t BipedMemory::Clear() {
    nvs_handle_t handle;
    esp_err_t ret = nvs_open(namespace_.c_str(), NVS_READWRITE, &handle);
    if (ret != ESP_OK) return ret;

    for (uint16_t i = 0; i < max_entries_; i++) {
        std::string key = MakeKey_(i);
        nvs_erase_key(handle, key.c_str());
    }
    nvs_erase_key(handle, "_next_index");
    nvs_commit(handle);
    nvs_close(handle);
    next_index_ = 0;
    return ESP_OK;
}