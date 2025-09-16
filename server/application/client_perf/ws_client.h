#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <string>
#include <vector>
#include <thread>
#include <openssl/sha.h>
#include <openssl/bio.h>
#include <openssl/evp.h>
#include <openssl/buffer.h>
#include "muduo/base/Logging.h"
#include <json/json.h>
#include <map>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <functional>

#define HTTP_SERVER_IP "127.0.0.1"
// #define HTTP_SERVER_IP "192.168.1.18"

#define HTTP_SERVER_PORT 8080
#define k_message_batch_size 30   //一次读取30条消息，  客户端测试读取的时候也一次读取30条消息

using namespace std;

// Base64编码
string base64Encode(const unsigned char* input, int length) {
    BIO* bio, *b64;
    BUF_MEM* bufferPtr;

    b64 = BIO_new(BIO_f_base64());
    bio = BIO_new(BIO_s_mem());
    bio = BIO_push(b64, bio);

    BIO_write(bio, input, length);
    BIO_flush(bio);
    BIO_get_mem_ptr(bio, &bufferPtr);

    string result(bufferPtr->data, bufferPtr->length-1);  // 去掉末尾换行符

    BIO_free_all(bio);
    return result;
}

class WSClient {
public:
    using MessageStatsCallback = std::function<void(size_t message_count, size_t bytes)>;
    
    WSClient(const string& cookie) : cookie_(cookie), connected_(false), sockfd_(-1) {}
    
    void setMessageStatsCallback(MessageStatsCallback cb) {
        message_stats_callback_ = cb;
    }
    
    ~WSClient() {
        close();
    }

    bool connect(const string& host, int port) {
        // 创建socket
        sockfd_ = socket(AF_INET, SOCK_STREAM, 0);
        if (sockfd_ < 0) {
            LOG_ERROR << "创建socket失败";
            return false;
        }

        // 解析主机名
        struct hostent* server = gethostbyname(host.c_str());
        if (server == NULL) {
            LOG_ERROR << "无法解析主机名";
            return false;
        }

        struct sockaddr_in serv_addr;
        memset(&serv_addr, 0, sizeof(serv_addr));
        serv_addr.sin_family = AF_INET;
        memcpy(&serv_addr.sin_addr.s_addr, server->h_addr, server->h_length);
        serv_addr.sin_port = htons(port);

        // 连接服务器
        if (::connect(sockfd_, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
            LOG_ERROR << "连接服务器失败";
            return false;
        }

        // WebSocket握手
        if (!performHandshake(host, port)) {
            LOG_ERROR << "WebSocket握手失败";
            return false;
        }

        connected_ = true;

        // 启动接收消息的线程并立即分离
        std::thread([this]() {
            this->receiveLoop();
        }).detach();

        return true;
    }
    // 构造 WebSocket 数据帧
    std::string buildWebSocketFrame(const std::string& payload, const uint8_t opcode = 0x01) {
        std::string frame;

        frame.push_back(0x80 | (opcode & 0x0F));

        size_t payload_length = payload.size();
        if (payload_length <= 125) {
            frame.push_back(static_cast<uint8_t>(payload_length));
        } else if (payload_length <= 65535) {
            frame.push_back(126);
            frame.push_back(static_cast<uint8_t>((payload_length >> 8) & 0xFF));
            frame.push_back(static_cast<uint8_t>(payload_length & 0xFF));
        } else {
            frame.push_back(127);
            for (int i = 7; i >= 0; i--) {
                frame.push_back(static_cast<uint8_t>((payload_length >> (8 * i)) & 0xFF));
            }
        }

        frame += payload;

        return frame;
    }
    bool sendMessage(const string& content, const string& roomId) {
        if (!connected_) {
            LOG_ERROR << "未连接到服务器";
            return false;
        }

        // 构造消息格式
        Json::Value root;
        Json::Value payload;
        Json::Value messages;
        Json::Value message;

        root["type"] = "clientMessages";
        message["content"] = content;
        messages.append(message);
        payload["roomId"] = roomId;
        payload["messages"] = messages;
        root["payload"] = payload;

        Json::FastWriter writer;
        string ws_json = buildWebSocketFrame(writer.write(root));


        // LOG_INFO << "发送消息大小: " << ws_json.size() << " bytes";
         
        // 发送数据帧
        ssize_t sent = ::send(sockfd_, ws_json.c_str(), ws_json.size(), 0);
        if (sent <= 0) {
            if (sent == 0) {
                LOG_ERROR << "连接已关闭";
            } else {
                LOG_ERROR << "发送消息失败: " << strerror(errno);
            }
            connected_ = false;
            return false;
        }

        return true;
    }

    void close() {
        if (connected_) {
            connected_ = false;
            if (sockfd_ >= 0) {
                ::close(sockfd_);
                sockfd_ = -1;
            }
        }
    }

    bool requestRoomHistory(const string& roomId, const string& firstMessageId = "", int count = k_message_batch_size) {
        if (!connected_) {
            LOG_ERROR << "未连接到服务器";
            return false;
        }

        Json::Value root;
        Json::Value payload;

        root["type"] = "requestRoomHistory";
        payload["roomId"] = roomId;
        payload["firstMessageId"] = firstMessageId;
        payload["count"] = count;
        root["payload"] = payload;

        Json::FastWriter writer;
        string json_msg = writer.write(root);
        
        // LOG_INFO << "请求历史消息大小: " << json_msg.size() << " bytes";
        return sendFrame(json_msg);
    }

    bool createRoom(const string& roomName) {
        if (!connected_) {
            LOG_ERROR << "未连接到服务器";
            return false;
        }

        Json::Value root;
        Json::Value payload;

        root["type"] = "clientCreateRoom";
        payload["roomName"] = roomName;
        root["payload"] = payload;

        Json::FastWriter writer;
        string json_msg = writer.write(root);
        
        LOG_DEBUG << "创建房间: " << json_msg;
        return sendFrame(json_msg);
    }

    bool hasHistoryResponse() {
        bool received = history_response_received_.load();
        if (received) {
            history_response_received_.store(false);
        }
        return received;
    }

    struct HistoryResponse {
        bool has_more;
        std::string last_msg_id;
        size_t message_count;
        size_t total_bytes;
    };

    using HistoryResponseCallback = std::function<void(const HistoryResponse&)>;
    
    void setHistoryResponseCallback(HistoryResponseCallback cb) {
        history_response_callback_ = cb;
    }

private:
    bool performHandshake(const string& host, int port) {
        // 生成WebSocket key
        unsigned char random[16];
        for (int i = 0; i < 16; i++) {
            random[i] = rand() % 256;
        }
        string ws_key = base64Encode(random, 16);

        // 构造握手请求
        string request = 
            "GET /chat/chatroom HTTP/1.1\r\n"
            "Host: " + host + ":" + to_string(port) + "\r\n"
            "Upgrade: websocket\r\n"
            "Connection: Upgrade\r\n"
            "Sec-WebSocket-Key: " + ws_key + "\r\n"
            "Sec-WebSocket-Version: 13\r\n"
            "Cookie: sid=" + cookie_ + "\r\n"
            "\r\n";

        // 发送握手请求
        if (::send(sockfd_, request.c_str(), request.length(), 0) < 0) {
            LOG_ERROR << "发送握手请求失败";
            return false;
        }

        // 接收握手响应
        char buffer[1024];
        ssize_t bytes = recv(sockfd_, buffer, sizeof(buffer)-1, 0);
        if (bytes <= 0) {
            LOG_ERROR << "接收握手响应失败";
            return false;
        }
        buffer[bytes] = '\0';

        // 验证响应状态码
        if (strstr(buffer, "HTTP/1.1 101") == nullptr) {
            LOG_ERROR << "握手响应无效";
            return false;
        }

        return true;
    }

    // 添加缓冲区来存储未完整的消息
    vector<uint8_t> recv_buffer_;
    
    // 解析WebSocket帧
    bool parseFrame(vector<uint8_t>& buffer, size_t& pos, string& message) {
        if (buffer.size() < pos + 2) return false;  // 数据不足以解析头部

        bool fin = (buffer[pos] & 0x80) != 0;
        uint8_t opcode = buffer[pos] & 0x0F;
        bool masked = (buffer[pos + 1] & 0x80) != 0;
        uint64_t payload_length = buffer[pos + 1] & 0x7F;
        
        size_t header_length = 2;
        
        // 处理扩展长度
        if (payload_length == 126) {
            if (buffer.size() < pos + 4) return false;  // 数据不足
            payload_length = (buffer[pos + 2] << 8) | buffer[pos + 3];
            header_length += 2;
        } else if (payload_length == 127) {
            if (buffer.size() < pos + 10) return false;  // 数据不足
            payload_length = 0;
            for (int i = 0; i < 8; i++) {
                payload_length = (payload_length << 8) | buffer[pos + 2 + i];
            }
            header_length += 8;
        }

        // 处理掩码
        uint8_t mask_key[4] = {0};
        if (masked) {
            if (buffer.size() < pos + header_length + 4) return false;  // 数据不足
            memcpy(mask_key, &buffer[pos + header_length], 4);
            header_length += 4;
        }

        // 检查是否有足够的数据
        if (buffer.size() < pos + header_length + payload_length) {
            return false;  // 数据不足
        }

        // 提取消息内容
        message.clear();
        for (size_t i = 0; i < payload_length; i++) {
            uint8_t byte = buffer[pos + header_length + i];
            if (masked) {
                byte ^= mask_key[i % 4];
            }
            message.push_back(byte);
        }

        // 更新位置
        pos += header_length + payload_length;
        
        return true;
    }

    void receiveLoop() {
        vector<uint8_t> buffer(4096);
        vector<uint8_t> message_buffer;
        
        while (connected_) {  // 使用connected_标志控制线程退出
            ssize_t bytes = recv(sockfd_, buffer.data(), buffer.size(), 0);
            if (bytes <= 0) {
                if (connected_) {  // 只有在还处于连接状态时才报错
                    LOG_ERROR << "接收消息失败或连接断开";
                }
                break;
            }
            
            // 将新数据添加到接收缓冲区
            recv_buffer_.insert(recv_buffer_.end(), buffer.begin(), buffer.begin() + bytes);

            // 尝试解析完整的帧
            size_t pos = 0;
            while (pos < recv_buffer_.size()) {
                string message;
                if (!parseFrame(recv_buffer_, pos, message)) {
                    break;  // 数据不完整，等待更多数据
                }

                // 处理完整的消息
                handleMessage(message);
            }

            // 移除已处理的数据
            if (pos > 0) {
                recv_buffer_.erase(recv_buffer_.begin(), recv_buffer_.begin() + pos);
            }

            // 防止缓冲区无限增长
            if (recv_buffer_.size() > 1024 * 1024) {  // 1MB限制
                LOG_ERROR << "接收缓冲区过大，关闭连接";
                break;
            }
        }
    }

    void handleMessage(const string& msg) {
        LOG_DEBUG << "收到消息: " << msg;

        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(msg, root)) {
            LOG_ERROR << "解析消息JSON失败:" << msg;
            return;
        }

        if (!root["type"].isNull()) {
            string type = root["type"].asString();
            
            if (type == "hello") {
                handleHelloMessage(root);
            } else if (type == "serverMessages") {
                handleServerMessages(root);
            } else if (type == "serverCreateRoom") {
                handleServerCreateRoom(root);
            } else if (type == "serverRoomHistory") {
                handleServerRoomHistory(root);
            } else {
                LOG_WARN << "未知的消息类型: " << type;
            }
        }
    }

    // 处理hello消息
    void handleHelloMessage(const Json::Value& root) {
        if (!root["payload"].isNull()) {
            const Json::Value& payload = root["payload"];
            
            // 处理用户信息
            if (!payload["me"].isNull()) {
                const Json::Value& me = payload["me"];
                LOG_INFO << "我的信息 - ID: " << me["id"].asString() 
                        << ", 用户名: " << me["username"].asString();
            }
            
            // 处理房间列表
            if (!payload["rooms"].isNull() && payload["rooms"].isArray()) {
                LOG_INFO << "房间列表:";
                for (const auto& room : payload["rooms"]) {
                    string room_id = room["id"].asString();
                    string room_name = room["name"].asString();
                    rooms_[room_id] = room_name;
                    
                    LOG_INFO << "房间 ID: " << room_id 
                            << ", 名称: " << room_name;
                    
                    // 如果还没有选择房间，使用第一个房间作为默认房间
                    if (current_room_id_.empty()) {
                        current_room_id_ = room_id;
                        LOG_INFO << "默认选择房间: " << room_name << " (ID: " << room_id << ")";
                    }
                }
            }
        }
    }

    // 处理服务器消息
    void handleServerMessages(const Json::Value& root) {
        // 1. 检查根节点是否包含payload
        if (root["payload"].isNull()) {
            LOG_ERROR << "服务器消息响应中缺少payload字段";
            return;
        }
        
        const Json::Value& payload = root["payload"];

        // 2. 检查payload中的messages字段
        if (payload["messages"].isNull() || !payload["messages"].isArray()) {
            LOG_ERROR << "服务器消息响应中messages字段无效";
            return;
        }
        
        const Json::Value& messages = payload["messages"];

        // 3. 遍历每条消息并进行检查
        for (const auto& msg : messages) {
            // 检查每个消息的必要字段
            if (msg["id"].isNull()) {
                LOG_WARN << "消息缺少id字段";
                continue;  // 跳过此消息
            }
            
            if (msg["user"].isNull() || msg["user"]["username"].isNull()) {
                LOG_WARN << "消息缺少用户信息";
                continue;  // 跳过此消息
            }
            
            if (msg["content"].isNull()) {
                LOG_WARN << "消息缺少内容";
                continue;  // 跳过此消息
            }

            // 4. 处理有效的消息
            std::string message_id = msg["id"].asString();
            std::string username = msg["user"]["username"].asString();
            std::string content = msg["content"].asString();

            // 5. 记录有效消息
            LOG_DEBUG << "收到消息: ID=" << message_id << ", 用户=" << username << ", 内容=" << content;

            // 6. 这里可以添加其他处理逻辑，比如更新统计信息等
        }
    }

    // 处理创建房间的响应
    void handleServerCreateRoom(const Json::Value& root) {
        if (!root["payload"].isNull()) {
            const Json::Value& payload = root["payload"];
            LOG_INFO << "新房间创建 - ID: " << payload["roomId"].asString() 
                    << ", 名称: " << payload["roomName"].asString();
        }
    }

    // 简化历史消息处理函数
    void handleServerRoomHistory(const Json::Value& root) {
        // 简化处理，只更新消息计数
        if (!root["payload"].isNull() && !root["payload"]["messages"].isNull() 
            && root["payload"]["messages"].isArray()) {
            
            const Json::Value& messages = root["payload"]["messages"];
            size_t message_count = messages.size();
            size_t total_bytes = 0;
            
            // 计算总字节数
            for (const auto& msg : messages) {
                if (!msg["content"].isNull()) {
                    total_bytes += msg["content"].asString().length();
                }
            }
            
            // 更新性能统计
            if (message_stats_callback_) {
                message_stats_callback_(message_count, total_bytes);
            }
        }
    }

    bool sendFrame(const string& msg) {
        vector<uint8_t> frame;
        frame.push_back(0x81);  // FIN=1, opcode=1 (text)

        if (msg.length() < 126) {
            frame.push_back(msg.length());
        } else if (msg.length() < 65536) {
            frame.push_back(126);
            frame.push_back((msg.length() >> 8) & 0xFF);
            frame.push_back(msg.length() & 0xFF);
        } else {
            frame.push_back(127);
            for (int i = 7; i >= 0; i--) {
                frame.push_back((msg.length() >> (i * 8)) & 0xFF);
            }
        }

        frame.insert(frame.end(), msg.begin(), msg.end());
        
        ssize_t sent = ::send(sockfd_, frame.data(), frame.size(), 0);
        if (sent <= 0) {
            if (sent == 0) {
                LOG_ERROR << "连接已关闭";
            } else {
                LOG_ERROR << "发送消息失败: " << strerror(errno);
            }
            connected_ = false;
            return false;
        }
        return true;
    }

private:
    string cookie_;
    bool connected_;
    int sockfd_;
    std::map<string, string> rooms_;  // roomId -> roomName
    string current_room_id_;
    std::atomic<bool> history_response_received_{false};
    std::mutex history_mutex_;
    std::condition_variable history_cv_;
    MessageStatsCallback message_stats_callback_;
    HistoryResponseCallback history_response_callback_;
};

// 修改 startChat 函数，删除性能测试相关的命令说明和处理逻辑
void startChat(const string& cookie) {
    WSClient ws_client(cookie);
    
    if (!ws_client.connect(HTTP_SERVER_IP, HTTP_SERVER_PORT)) {
        LOG_ERROR << "连接聊天服务器失败";
        return;
    }

    LOG_INFO << "连接成功，开始聊天\n"
             << "支持的命令：\n"
             << "1. room:房间ID          - 切换房间\n"
             << "2. create:房间名称      - 创建新房间\n"
             << "3. history:房间ID       - 获取房间历史消息\n"
             << "4. history:房间ID:消息ID:数量 - 获取指定消息之前的历史消息\n"
             << "5. quit                 - 退出程序\n"
             << "直接输入文本发送消息\n";
    
    string current_room_id;
    
    // 等待接收hello消息，获取默认房间ID
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 简单的命令行界面
    string input;
    while (getline(cin, input)) {
        if (input == "quit") {
            break;
        }
        
        // 处理各种命令
        if (input.substr(0, 5) == "room:") {
            current_room_id = input.substr(5);
            LOG_INFO << "切换到房间: " << current_room_id;
            continue;
        }
        
        if (input.substr(0, 7) == "create:") {
            string room_name = input.substr(7);
            ws_client.createRoom(room_name);
            continue;
        }
        
        if (input.substr(0, 8) == "history:") {
            string params = input.substr(8);
            vector<string> args;
            size_t pos = 0;
            string token;
            while ((pos = params.find(":")) != string::npos) {
                token = params.substr(0, pos);
                args.push_back(token);
                params.erase(0, pos + 1);
            }
            if (!params.empty()) {
                args.push_back(params);
            }
            
            if (args.size() == 1) {
                // 只有房间ID
                ws_client.requestRoomHistory(args[0]);
            } else if (args.size() == 3) {
                // 房间ID、消息ID和数量
                ws_client.requestRoomHistory(args[0], args[1], stoi(args[2]));
            } else {
                LOG_ERROR << "历史消息命令格式错误";
            }
            continue;
        }
        
        // 发送普通消息
        if (!current_room_id.empty()) {
            ws_client.sendMessage(input, current_room_id);
        } else {
            LOG_ERROR << "请先使用 room:房间ID 选择一个聊天室";
        }
    }

    ws_client.close();
}