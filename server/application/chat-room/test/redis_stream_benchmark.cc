#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <string>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <hiredis/hiredis.h>
#include <hiredis/async.h>
#include <hiredis/adapters/libevent.h>
#include <event2/event.h>
#include <muduo/base/Logging.h>

#define k_message_batch_size 30   //一次读取30条消息，  客户端测试读取的时候也一次读取30条消息
// 简单的连接池实现
class RedisConnPool {
public:
    RedisConnPool(const std::string& host, int port, int poolSize)
        : host_(host), port_(port), poolSize_(poolSize) {
        // 预创建连接
        for (int i = 0; i < poolSize_; i++) {
            redisContext* conn = createConnection();
            if (conn) {
                freeConns_.push(conn);
            }
        }
    }

    ~RedisConnPool() {
        std::lock_guard<std::mutex> lock(mutex_);
        while (!freeConns_.empty()) {
            auto conn = freeConns_.front();
            redisFree(conn);
            freeConns_.pop();
        }
    }

    // 获取连接
    redisContext* getConnection() {
        std::unique_lock<std::mutex> lock(mutex_);
        while (freeConns_.empty()) {
            cond_.wait(lock);
        }
        auto conn = freeConns_.front();
        freeConns_.pop();
        return conn;
    }

    // 归还连接
    void releaseConnection(redisContext* conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        // 检查连接是否有效，无效则重新创建
        if (conn->err) {
            redisFree(conn);
            conn = createConnection();
        }
        if (conn) {
            freeConns_.push(conn);
            cond_.notify_one();
        }
    }

private:
    redisContext* createConnection() {
        struct timeval timeout = { 1, 500000 }; // 1.5 seconds
        redisContext* c = redisConnectWithTimeout(host_.c_str(), port_, timeout);
        if (c == nullptr || c->err) {
            if (c) {
                LOG_ERROR << "Connection error: " << c->errstr;
                redisFree(c);
            }
            return nullptr;
        }
        return c;
    }

    std::string host_;
    int port_;
    int poolSize_;
    std::queue<redisContext*> freeConns_;
    std::mutex mutex_;
    std::condition_variable cond_;
};

class RedisStreamBenchmark {
private:
    struct BenchmarkStats {
        std::atomic<int64_t> total_messages{ 0 };
        std::atomic<int64_t> total_duration_ms{ 0 };
        std::atomic<int> error_count{ 0 };

        void print(const std::string& test_name) {
            LOG_INFO << "=== " << test_name << " 测试结果 ===";
            LOG_INFO << "总消息数: " << total_messages;
            LOG_INFO << "总耗时: " << total_duration_ms << "ms";
            LOG_INFO << "平均每条消息耗时: " << (float)total_duration_ms / total_messages << "ms";
            LOG_INFO << "每秒处理消息数(TPS): " << (float)total_messages * 1000 / total_duration_ms;
            LOG_INFO << "错误次数: " << error_count;
            LOG_INFO << "========================\n";
        }
    };

    // 同步写入测试
    void syncWriter(RedisConnPool& pool,
        const std::string& key_prefix,
        int thread_id,
        int message_count,
        BenchmarkStats& stats) {
        // 获取连接
        redisContext* c = pool.getConnection();
        if (!c) return;

        // 使用线程ID构造唯一的key
        std::string unique_key = key_prefix + "_" + std::to_string(thread_id);

        std::string msg_content = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOP"
            "QRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ012"
            "3456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        //实际格式：DEBUG Xadd command: XADD 0002 * payload {"content":"2","timestamp":1740213419454,"user_id":8,"username":"darren"}

        for (int i = 0; i < message_count; i++) {
            redisReply* reply = (redisReply*)redisCommand(c,
                "XADD %s * payload  {msg_id:%d,msg:%s}",
                unique_key.c_str(), i, msg_content.c_str());

            if (reply == nullptr) {
                stats.error_count++;
            }
            else {
                //打印第一条消息做调试信息，这里打印返回的消息ID和消息内容
                if (i == 0) {
                    LOG_DEBUG << "写入的消息ID: " << reply->str << " 消息内容: " << msg_content;
                }
                stats.total_messages++;
                freeReplyObject(reply);
            }
        }

        pool.releaseConnection(c);
    }

    // 同步读取测试
    void syncReader(RedisConnPool& pool,
        const std::string& key_prefix,
        int thread_id,
        int read_count,
        BenchmarkStats& stats) {
        redisContext* c = pool.getConnection();
        if (!c) return;

        // 使用线程ID构造唯一的key
        std::string unique_key = key_prefix + "_" + std::to_string(thread_id);
        std::string last_id = "+"; // 从最新消息开始读取
        read_count = read_count / k_message_batch_size + 1;  // 计算需要读取的批次数
        // XREVRANGE key + - COUNT 30
        for (int i = 0; i < read_count; i++) {
            redisReply* reply = (redisReply*)redisCommand(c,
                "XREVRANGE %s %s - COUNT %d",
                unique_key.c_str(), last_id.c_str(), k_message_batch_size);

            if (reply == nullptr) {
                stats.error_count++;
                continue;
            }

            if (reply->type == REDIS_REPLY_ARRAY) {
                stats.total_messages += reply->elements;  // 记录实际读取的消息数量

                // 获取最后一条消息的ID作为下次读取的起点
                if (reply->elements > 0) {
                    redisReply* first_msg = reply->element[0];
                    if (first_msg->elements == 2) {  // ID和消息体
                        std::string msg_id = first_msg->element[0]->str;

                        // 获取消息内容 - payload字段
                        redisReply* fields = first_msg->element[1];
                        std::string payload;
                        for (size_t j = 0; j < fields->elements; j += 2) {
                            if (strcmp(fields->element[j]->str, "payload") == 0) {
                                payload = fields->element[j + 1]->str;
                                break;
                            }
                        }

                        // 打印第一条消息用于调试
                        LOG_DEBUG << "读取到的消息ID: " << msg_id
                            << " 消息内容: " << payload;

                        // 更新last_id为当前批次最后一条消息的ID
                        redisReply* last_msg = reply->element[reply->elements - 1];
                        last_id = last_msg->element[0]->str;
                    }
                }
            }

            freeReplyObject(reply);
        }

        pool.releaseConnection(c);
    }

public:
    void runBenchmark(const std::string& host = "127.0.0.1",
        int port = 6379,
        const std::string& key_prefix = "test_stream",
        int thread_count = 16,
        int messages_per_thread = 10000) {

        RedisConnPool pool(host, port, thread_count);

        // 清除所有线程的旧数据
        redisContext* c = pool.getConnection();
        if (c) {
            for (int i = 0; i < thread_count; i++) {
                std::string unique_key = key_prefix + "_" + std::to_string(i);
                LOG_INFO << "清除数据 unique_key: " << unique_key;
                redisReply* reply = (redisReply*)redisCommand(c, "DEL %s", unique_key.c_str());
                if (reply) freeReplyObject(reply);
            }
            pool.releaseConnection(c);
        }

        // 写入测试
        {
            BenchmarkStats stats;
            std::vector<std::thread> threads;

            LOG_INFO << "开始 Redis Stream 写入性能测试";
            LOG_INFO << "线程数: " << thread_count;
            LOG_INFO << "每个线程消息数: " << messages_per_thread;

            auto start = std::chrono::high_resolution_clock::now();

            for (int i = 0; i < thread_count; ++i) {
                threads.emplace_back(&RedisStreamBenchmark::syncWriter,
                    this,
                    std::ref(pool),        // 需要用 std::ref 包装
                    key_prefix,            // string 可以直接传值
                    i,
                    messages_per_thread,
                    std::ref(stats));
            }

            for (auto& thread : threads) {
                thread.join();
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            stats.total_duration_ms = duration.count();

            stats.print("Redis Stream 写入");
        }

        // 等待1秒确保数据写入完成
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // 读取测试
        {
            BenchmarkStats stats;
            std::vector<std::thread> threads;

            LOG_INFO << "开始 Redis Stream 读取性能测试";
            LOG_INFO << "线程数: " << thread_count;
            LOG_INFO << "每个线程读取次数: " << messages_per_thread;

            auto start = std::chrono::high_resolution_clock::now();

            for (int i = 0; i < thread_count; ++i) {
                threads.emplace_back(&RedisStreamBenchmark::syncReader,
                    this,
                    std::ref(pool),        // 需要用 std::ref 包装
                    key_prefix,            // string 可以直接传值
                    i,
                    messages_per_thread,
                    std::ref(stats));
            }

            for (auto& thread : threads) {
                thread.join();
            }

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            stats.total_duration_ms = duration.count();

            stats.print("Redis Stream 读取");
        }
    }
};

int main(int argc, char* argv[]) {
    RedisStreamBenchmark benchmark;

    std::string host = "127.0.0.1";
    int port = 6379;
    std::string key_prefix = "test_stream";
    int thread_count = 1;
    int messages_per_thread = 10000;

    if (argc > 1) thread_count = std::stoi(argv[1]);
    if (argc > 2) messages_per_thread = std::stoi(argv[2]);

    benchmark.runBenchmark(host, port, key_prefix, thread_count, messages_per_thread);

    return 0;
}