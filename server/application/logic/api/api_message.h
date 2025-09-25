#ifndef __LOGIC_API_MESSAGES_H__
#define __LOGIC_API_MESSAGES_H__

#include "api_common.h"
#include "api_types.h"
#include "db_pool.h"
#include <memory>
#include <mutex>

// const when compiling
const constexpr size_t k_message_batch_size = 20;     // the max count of history messages

// 消息存储管理类
class MessageStorageManager {
public:
    // 获取单例实例
    static MessageStorageManager& getInstance();
    
    // 禁用拷贝构造和赋值操作
    MessageStorageManager(const MessageStorageManager&) = delete;
    MessageStorageManager& operator=(const MessageStorageManager&) = delete;
    
    // 初始化存储系统
    bool init(const string& config_file);
    
    // 获取房间历史消息
    int getRoomHistory(Room& room, MessageBatch& message_batch, 
                      const int msg_count = k_message_batch_size);
    
    // 存储单条消息到Redis缓存
    bool storeMessageToCache(const string& room_id, const Message& msg);
    
    // 批量存储消息到Redis缓存
    int storeMessagesToCache(const string& room_id, std::vector<Message>& msgs);
    
    // 存储单条消息到MySQL数据库
    bool storeMessageToDB(const string& room_id, const Message& msg);
    
    // 批量存储消息到MySQL数据库
    bool storeMessagesToDB(const string& room_id, const std::vector<Message>& messages);
    
    // 工具函数
    string serializeMessageToJson(const Message& msg);
    string generateMessageId();
    
    // 获取初始化状态
    bool isInitialized() const { return initialized_; }

private:
    MessageStorageManager() = default;
    ~MessageStorageManager() = default;
    
    // 内部状态
    bool initialized_ = false;
    mutable std::mutex mutex_;  // 线程安全保护
    
    // 配置信息
    string config_file_;
    
    // 内部辅助函数
    CDBConn* getDBConnection();
    CacheConn* getCacheConnection();
    void releaseDBConnection(CDBConn* conn);
    void releaseCacheConnection(CacheConn* conn);
};


#endif