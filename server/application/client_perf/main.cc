#include "httplib.h"
#include <iostream>
#include <fstream>
#include <sys/time.h>
#include <cmath>  // 添加这行来使用 std::floor
#include "muduo/base/md5.h"
#include "muduo/base/Logging.h"
#include "ws_client.h"
#include "config_manager.h"

#include <json/json.h> // jsoncpp头文件
#include <thread>
#include <atomic>

using std::string;


class User {
public:
    User(string email, string password) {
        email_ = email;
        MD5 md5(password);
        password_ = md5.toString();
        LOG_INFO << "email: " << email_ << ", password: " << password_;
    }
    void SetCookie(string &cookie) {
        cookie_ = cookie;
    }
    string &GetName() {
        return email_;
    }
    string &GetPassword() {
        return password_;
    }
    string &getCookie() {
        return cookie_;
    }
private:
    string email_;           //用户邮箱
    string password_;       //密码
    string cookie_;
    string  file_full_path_;    //文件路径
};
 


int decodeLoginRespone(string &str_json, string &id, string &message) {
    bool res;
    Json::Value root;
    Json::Reader jsonReader;
    res = jsonReader.parse(str_json, root);
    if (!res) {
        LOG_ERROR << "parse reg json failed ";
        return -1;
    }
  
    if (root["id"].isNull()) {
        LOG_ERROR << "code null";
        return -1;
    }
    id = root["id"].asString();
 
    if (root["message"].isNull()) {
        LOG_ERROR << "message null";
        return -1;
    }
    message = root["message"].asString();

    return 0;
}
 

static uint64_t getMicroseconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    long long microseconds = tv.tv_sec * 1000000LL + tv.tv_usec;
    return microseconds;
}  

static uint64_t getMs() {
    return getMicroseconds()/1000;
}  

// 添加解析cookie的函数
string parseCookie(const httplib::Headers& headers) {
    auto it = headers.find("Set-Cookie");
    if (it != headers.end()) {
        string cookie = it->second;
        LOG_INFO << "完整的Set-Cookie内容: [" << cookie << "]";
        // 查找cookie值的开始位置
        size_t start = cookie.find("Cookie");
        if (start != string::npos) {
            start += 12; // "Cookie: sid="的长度
            LOG_INFO << "start: " << start;
            // 查找cookie值的结束位置(分号前)
            size_t end = cookie.find(";", start);
            if (end != string::npos) {
                return cookie.substr(start, end - start);
            } else {
                return cookie.substr(start);
            }
        } else {
            // 如果没找到"Cookie"，尝试直接查找"sid="
            start = cookie.find("sid=");
            if (start != string::npos) {
                start += 4; // "sid="的长度
                LOG_INFO << "start: " << start;
                size_t end = cookie.find(";", start);
                if (end != string::npos) {
                    return cookie.substr(start, end - start);
                } else {
                    return cookie.substr(start);
                }
            }
            LOG_WARN << "can't find Cookie: sid= or sid=";
        }
    }
    return "";
}

std::atomic<bool> g_running{true};
std::atomic<int> g_connected_users{0};
 

// 修改性能统计结构体
struct PerformanceStats {
    std::atomic<uint64_t> total_messages{0};
    std::atomic<uint64_t> total_send_requests{0};  //发送次数
    std::atomic<uint64_t> total_bytes{0};
    std::atomic<uint64_t> failed_messages{0};
    uint64_t start_time;
    uint64_t target_messages;  // 目标消息数
    bool is_send_test;
    
    void start(bool send_test, uint64_t messages) {
        start_time = getMs();
        is_send_test = send_test;
        target_messages = messages;
    }
    
    bool isCompleted() const {
        return total_messages >= target_messages;
    }
    
    void printStats() {
        uint64_t duration = getMs() - start_time;
        double seconds = duration / 1000.0;
        double msg_per_sec = total_messages / seconds;
        double mb_per_sec = (total_bytes / (1024.0 * 1024.0)) / seconds;
        is_send_test = 1; //这里不管是发送消息，还是请求消息，都只打印请求的情况
        LOG_INFO << "性能测试统计 (" << (is_send_test ? "发送" : "接收") << "):";
        LOG_INFO << "总" << (is_send_test ? "发送" : "接收") << "消息数: " << total_messages;
        LOG_INFO << "总" << (is_send_test ? "发送" : "接收") << "字节数: " << total_bytes << " bytes";
        if (is_send_test) {
            LOG_INFO << "发送失败消息数: " << failed_messages;
        }
        LOG_INFO << "测试时长: " << seconds << " 秒";
        LOG_INFO << (is_send_test ? "发送" : "接收") << "消息吞吐量: " << msg_per_sec << " 消息/秒";
        LOG_INFO << (is_send_test ? "发送" : "接收") << "数据吞吐量: " << mb_per_sec << " MB/秒";
        if (is_send_test && total_messages > 0) {
            LOG_INFO << "消息失败率: " << (failed_messages * 100.0 / total_messages) << "%";
        }
        LOG_INFO << "发送次数: " << total_send_requests;
    }
};

static PerformanceStats g_stats;

// 添加用户连接信息结构体
struct UserConnection {
    std::unique_ptr<WSClient> ws_client;
    std::string room_id;
    bool connected{false};
};

// 处理用户登录和WebSocket连接
bool setupUserConnection(const UserInfo& user_info, UserConnection& conn) {
    httplib::Client cli(HTTP_SERVER_IP, HTTP_SERVER_PORT);
    User user(user_info.email, user_info.password);
    
    // 登录
    Json::Value root;
    root["email"] = user.GetName();
    root["password"] = user.GetPassword();
    
    Json::FastWriter writer;
    std::string req_json = writer.write(root);
    
    httplib::Headers headers = {
        { "content-type", "application/json" }
    };

    auto res = cli.Post("/api/login", headers, req_json, "application/json");
    if (!res || res->status != 204) {
        LOG_ERROR << "用户登录失败: " << user_info.email;
        return false;
    }
    LOG_INFO << "用户登录成功: " << user_info.email;

    // 获取cookie
    std::string cookie = parseCookie(res->headers);
    if (cookie.empty()) {
        LOG_ERROR << "获取cookie失败: " << user_info.email;
        return false;
    }

    // 创建WebSocket客户端
    conn.ws_client = std::make_unique<WSClient>(cookie);
    conn.room_id = user_info.room_id;
    
    // 连接WebSocket服务器
    if (!conn.ws_client->connect(HTTP_SERVER_IP, HTTP_SERVER_PORT)) {
        LOG_ERROR << "WebSocket连接失败: " << user_info.email;
        return false;
    }

    conn.connected = true;
    return true;
}

// 简化工作线程函数中的历史消息测试部分
void userWorkerThread(UserConnection& conn, bool is_send_test, int message_count) {
    if (is_send_test) {
        int msg_count = 0;
        std::string msg_content = "--0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOP"
            "QRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ012"
            "3456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
        
        while (g_running && msg_count < message_count) {
            std::string message = std::to_string(msg_count++) + msg_content;
            if (conn.ws_client->sendMessage(message, conn.room_id)) {
                g_stats.total_send_requests++;
                g_stats.total_messages++;
                g_stats.total_bytes += 275;  // 根据send前获取的字节大小
                LOG_DEBUG << "room_id:" << conn.room_id << ", msg_count:" << msg_count 
                        << ", total_messages: " << g_stats.total_messages;
            } else {
                g_stats.failed_messages++;
            }
        }
    } else {  // 接收消息测试
        while (g_running && !g_stats.isCompleted()) {
            // 简化为只发送请求，不等待响应
            conn.ws_client->requestRoomHistory(conn.room_id, "", k_message_batch_size);
            g_stats.total_messages += k_message_batch_size;
            g_stats.total_send_requests++;
            g_stats.total_bytes += 89;  // 根据send前获取的字节大小
        }
    }
    LOG_INFO << "房间 " << conn.room_id << " 测试完成";
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        std::cout << "Usage: " << argv[0] << " <user_count> <messages_per_thread> <test_type>\n";
        std::cout << "test_type: 1=发送测试, 2=接收测试\n";
        std::cout << "messages_per_thread: 发送消息数量或历史消息请求次数\n";
        return 1;
    }

    int user_count = std::atoi(argv[1]);
    uint64_t messages_per_thread = std::atoll(argv[2]);
    uint64_t total_message_count = messages_per_thread * user_count;  //总的发送消息数量
    int test_type = std::atoi(argv[3]);
    
    if (user_count <= 0 || total_message_count <= 0 || (test_type != 1 && test_type != 2)) {
        LOG_ERROR << "参数无效";
        return 1;
    }
    
    // 设置日志级别为 INFO
    muduo::Logger::setLogLevel(muduo::Logger::INFO);
    
    // 初始化配置管理器并加载用户信息
    auto& config_mgr = ConfigManager::getInstance();
    if (!config_mgr.loadConfig("users.ini")) {
        LOG_ERROR << "加载用户配置文件失败";
        return 1;
    }

    // 检查是否有足够的用户账号
    if (!config_mgr.hasEnoughUsers(user_count)) {
        LOG_ERROR << "账号数量不足，需要 " << user_count << " 个账号，但只有 " 
                 << config_mgr.getUsers().size() << " 个账号";
        return 1;
    }

    // 初始化所有用户连接
    std::vector<UserConnection> user_connections(user_count);
    auto users = config_mgr.getUsers();
    
    LOG_INFO << "正在建立用户连接...";
    for (int i = 0; i < user_count; i++) {
        if (!setupUserConnection(users[i], user_connections[i])) {
            LOG_ERROR << "用户 " << users[i].email << " 连接失败";
            return 1;
        }
        g_connected_users++;
        LOG_INFO << "用户 " << users[i].email << " 连接成功 (" 
                << g_connected_users << "/" << user_count << ")";
    }

    bool is_send_test = (test_type == 1);
    LOG_INFO << "开始" << (is_send_test ? "发送" : "接收") << "性能测试";
    LOG_INFO << "用户数量: " << user_count;
    LOG_INFO << "目标" << (is_send_test ? "消息" : "请求") << "数量: " << total_message_count;

    // 启动性能统计
    g_stats.start(is_send_test, total_message_count); // 启动性能统计 获取起始时间

    // 启动工作线程
    std::vector<std::thread> worker_threads;
    for (int i = 0; i < user_count; i++) {
        worker_threads.emplace_back(userWorkerThread, 
                                  std::ref(user_connections[i]), 
                                  is_send_test, 
                                  messages_per_thread);
    }

    LOG_INFO << "所有工作线程已启动";
    LOG_INFO << "测试进行中...";

    // 等待测试完成
    while (!g_stats.isCompleted()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    LOG_INFO << "g_running:" << g_running << ",g_stats.isCompleted():" << g_stats.isCompleted();
    // 停止测试并等待线程结束
    g_running = false;
    for (auto& thread : worker_threads) {
        thread.join();
    }

    // 打印统计信息
    g_stats.printStats(); //结束时间

    return 0;
}
