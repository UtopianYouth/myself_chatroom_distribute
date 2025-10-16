#include<cstring>
#include <thread>
#include<sstream>

#include "api_types.h"
#include "pub_sub_service.h"
#include "websocket_conn.h"
#include "base64.h"
#include "logic_client.h"
#include "logic_config.h"

typedef struct WebSocketFrame {
    bool fin;
    uint8_t opcode;
    bool mask;
    uint64_t payload_length;
    uint8_t masking_key[4];
    std::string payload_data;
}WebSocketFrame;

// 全局变量定义
std::unordered_map<string, CHttpConnPtr> s_user_ws_conn_map;       // store user id and websocket conn
std::mutex s_mtx_user_ws_conn_map;
ThreadPool* CWebSocketConn::s_thread_pool = nullptr;                // thread pool handles for websocket conn

// handshake
string GenerateWebSocketHandshakeResponse(const string& key) {
    string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    string accept_key = key + magic;

    unsigned char sha1[20];     // openssl
    SHA1(reinterpret_cast<const unsigned char*>(accept_key.data()), accept_key.size(), sha1);

    string accept = base64_encode(sha1, 20);    // base64 code

    string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
    return response;
}

// create websocket frame
string BuildWebSocketFrame(const string& payload, const uint8_t opcode) {
    string frame;

    frame.push_back(0x80 | (opcode & 0x0F));

    size_t payload_length = payload.size();
    if (payload_length <= 125) {
        frame.push_back(static_cast<uint8_t>(payload_length));
    }
    else if (payload_length <= 65535) {
        frame.push_back(126);
        frame.push_back(static_cast<uint8_t>((payload_length >> 8) & 0xFF));
        frame.push_back(static_cast<uint8_t>(payload_length & 0xFF));
    }
    else {
        frame.push_back(127);
        for (int i = 7; i >= 0; i--) {
            frame.push_back(static_cast<uint8_t>((payload_length >> (8 * i)) & 0xFF));
        }
    }

    frame += payload;

    return frame;
}

// parse http data and get websocket frame
WebSocketFrame ParseWebSocketFrame(const std::string& data) {
    WebSocketFrame frame;
    const uint8_t* bytes = reinterpret_cast<const uint8_t*>(data.data());

    // parse first byte
    frame.fin = (bytes[0] & 0x80) != 0;
    frame.opcode = bytes[0] & 0x0F;

    // parse second byte
    frame.mask = (bytes[1] & 0x80) != 0;
    frame.payload_length = bytes[1] & 0x7F;

    size_t offset = 2;

    // parse 
    if (frame.payload_length == 126) {
        frame.payload_length = (bytes[2] << 8) | bytes[3];
        offset += 2;
    }
    else if (frame.payload_length == 127) {
        frame.payload_length =
            (static_cast<uint64_t>(bytes[2]) << 56) |
            (static_cast<uint64_t>(bytes[3]) << 48) |
            (static_cast<uint64_t>(bytes[4]) << 40) |
            (static_cast<uint64_t>(bytes[5]) << 32) |
            (static_cast<uint64_t>(bytes[6]) << 24) |
            (static_cast<uint64_t>(bytes[7]) << 16) |
            (static_cast<uint64_t>(bytes[8]) << 8) |
            static_cast<uint64_t>(bytes[9]);
        offset += 8;
    }

    // parse mask
    if (frame.mask) {
        std::memcpy(frame.masking_key, bytes + offset, 4);
        offset += 4;
    }

    // parse payload_data
    frame.payload_data.assign(data.begin() + offset, data.end());

    // parse masking-key
    if (frame.mask) {
        for (size_t i = 0; i < frame.payload_data.size(); i++) {
            frame.payload_data[i] ^= frame.masking_key[i % 4];
        }
    }

    return frame;
}

string ExtractSid(const string& input) {
    // get location of "sid=" 
    size_t sid_start = input.find("sid=");
    if (sid_start == string::npos) {
        return "";
    }

    // the length of "sid=" is 4
    sid_start += 4;

    // get end location of sid, ' ' or '&' or ';'
    size_t sid_end = input.find_first_of(" &;", sid_start);

    if (sid_end == string::npos) {
        sid_end = input.length();
    }

    // get sid value
    string sid_value = input.substr(sid_start, sid_end - sid_start);

    return sid_value;
}

// get current timestamp
uint64_t getCurrentTimestamp() {
    auto now = std::chrono::system_clock::time_point::clock::now();
    auto duration = now.time_since_epoch();
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);
    return milliseconds.count(); // ms
}

// =========================CWebSocketConn======================
CWebSocketConn::CWebSocketConn(const TcpConnectionPtr& conn)
    :CHttpConn(conn), handshake_completed(false) {
    LOG_INFO << "Constructor CWebSocketConn";
}

CWebSocketConn::~CWebSocketConn() {
    LOG_INFO << "Destructor CWebSocketConn" << this->user_id << ", stats_total_messages: " << this->stats_total_messages
        << ", stats_total_bytes: " << this->stats_total_bytes;
}


void CWebSocketConn::OnRead(Buffer* buf) {
    if (!this->handshake_completed) {
        // WebSocket handshake
        string request = buf->retrieveAllAsString();
        LOG_DEBUG << "request: " << request;

        // if don't judge Websocket frame, please judgement first
        size_t key_start = request.find("Sec-WebSocket-Key: ");
        if (key_start != std::string::npos) {
            key_start += 19;
            size_t key_end = request.find("\r\n", key_start);
            std::string key = request.substr(key_start, key_end - key_start);

            LOG_DEBUG << "key: " << key;

            // generate handshake response
            std::string response = GenerateWebSocketHandshakeResponse(key);

            // send http response
            this->send(response);
            this->handshake_completed = true;
            LOG_DEBUG << "WebSocket handshake completed";

            // verify cookie
            string Cookie = this->headers_["Cookie"];
            LOG_DEBUG << "Cookie: " << Cookie;

            string sid;
            string email;

            if (!Cookie.empty()) {
                sid = ExtractSid(Cookie);      // get cookie data
            }
            LOG_DEBUG << "sid: " << sid;
            
            // logic层进行用户认证
            LogicConfig& config = LogicConfig::getInstance();
            LogicClient logic_client(config.getLogicServerUrl());
            LogicAuthResult auth_result = logic_client.verifyUserAuth(sid);
            
            if (Cookie.empty() || !auth_result.success) {
                string reason = auth_result.error_message.empty() ? 
                    "Cookie validation failed" : auth_result.error_message;

                // validation failed, send close websocket frame
                LOG_WARN << "cookie validation failed via logic server, reason: " << reason;
                this->SendCloseFrame(1008, reason);
            }
            else {
                // 认证成功，设置用户信息
                this->user_id = auth_result.user_id;
                this->username = auth_result.username;
                
                // get chatrooms that current user join
                LOG_DEBUG << "cookie validation ok via logic server, user_id: " << this->user_id 
                         << ", username: " << this->username;

                s_mtx_user_ws_conn_map.lock();
                // insert current userid and websocket
                // shared_from_this() ==> ensure one ControlBlock, one WebSocketConn
                auto existing_conn = s_user_ws_conn_map.find(this->user_id);
                if (existing_conn != s_user_ws_conn_map.end()) {
                    s_user_ws_conn_map.erase(existing_conn);
                    LOG_DEBUG << "old websocket conn founded and will be cover, user_id: " << this->user_id;
                }
                // the same userid maybe exists, because websocket conn maybe don't close
                // force insert(update)
                s_user_ws_conn_map[this->user_id] = this->shared_from_this();
                s_mtx_user_ws_conn_map.unlock();

                // join the rooms
                std::vector<Room>& room_list = PubSubService::GetRoomList();    // get all the default chatroom
                for (const auto& room : room_list) {
                    this->rooms_map.insert({ room.room_id, room });
                }

                // send message to client, get history message and add subscribe to all chatrooms(join)
                this->SendHelloMessage();
            }
        }
        else {
            LOG_ERROR << "no Sec-Websocket-Key";
        }
    }
    else {
        // loop handle WebSocket frame
        this->incomplete_frame_buffer += buf->retrieveAllAsString();
        LOG_DEBUG << "current buffer length: " << this->incomplete_frame_buffer.length();
        while (!this->incomplete_frame_buffer.empty()) {
            // 
            if (this->incomplete_frame_buffer.length() < 2) {
                LOG_DEBUG << "Not enough data for frame header, waiting for more...";
                return;
            }

            const uint8_t* bytes = reinterpret_cast<const uint8_t*>(this->incomplete_frame_buffer.data());
            uint64_t payload_len = bytes[1] & 0x7F;
            size_t header_length = 2;

            // calculate payload length
            if (payload_len == 126) {
                if (this->incomplete_frame_buffer.length() < 4) {
                    LOG_DEBUG << "not enough data for extended length(2 bytes), waiting for more...";
                    return;
                }
                header_length += 2;
                payload_len = (bytes[2] << 8) | bytes[3];
            }
            else if (payload_len == 127) {
                if (this->incomplete_frame_buffer.length() < 10) {
                    LOG_DEBUG << "not enough data for extended length(8 bytes), waiting for more...";
                    return;
                }
                header_length += 8;
                payload_len = ((uint64_t)bytes[2] << 56) | ((uint64_t)bytes[3] << 48) |
                    ((uint64_t)bytes[4] << 40) | ((uint64_t)bytes[5] << 32) |
                    ((uint64_t)bytes[6] << 24) | ((uint64_t)bytes[7] << 16) |
                    ((uint64_t)bytes[8] << 8) | bytes[9];
            }

            // is there mask 
            bool has_mask = (bytes[1] & 0x80) != 0;
            if (has_mask) {
                header_length += 4;
            }

            // the length of one websocket frame
            size_t total_frame_length = header_length + payload_len;

            // check is enough data for one websocket frame
            if (this->incomplete_frame_buffer.length() < total_frame_length) {
                LOG_DEBUG << "not enough data for complete frame, waiting for more... "
                    << "need: " << total_frame_length
                    << ", have: " << this->incomplete_frame_buffer.length();
                return;
            }

            // complete websocket frame ==> buffer maybe have more than one websocket frame
            if (this->incomplete_frame_buffer.length() >= total_frame_length) {
                // copy one complete websocket frame and delete handled complete websocket frame
                string frame_data = this->incomplete_frame_buffer.substr(0, total_frame_length);
                this->incomplete_frame_buffer = this->incomplete_frame_buffer.substr(total_frame_length);

                // shared_from_this() ==> copy constructor of shared_ptr
                auto self = shared_from_this();

                // push websocket handle task to thread pool
                this->s_thread_pool->run([this, self, frame_data]()
                    {
                        // parse websocket frame
                        WebSocketFrame frame = ParseWebSocketFrame(frame_data);
                        ++this->stats_total_messages;
                        this->stats_total_bytes += frame.payload_data.size();

                        // get thread id
                        std::ostringstream oss;
                        oss << std::this_thread::get_id();
                        LOG_DEBUG << "pool thread id: " << oss.str() << ", stats_total_messages: "
                            << this->stats_total_messages << ", stats_total_bytes: " << this->stats_total_bytes;

                        // handle frame
                        if (frame.opcode == 0x01) {
                            // text frame
                            LOG_DEBUG << "process text frame, payload: " << frame.payload_data;
                            bool res;
                            Json::Value root;
                            Json::Reader jsonReader;
                            res = jsonReader.parse(frame.payload_data, root);
                            if (!res) {
                                LOG_WARN << "parse json failed ";
                                return;
                            }
                            else {
                                string type;
                                if (root.isObject() && !root["type"].isNull()) {
                                    type = root["type"].asString();
                                    if (type == "clientMessages") {
                                        HandleClientMessages(root);
                                    }
                                    else if (type == "requestRoomHistory") {
                                        HandleRequestRoomHistory(root);
                                    }
                                    else if (type == "clientCreateRoom") {
                                        HandleClientCreateRoom(root);
                                    }
                                }
                                else {
                                    LOG_ERROR << "data no a json object";
                                }
                            }
                        }
                        else if (frame.opcode == 0x08) {
                            // close frame
                            LOG_DEBUG << "received close frame, closing connection...";
                            this->Disconnect();
                        }
                    }
                );
            }
        }
    }
}

void CWebSocketConn::SendCloseFrame(uint16_t code, const string& reason) {
    if (!tcp_conn_) {
        return;
    }

    // create websocket close frame
    char frame[2 + reason.size()];
    frame[0] = (code >> 8) & 0xFF;  // higt position of status code
    frame[1] = code & 0xFF;         // low position of status code
    std::memcpy(frame + 2, reason.data(), reason.size());

    // send websocket close frame
    tcp_conn_->send(frame, sizeof(frame));
}

int CWebSocketConn::SendHelloMessage() {

    // 调用 logic 层 hello 接口
    string logic_server_addr = LogicConfig::getInstance().getLogicServerUrl();
    LogicClient logic_client(logic_server_addr);
    
    // 构造hello请求数据
    Json::Value hello_request;
    hello_request["userId"] = this->user_id;
    hello_request["username"] = this->username;
    
    Json::FastWriter writer;
    string post_data = writer.write(hello_request);
    
    string response_json;
    string url = logic_server_addr + "/logic/hello";
    if (!logic_client.sendHttpRequest(url, post_data, response_json)) {
        LOG_ERROR << "Failed to call logic hello API";
        return -1;
    }
    
    // 解析 logic 层返回的 JSON
    Json::Reader reader;
    Json::Value logic_response;
    if (!reader.parse(response_json, logic_response)) {
        LOG_ERROR << "Failed to parse logic hello response JSON";
        return -1;
    }
    
    LOG_INFO << "Logic hello response: " << response_json;
    
    // 为用户订阅所有房间
    if (logic_response.isMember("payload") && logic_response["payload"].isMember("rooms")) {
        Json::Value rooms = logic_response["payload"]["rooms"];
        for (int i = 0; i < rooms.size(); ++i) {
            Json::Value room = rooms[i];
            if (room.isMember("id")) {
                string room_id = room["id"].asString();
                string room_name = room.get("name", "").asString();
                
                // 添加到本地房间映射
                Room local_room;
                local_room.room_id = room_id;
                local_room.room_name = room_name;
                this->rooms_map[room_id] = local_room;
                
                // 订阅房间
                PubSubService::GetInstance().AddSubscriber(room_id, this->user_id);
                LOG_DEBUG << "User " << this->user_id << " subscribed to room: " << room_id;
            }
        }
    }
    

    std::string hello_frame = BuildWebSocketFrame(response_json);
    this->send(hello_frame);
    
    LOG_INFO << "Hello message sent successfully for user: " << this->user_id;

    return 0;
}

int CWebSocketConn::HandleClientMessages(Json::Value& root) {
    // 转发给logic层处理
    string logic_server_addr = LogicConfig::getInstance().getLogicServerUrl();
    LogicClient logic_client(logic_server_addr);
    string response_json;
    
    // 调用logic层的handleSend
    int ret = logic_client.handleSend(root, this->user_id, this->username, response_json);
    
    if (ret == 0) {
        LOG_INFO << "Message sent to logic layer successfully, will be broadcasted via Kafka->Job->gRPC";
    } else {
        LOG_ERROR << "Failed to send message to logic layer";
    }
    
    return ret;
}

int CWebSocketConn::HandleRequestRoomHistory(Json::Value& root) {
    string logic_server_addr = LogicConfig::getInstance().getLogicServerUrl();
    LogicClient logic_client(logic_server_addr);
    string response_json;
    
    // 调用logic层的handleRoomHistory方法
    int ret = logic_client.handleRoomHistory(root, this->user_id,
        this->username, response_json);
    
    if (ret == 0) {
        LOG_INFO << "Room history request sent to logic layer successfully, response will be sent directly";
        
        // 构造WebSocket帧
        string websocket_frame = BuildWebSocketFrame(response_json);
        this->send(websocket_frame);
        
        LOG_INFO << "Room history WebSocket message sent to client: " << response_json;
    } else {
        LOG_ERROR << "Failed to send room history request to logic layer";
    }
    
    return ret;
}

// request format: {"type":"clientCreateRoom","payload":{"roomName":"dpdk"}} 
// response format: {"type":"serverCreateRoom","payload":{"roomId":"3bb1b0b6-e91c-11ef-ba07-bd8c0260908d", "roomName":"dpdk"}}
int CWebSocketConn::HandleClientCreateRoom(Json::Value& root) {
    string logic_server_addr = LogicConfig::getInstance().getLogicServerUrl();
    LogicClient logic_client(logic_server_addr);
    string response_json;
    
    // 调用logic层handleCreateRoom
    int ret = logic_client.handleCreateRoom(root, this->user_id,
        this->username, response_json);
    
    if (ret != 0) {
        LOG_ERROR << "Failed to send create room request to logic layer";
        return -1;
    }
    
    LOG_INFO << "Create room request sent to logic layer successfully";
    
    // 解析logic层返回的响应以获取房间信息
    Json::Value response_root;
    Json::Reader reader;
    if (!reader.parse(response_json, response_root)) {
        LOG_ERROR << "Failed to parse logic response: " << response_json;
        return -1;
    }
    
    // 验证响应格式并获取房间信息
    if (!response_root.isMember("type") || response_root["type"].asString() != "serverCreateRoom" ||
        !response_root.isMember("payload") || response_root["payload"].isNull()) {
        LOG_ERROR << "Logic layer returned unexpected response format: " << response_json;
        return -1;
    }
    
    Json::Value payload = response_root["payload"];
    if (!payload.isMember("roomId") || !payload.isMember("roomName")) {
        LOG_ERROR << "Missing roomId or roomName in logic response: " << response_json;
        return -1;
    }
    
    // 获取新创建的房间信息
    string room_id = payload["roomId"].asString();
    string room_name = payload["roomName"].asString();
    
    if (room_id.empty()) {
        LOG_INFO << "Room name duplicated, sending failure response to client for room: (" << room_name << ")";
        Json::Value resp;
        resp["type"] = "serverCreateRoom";
        Json::Value respPayload;
        respPayload["roomId"] = "";
        respPayload["roomName"] = room_name;
        respPayload["creatorId"] = this->user_id;
        resp["payload"] = respPayload;
        Json::FastWriter writer;
        std::string resp_json = writer.write(resp);
        std::string websocket_frame = BuildWebSocketFrame(resp_json, 0x01);
        this->send(websocket_frame);
        return 0;
    }
    
    LOG_INFO << "Room created successfully: " << room_id << " (" << room_name << ")";
    return 0;
}

void CWebSocketConn::Disconnect() {
    if (tcp_conn_) {
        LOG_INFO << "tcp_conn_ sendCloseFrame";
        // send webSocket close frame
        this->SendCloseFrame(1000, "Normal closure");
        tcp_conn_->shutdown();
    }

    {
        std::lock_guard<std::mutex> lock(s_mtx_user_ws_conn_map);
        auto existing_conn = s_user_ws_conn_map.find(this->user_id);
        if (existing_conn != s_user_ws_conn_map.end()) {
            LOG_DEBUG << "disconnect, userid: " << this->user_id << " from s_user_ws_conn_map erase";
            LOG_DEBUG << "1 s_user_ws_conn_map.size(): " << s_user_ws_conn_map.size();

            s_user_ws_conn_map.erase(existing_conn);

            LOG_DEBUG << "2 s_user_ws_conn_map.size(): " << s_user_ws_conn_map.size();
        }
    }
}

void CWebSocketConn::InitThreadPool(int thread_num) {
    LOG_INFO << "InitThreadPool, thread_num:" << thread_num;

    if (!s_thread_pool) {
        s_thread_pool = new ThreadPool("WebSocketConnThreadPool");
        s_thread_pool->start(thread_num);
    }
}