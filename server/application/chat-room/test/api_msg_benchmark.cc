#include <chrono>
#include <random>
#include <thread>
#include <vector>
#include <atomic>
#include <muduo/base/Logging.h>
#include "db_pool.h"
#include "cache_pool.h"
#include "api_message.h"

#define k_message_batch_size 30   //一次读取30条消息，  客户端测试读取的时候也一次读取30条消息


extern bool initPools();
// 性能统计结构
struct BenchmarkStats {
    std::atomic<int64_t> total_messages{0};
    std::atomic<int64_t> total_duration_ms{0};
    std::atomic<int> error_count{0};
    
    void reset() {
        total_messages = 0;
        total_duration_ms = 0;
        error_count = 0;
    }
    
    void print(const std::string& test_name) {
        if (total_messages == 0) return;
        
        LOG_INFO << "=== " << test_name << " 测试结果 ===";
        LOG_INFO << "总消息数: " << total_messages;
        LOG_INFO << "总耗时: " << total_duration_ms << "ms";
        LOG_INFO << "平均每条消息耗时: " << (float)total_duration_ms / total_messages << "ms";
        LOG_INFO << "每秒处理消息数(TPS): " << (float)total_messages * 1000 / total_duration_ms;
        LOG_INFO << "错误次数: " << error_count;
        std::cout << "=================================\n\n" << std::endl;

    }
};

// 清除房间消息
void clearRoomMessages(const std::string& room_id) {
    CacheManager *cache_manager = CacheManager::getInstance();
    CacheConn *cache_conn = cache_manager->GetCacheConn("msg");
    if (!cache_conn) {
        LOG_ERROR << "获取Redis连接失败";
        return;
    }
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    // 删除房间的所有消息
    bool ret = cache_conn->Del(room_id);
    if (!ret) {
        LOG_ERROR << "清除房间 " << room_id << " 的消息失败";
    } else {
        LOG_INFO << "清除房间 " << room_id << " 的消息成功";
    }
}

// 生成随机字符串
std::string generateRandomString(int length) {
    
    static const char charset[] = "0123456789"
        "abcdefghijklmnopqrstuvwxyz"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
    return charset;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, sizeof(charset) - 2);
    
    std::string str;
    str.reserve(length);
    for (int i = 0; i < length; ++i) {
        str += charset[dis(gen)];
    }
    return str;
}

static const std::string msg_content = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOP"
            "QRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ012"
            "3456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
// 生成测试消息
Message generateTestMessage(int64_t user_id) {
    
    Message msg;
    msg.content = msg_content;
    msg.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
    msg.user_id = user_id;
    msg.username = "test_user_" + std::to_string(msg.user_id);
    return msg;
}

// 存储消息的工作线程函数
void storeMessageWorker(int thread_id,
                       int messages_per_thread,
                       BenchmarkStats& stats) {
    int64_t user_id = 0;
    // 使用线程ID作为房间ID
    std::string room_id = "room_" + std::to_string(thread_id);
    LOG_INFO << "线程 " << thread_id << " 开始存储测试, 房间ID: " << room_id;
   
    for (int i = 0; i < messages_per_thread; i ++) {
        std::vector<Message> msgs;
        int current_batch = 1;// 一次只存储一条消息
        
        // 生成一批消息
        for (int j = 0; j < current_batch; ++j) {
            msgs.push_back(generateTestMessage(user_id++));
        }
        
        // 存储消息
        int ret = ApiStoreMessage(room_id, msgs);
                
        if (ret != 0) {
            stats.error_count++;
            LOG_ERROR << "存储消息失败  " << " in room " << room_id;
            continue;
        }
    }
    stats.total_messages += messages_per_thread;
   
}

// 读取消息的工作线程函数
void getRoomHistoryWorker(int thread_id,
                         int messages_per_thread,
                         BenchmarkStats& stats) {
    // 使用线程ID作为房间ID
    std::string room_id = "room_"  + std::to_string(thread_id);
    LOG_INFO << "线程 " << thread_id << " 开始读取测试, 房间ID: " << room_id;

    Room room;
    room.room_id = room_id;
    room.history_last_message_id = ""; // 第一次从最新的开始读取
    int read_count = messages_per_thread/k_message_batch_size + 1;  // 计算需要读取的批次数
    for (int i = 0; i < read_count; ++i) {   
        MessageBatch message_batch;
        int ret = ApiGetRoomHistory(room, message_batch, k_message_batch_size);  //每次读取30条消息
        
        if (ret != 0) {
            stats.error_count++;
            LOG_ERROR << "读取消息失败 at iteration " << i << " in room " << room_id;
            break;
        }       
        stats.total_messages += message_batch.messages.size();  //每次读取的消息数量
    }
}

// 执行存储性能测试
void benchmarkStoreMessage(int messages_per_thread,
                          int thread_count) {
    LOG_INFO << "开始消息存储性能测试 (线程数: " << thread_count << ")";
    
    // 清除所有测试房间的消息
    for (int i = 0; i < thread_count; ++i) {
        std::string room_id = "room_" + std::to_string(i);
        clearRoomMessages(room_id);
    }
     // 等待1秒确保清除操作完成
    std::this_thread::sleep_for(std::chrono::seconds(1));

    BenchmarkStats stats;
    std::vector<std::thread> threads;
    
    auto start = std::chrono::high_resolution_clock::now();
    // 创建工作线程
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(storeMessageWorker,
                           i,  // 线程ID作为房间ID
                           messages_per_thread,
                           std::ref(stats));
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    
    stats.total_duration_ms += duration.count();
    stats.print("消息存储");
}

// 执行读取性能测试
void benchmarkGetRoomHistory(int messages_per_thread,
                           int thread_count) {
    LOG_INFO << "开始消息读取性能测试 (线程数: " << thread_count << ")";
    
    BenchmarkStats stats;
    std::vector<std::thread> threads;

     auto start = std::chrono::high_resolution_clock::now();
    // 创建工作线程
    for (int i = 0; i < thread_count; ++i) {
        threads.emplace_back(getRoomHistoryWorker,
                           i,  // 线程ID作为房间ID
                           messages_per_thread,
                           std::ref(stats));
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
   
    stats.total_duration_ms += duration.count();
    stats.print("消息读取");
}

int main(int argc, char* argv[]) {
    // 初始化连接池
    if (!initPools()) {
        LOG_ERROR << "初始化连接池失败";
        return 1;
    }
    
    LOG_INFO << "连接池初始化成功";
    
    // 默认测试参数
    int thread_count = 1;           // 默认线程数
    int messages_per_thread = 10000; // 默认每个线程的消息数
    
    // 解析命令行参数
    if (argc > 1) {
        thread_count = std::atoi(argv[1]);
    }
    if (argc > 2) {
        messages_per_thread = std::atoi(argv[2]);
    }
    
    LOG_INFO << "\n=== 开始性能测试 ===";
    LOG_INFO << "线程数: " << thread_count;
    LOG_INFO << "每线程消息数: " << messages_per_thread;
    
    // 存储测试
    benchmarkStoreMessage(messages_per_thread,  thread_count);

    benchmarkGetRoomHistory(messages_per_thread, thread_count);
    
    LOG_INFO << "=== 性能测试完成 ===\n";
    
    return 0;
} 