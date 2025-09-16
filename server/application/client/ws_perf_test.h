#pragma once
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <string>
#include "muduo/base/Logging.h"

using namespace std;

// 性能测试类型
enum class PerfTestType {
    SEND_MESSAGE,    // 发送消息测试
    CREATE_ROOM,      // 创建房间测试
    REQUEST_HISTORY   // 请求历史消息测试
};

// 性能测试结果
struct PerfTestResult {
    uint64_t total_operations{0};  // 总操作数（消息数或房间数）
    uint64_t total_bytes{0};
    uint64_t total_time_ms{0};
    uint64_t avg_latency_ms{0};
    uint64_t max_latency_ms{0};
    uint64_t min_latency_ms{UINT64_MAX};
    uint64_t operations_per_second{0};  // 每秒操作数
    uint64_t kbps{0};
};

class WSPerfTest {
public:
    // 执行性能测试
    template<typename WSClient>
    static void runTest(WSClient& client, PerfTestType test_type,
                       const string& roomId, int count, int size, int thread_count) {
        string test_type_name;
        switch (test_type) {
            case PerfTestType::SEND_MESSAGE:
                test_type_name = "发送消息";
                break;
            case PerfTestType::CREATE_ROOM:
                test_type_name = "创建房间";
                break;
            case PerfTestType::REQUEST_HISTORY:
                test_type_name = "请求历史消息";
                break;
        }

        // 如果是历史消息测试，重置消息计数器
        if (test_type == PerfTestType::REQUEST_HISTORY) {
            client.resetHistoryCounter();
        }

        LOG_INFO << "\n开始性能测试:\n"
                << "测试类型: " << test_type_name << "\n"
                << "操作数量: " << count << "\n"
                << "大小/数量: " << size << (test_type == PerfTestType::REQUEST_HISTORY ? " 条" : " 字节") << "\n"
                << "并发线程: " << thread_count;

        // 性能统计
        std::atomic<uint64_t> total_operations{0};
        std::atomic<uint64_t> total_bytes{0};
        std::atomic<uint64_t> total_time_ms{0};
        std::atomic<uint64_t> max_latency{0};
        std::atomic<uint64_t> min_latency{UINT64_MAX};
        
        // 创建测试线程
        vector<thread> threads;
        std::atomic<int> ready_threads{0};
        std::atomic<bool> stop_test{false};
        
        auto thread_func = [&](int thread_id) {
            ready_threads++;
            while (ready_threads < thread_count) {
                this_thread::yield();
            }

            int ops_per_thread = count / thread_count;
            int op_index = 0;

            while (!stop_test && op_index < ops_per_thread) {
                uint64_t start_time = getCurrentTimestampMs();
                bool success = false;

                switch (test_type) {
                    case PerfTestType::SEND_MESSAGE: {
                        string msg = "perf_test_" + to_string(thread_id) + "_" + 
                                    to_string(op_index) + "_" + string(size - 20, 'A');
                        success = client.sendMessage(msg.substr(0, size), roomId);
                        break;
                    }
                    case PerfTestType::CREATE_ROOM: {
                        string room_name = "test_room_" + to_string(thread_id) + "_" + 
                                         to_string(op_index);
                        success = client.createRoom(room_name);
                        break;
                    }
                    case PerfTestType::REQUEST_HISTORY: {
                        // 设置请求超时时间为2秒
                        uint64_t timeout = 2000; // 2秒
                        success = client.requestRoomHistory(roomId, "", size);
                        
                        if (success) {
                            // 等待接收响应或超时
                            uint64_t wait_start = getCurrentTimestampMs();
                            while (getCurrentTimestampMs() - wait_start < timeout) {
                                if (client.hasHistoryResponse()) {
                                    success = true;
                                    break;
                                }
                                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                            }
                            
                            if (getCurrentTimestampMs() - wait_start >= timeout) {
                                LOG_ERROR << "请求历史消息超时，线程: " << thread_id 
                                         << ", 序号: " << op_index;
                                success = false;
                            }
                        }
                        break;
                    }
                }

                if (success) {
                    uint64_t latency = getCurrentTimestampMs() - start_time;
                    
                    total_operations++;
                    total_bytes += (test_type == PerfTestType::REQUEST_HISTORY) ? 
                                 sizeof(int) : size;  // 历史消息请求使用固定大小
                    total_time_ms += latency;
                    
                    updateMaxLatency(max_latency, latency);
                    updateMinLatency(min_latency, latency);

                    op_index++;
                } else {
                    LOG_ERROR << "操作失败，线程: " << thread_id 
                             << ", 序号: " << op_index;
                }

                // 每100次操作输出一次进度
                if (op_index % 100 == 0) {
                    LOG_DEBUG << "线程 " << thread_id << " 已完成 " 
                              << op_index << "/" << ops_per_thread << " 次操作";
                }
            }
        };

        uint64_t test_start_time = getCurrentTimestampMs();
        
        // 启动测试线程
        for (int i = 0; i < thread_count; i++) {
            threads.emplace_back(thread_func, i);
        }

        // 等待所有线程完成或超时
        uint64_t timeout_ms = 30000;  // 30秒超时
        for (auto& th : threads) {
            if (th.joinable()) {
                th.join();
            }
        }

        stop_test = true;  // 通知所有线程停止测试
        
        uint64_t total_time = getCurrentTimestampMs() - test_start_time;

        // 输出测试结果
        printTestResult({
            total_operations.load(),
            total_bytes.load(),
            total_time,
            total_time_ms.load() / (total_operations.load() > 0 ? total_operations.load() : 1),
            max_latency.load(),
            min_latency.load(),
            total_operations.load() * 1000 / total_time,
            total_bytes.load() * 8 * 1000 / total_time / 1024
        }, test_type);
    }

private:
    static uint64_t getCurrentTimestampMs() {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
    }

    static void updateMaxLatency(std::atomic<uint64_t>& max_latency, uint64_t latency) {
        uint64_t current_max = max_latency.load();
        while (latency > current_max && 
               !max_latency.compare_exchange_weak(current_max, latency));
    }

    static void updateMinLatency(std::atomic<uint64_t>& min_latency, uint64_t latency) {
        uint64_t current_min = min_latency.load();
        while (latency < current_min && 
               !min_latency.compare_exchange_weak(current_min, latency));
    }

    static void printTestResult(const PerfTestResult& result, PerfTestType test_type) {
        string operation_name;
        switch (test_type) {
            case PerfTestType::SEND_MESSAGE:
                operation_name = "消息";
                break;
            case PerfTestType::CREATE_ROOM:
                operation_name = "房间";
                break;
            case PerfTestType::REQUEST_HISTORY:
                operation_name = "历史请求";
                break;
        }
        
        LOG_INFO << "\n性能测试结果:";
        LOG_INFO << "总" << operation_name << "数: " << result.total_operations;
        LOG_INFO << "总字节数: " << result.total_bytes << " bytes";
        LOG_INFO << "总耗时: " << result.total_time_ms << " ms";
        LOG_INFO << "平均延迟: " << result.avg_latency_ms << " ms";
        LOG_INFO << "最大延迟: " << result.max_latency_ms << " ms";
        LOG_INFO << "最小延迟: " << result.min_latency_ms << " ms";
        LOG_INFO << "吞吐量: " << result.operations_per_second << " " << operation_name << "/秒";
        if (test_type != PerfTestType::REQUEST_HISTORY) {
            LOG_INFO << "带宽: " << result.kbps << " Kbps";
        }
    }
}; 