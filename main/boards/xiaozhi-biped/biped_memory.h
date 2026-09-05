/*
    双足机器人记忆存储
    使用 NVS (Non-Volatile Storage) 存储记忆条目
    每条记忆是简短的摘要字符串, 由云端AI生成
*/

#ifndef BIPED_MEMORY_H
#define BIPED_MEMORY_H

#include <cstdint>
#include <string>
#include <vector>

class BipedMemory {
public:
    BipedMemory(const char* ns = "biped_mem", uint16_t max_entries = 64);
    ~BipedMemory();

    esp_err_t Init();

    // 添加一条记忆 (FIFO, 满则覆盖最旧的)
    esp_err_t AddEntry(const std::string& entry);

    // 获取所有记忆 (按时间顺序, 最旧的在前)
    std::vector<std::string> GetAllEntries() const;

    // 获取最近N条记忆
    std::vector<std::string> GetRecent(size_t count) const;

    // 获取记忆条数
    size_t Size() const;

    // 清空所有记忆
    esp_err_t Clear();

    // 获取当前序列号 (用于递增索引)
    uint16_t GetNextIndex() const;

private:
    std::string namespace_;
    uint16_t max_entries_;
    uint16_t next_index_;

    std::string MakeKey_(uint16_t index) const;
};

#endif  // BIPED_MEMORY_H