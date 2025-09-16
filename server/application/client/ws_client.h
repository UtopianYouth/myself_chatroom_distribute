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

#define HTTP_SERVER_IP "127.0.0.1"
// #define HTTP_SERVER_IP "192.168.1.18"

#define HTTP_SERVER_PORT 8080

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
    WSClient(const string& cookie) : cookie_(cookie), connected_(false), sockfd_(-1) {}
    
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

        // 启动接收消息的线程
        receive_thread_ = std::thread([this]() {
            this->receiveLoop();
        });

        return true;
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
        string json_msg = writer.write(root);
        
        LOG_DEBUG << "发送消息: " << json_msg;
        
        // 构造WebSocket数据帧
        vector<uint8_t> frame;
        frame.push_back(0x81);  // FIN=1, opcode=1 (text)

        // 设置payload长度
        if (json_msg.length() < 126) {
            frame.push_back(json_msg.length());
        } else if (json_msg.length() < 65536) {
            frame.push_back(126);
            frame.push_back((json_msg.length() >> 8) & 0xFF);
            frame.push_back(json_msg.length() & 0xFF);
        } else {
            frame.push_back(127);
            for (int i = 7; i >= 0; i--) {
                frame.push_back((json_msg.length() >> (i * 8)) & 0xFF);
            }
        }

        // 添加payload
        frame.insert(frame.end(), json_msg.begin(), json_msg.end());

        // 发送数据帧
        ssize_t sent = ::send(sockfd_, frame.data(), frame.size(), 0);
        if (sent < 0) {
            LOG_ERROR << "发送消息失败";
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
        if (receive_thread_.joinable()) {
            receive_thread_.join();
        }
    }

    bool requestRoomHistory(const string& roomId, const string& firstMessageId = "", int count = 30) {
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
        
        LOG_DEBUG << "请求历史消息: " << json_msg;
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
        vector<uint8_t> temp_buffer(1024);
        size_t pos = 0;
        
        while (connected_) {
            LOG_DEBUG << "接收数据 into, temp_buffer.size(): " << temp_buffer.size();
            ssize_t bytes = recv(sockfd_, temp_buffer.data(), temp_buffer.size(), 0);
            if (bytes <= 0) {
                if (bytes < 0) {
                    LOG_ERROR << "接收数据错误: " << strerror(errno);
                }
                LOG_ERROR << "接收数据错误: " << strerror(errno);
                break;
            }
            LOG_DEBUG << "接收数据 bytes: " << bytes;
            // 将新数据添加到接收缓冲区
            recv_buffer_.insert(recv_buffer_.end(), temp_buffer.begin(), temp_buffer.begin() + bytes);

            // 尝试解析完整的帧
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
                pos = 0;
            }

            // 防止缓冲区无限增长
            if (recv_buffer_.size() > 1024 * 1024) {  // 1MB限制
                LOG_ERROR << "接收缓冲区过大，关闭连接";
                break;
            }
        }

        connected_ = false;
    }

    void handleMessage(const string& msg) {
        LOG_DEBUG << "收到消息: " << msg;

        Json::Value root;
        Json::Reader reader;
        if (!reader.parse(msg, root)) {
            LOG_ERROR << "解析消息JSON失败";
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
        if (!root["payload"].isNull()) {
            const Json::Value& payload = root["payload"];
            string roomId = payload["roomId"].asString();
            
            if (!payload["messages"].isNull() && payload["messages"].isArray()) {
                const Json::Value& messages = payload["messages"];
                LOG_INFO << "房间 " << roomId << " (" << rooms_[roomId] << ") 的新消息:";
                
                for (const auto& msg : messages) {
                    if (!msg["user"].isNull() && !msg["content"].isNull()) {
                        LOG_INFO << msg["user"]["username"].asString() << ": " 
                                << msg["content"].asString();
                    }
                }
            }
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

    // 处理房间历史消息
    void handleServerRoomHistory(const Json::Value& root) {
        if (!root["payload"].isNull()) {
            const Json::Value& payload = root["payload"];
            LOG_DEBUG << "房间历史消息 - ID: " << payload["id"].asString() 
                    << ", 名称: " << payload["name"].asString();
            
            if (!payload["messages"].isNull() && payload["messages"].isArray()) {
                const Json::Value& messages = payload["messages"];
                
                // 可选：打印每条消息的详细信息
                for (const auto& msg : messages) {
                    LOG_DEBUG << msg["user"]["username"].asString() << ": " 
                             << msg["content"].asString()
                             << " (消息ID: " << msg["id"].asString() << ")";
                }
            }
            
            LOG_DEBUG << "是否有更多消息: " << (payload["hasMoreMessages"].asBool() ? "是" : "否");
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
        if (sent < 0) {
            LOG_ERROR << "发送消息失败";
            return false;
        }
        return true;
    }

private:
    string cookie_;
    bool connected_;
    int sockfd_;
    std::thread receive_thread_;
    std::map<string, string> rooms_;  // roomId -> roomName
    string current_room_id_;
    std::atomic<bool> history_response_received_{false};
    std::mutex history_mutex_;
    std::condition_variable history_cv_;
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