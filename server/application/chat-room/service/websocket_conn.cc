#include<unordered_map>
#include<cstring>
#include "api_types.h"
#include "api_message.h"
#include "pub_sub_service.h"
#include "websocket_conn.h"
#include "base64.h"

typedef struct WebSocketFrame {
    bool fin;
    uint8_t opcode;
    bool mask;
    uint64_t payload_length;
    uint8_t masking_key[4];
    std::string payload_data;
}WebSocketFrame;

std::unordered_map<int32_t, CHttpConnPtr> s_user_ws_conn_map;
std::mutex s_mtx_user_ws_conn_map;

string ExtractSid(const string& input) {
    // get location of "sid=" 
    size_t sid_start = input.find("sid=");
    if (sid_start == string::npos) {
        return "";
    }

    // the length of "sid=" is 4
    sid_start += 4;

    // get end location of sid
    size_t sid_end = input.find_first_of(";", sid_start);

    if (sid_end == string::npos) {
        sid_end = input.length();
    }

    // get sid value
    string sid_value = input.substr(sid_start, sid_end - sid_start);

    return sid_value;
}

// handshake
string GenerateWebSocketHandshakeResponse(const string& key) {
    string magic = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    string accept_key = key + magic;

    unsigned char sha1[20];
    SHA1(reinterpret_cast<const unsigned char*>(accept_key.data()), accept_key.size(), sha1);

    string accept = base64_encode(sha1, 20);

    string response =
        "HTTP/1.1 101 Switching Protocols\r\n"
        "Upgrade: websocket\r\n"
        "Connection: Upgrade\r\n"
        "Sec-WebSocket-Accept: " + accept + "\r\n\r\n";
    return response;
}

// parse http data: get websocket frame
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
        frame.payload_length = (static_cast<uint64_t>(bytes[2]) << 56) |
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

// create Websocket frame
string BuildWebSocketFrame(const string& payload, uint8_t opcode = 0x01) {
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

// =========================CWebSocketConn======================
CWebSocketConn::CWebSocketConn(const TcpConnectionPtr& conn)
    :CHttpConn(conn), handshake_completed(false) {
    LOG_INFO << "Constructor CWebSocketConn";
}

CWebSocketConn::~CWebSocketConn() {
    LOG_INFO << "Destructor CWebSocketConn";
    if (this->userid != -1) {
        for (auto it = this->rooms_map.begin();it != rooms_map.end();++it) {
            // delete user from chat room
            PubSubService::GetInstance().DeleteSubscriber(it->second.room_id, this->userid);
        }
        std::lock_guard<std::mutex> lock(s_mtx_user_ws_conn_map);
        CHttpConnPtr ws_conn_ptr = s_user_ws_conn_map[this->userid];

        if (ws_conn_ptr && ws_conn_ptr.get() == this) {
            // tips: 这里只所有这么做，是因为多个客户端登陆时，可能已经被挤下线，已经被移除了
            s_user_ws_conn_map.erase(this->userid);
        }
    }
}

void CWebSocketConn::OnRead(Buffer* buf) {
    if (!this->handshake_completed) {
        // WebSocket handshake
        std::string request(buf->retrieveAllAsString());
        // get url
        this->url_ = getSubdirectoryFromHttpRequest(request);

        if (request.find("Upgrade: websocket") != std::string::npos) {
            size_t key_start = request.find("Sec-WebSocket-Key: ");
            if (key_start != std::string::npos) {
                key_start += 19;
                size_t key_end = request.find("\r\n", key_start);
                std::string key = request.substr(key_start, key_end - key_start);

                // generate handshake response
                std::string response = GenerateWebSocketHandshakeResponse(key);

                // send http response
                this->send(response);

                this->handshake_completed = true;

                LOG_INFO << "WebSocket handshake completed";

                // verify cookie
                string Cookie = headers_["Cookie"];
                string sid;
                string email;

                if (!Cookie.empty()) {
                    sid = ExtractSid(Cookie);      // get cookie data
                    LOG_INFO << "sid = " << sid;
                }

                if (Cookie.empty() || ApiGetUsernameAndUseridByCookie(sid, this->username, this->userid, email) != 0) {
                    LOG_WARN << "cookie validate failed" << ", Cookie.empty(): " << Cookie.empty()
                        << ", username:" << this->username;

                    string reason;
                    if (email.empty()) {
                        reason = "Cookie validat failed";
                    }
                    else {
                        // redis validate cookie and get email success, mysql don't get username
                        reason = "mysql db failed";
                    }

                    // send close websocket frame
                    this->SendCloseFrame(1008, reason);
                }
                else {
                    // get chatrooms that current user join
                    std::vector<Room>& room_list = GetRoomList();
                    for (const auto& room : room_list) {
                        rooms_map.insert({ room.room_id, room });        //这里是固定的房间信息
                    }

                    //处理校验正常的逻辑
                    //先查询是否有同样userid的连接存在
                    // s_mtx_user_ws_conn_map_.lock();
                    // CHttpConnPtr ws_conn_ptr =  s_user_ws_conn_map[userid_];
                    // if(ws_conn_ptr) {
                    //     // 如果存在则移除
                    //     s_user_ws_conn_map.erase(userid_);
                    //     s_mtx_user_ws_conn_map_.unlock();
                    //     ws_conn_ptr->disconnect();
                    // }else {
                    //     s_mtx_user_ws_conn_map_.unlock();
                    // }

                    s_mtx_user_ws_conn_map.lock();
                    // insert current userid and websocket
                    // shared_from_this() ==> ensure one ControlBlock, one WebSocketConn
                    s_user_ws_conn_map.insert({ this->userid, this->shared_from_this() });
                    s_mtx_user_ws_conn_map.unlock();

                    // send message to client
                    SendHelloMessage();
                }
            }
        }
    }
    else {
        // parse websocket frame
        std::string data = buf->retrieveAllAsString();
        WebSocketFrame frame = ParseWebSocketFrame(data);

        if (frame.opcode == 0x01) {
            // 0x01: text frame
            LOG_INFO << "Received WebSocket message: " << frame.payload_data;
            //parse type field, handle various message by type field 
            bool res;
            Json::Value root;
            Json::Reader jsonReader;
            res = jsonReader.parse(frame.payload_data, root);

            if (!res) {
                LOG_ERROR << "parse websocket message json failed";
                Disconnect();
                return;
            }
            // get type field
            string  type;
            if (root["type"].isNull()) {
                LOG_ERROR << "type null";
                Disconnect();
                return;
            }
            type = root["type"].asString();
            if (type == "clientMessages") {
                HandleClientMessage(root);
            }
            else {
                LOG_ERROR << "can't handle " << type;
            }
        }
        else if (frame.opcode == 0x08) {
            // 0x8: close frame
            LOG_INFO << "Received close frame, closing connection...";
            Disconnect();
        }
        else if (frame.opcode == 0x09) {
            // 0x09: ping frame
            LOG_INFO << "Received Ping frame, sending Pong frame...";
            SendPongFrame();
        }
        else {
            LOG_ERROR << "can't handle opcode " << frame.opcode;
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

void CWebSocketConn::SendPongFrame() {
    if (!tcp_conn_) {
        return;
    }
    // 0x8A: Pong frame
    char frame[2] = { 0x8A, 0x00 };
    tcp_conn_->send(frame, sizeof(frame));
}

void CWebSocketConn::Disconnect() {
    if (tcp_conn_) {
        // send webSocket close frame
        this->SendCloseFrame(1000, "Normal closure");
        tcp_conn_->shutdown();
    }

    std::lock_guard<std::mutex> lock(s_mtx_user_ws_conn_map);

    CHttpConnPtr ws_conn_ptr = s_user_ws_conn_map[this->userid];

    if (ws_conn_ptr && ws_conn_ptr.get() == this) {
        // tips: 这里只所有这么做，是因为多个客户端登陆时，可能已经被挤下线，已经被移除了
        LOG_INFO << "s_user_ws_conn_map.erase userid: " << this->userid;
        s_user_ws_conn_map.erase(this->userid);
    }
}

bool CWebSocketConn::IsCloseFrame(const string& frame) {
    if (frame.empty()) {
        return false;
    }
    uint8_t opcode = static_cast<uint8_t>(frame[0]) & 0x0F;

    // 0x8 is close frame 
    return opcode == 0x8;
}

void CWebSocketConn::SendHelloMessage() {
    Json::Value root;
    root["type"] = "hello";

    Json::Value me;
    me["id"] = this->userid;
    me["username"] = this->username;

    Json::Value payload;
    payload["me"] = me;

    Json::Value rooms;
    int it_index = 0;
    for (auto it = this->rooms_map.begin(); it != rooms_map.end(); ++it)
    {
        Room& room_item = it->second;
        PubSubService::GetInstance().AddSubscriber(room_item.room_id, this->userid);
        string  last_message_id;
        MessageBatch  message_batch;

        // get chat-room message
        ApiGetRoomHistory(room_item, message_batch);
        LOG_INFO << "room: " << room_item.room_name << ", history_last_message_id:" << it->second.history_last_message_id;

        Json::Value  room;
        room["id"] = room_item.room_id;             // chatroom id
        room["name"] = room_item.room_name;         // chatroom name
        room["hasMoreMessages"] = message_batch.has_more;

        Json::Value  messages;
        for (int j = 0; j < message_batch.messages.size(); ++j) {
            Json::Value message;
            Json::Value user;
            message["id"] = message_batch.messages[j].id;
            message["content"] = message_batch.messages[j].content;
            user["id"] = this->userid;
            user["username"] = this->username;
            message["user"] = user;
            message["timestamp"] = (Json::UInt64)message_batch.messages[j].timestamp;
            messages[j] = message;
        }
        if (message_batch.messages.size() > 0) {
            room["messages"] = messages;
        }
        else {
            // front-end will be error if room["message"] is nullptr
            room["messages"] = Json::arrayValue;
        }
        rooms[it_index] = room;
        ++it_index;
    }

    payload["rooms"] = rooms;
    root["payload"] = payload;
    Json::FastWriter writer;
    string str_json = writer.write(root);

    LOG_INFO << "Serialized JSON: " << str_json;

    // encapsulate websocket frame
    std::string hello = BuildWebSocketFrame(str_json);
    send(hello);
}

void CWebSocketConn::HandleClientMessage(Json::Value& root) {
    //{"type":"clientMessages","payload":{"roomId":"老周讲golang","messages":[{"content":"3"}]}}
    string roomId;
    Json::Value payload;
    if (root["payload"].isObject()) {
        payload = root["payload"];
    }
    roomId = payload["roomId"].asString();
    const Json::Value arrayObj = payload["messages"];
    auto now = timestamp_t::clock::now();
    auto duration = now.time_since_epoch();  // clock start ~ clock current(milliseconds)
    auto milliseconds = std::chrono::duration_cast<std::chrono::milliseconds>(duration);  // convert seconds
    printf("=============%ld=============\n", milliseconds);
    uint64_t timestamp = milliseconds.count();

    std::vector<Message> msgs;
    for (int i = 0; i < arrayObj.size(); i++) {
        msgs.emplace_back(Message{
            "",  // blank ID, will be assigned by Redis
            std::move(arrayObj[i]["content"].asString()),
            timestamp,
            this->userid,
            });
    }

    // store to redis
    int ret = ApiStoreMessage(roomId, msgs);

    // json encode
    root = Json::Value();
    payload["roomId"] = roomId;

    Json::Value messages;
    for (int i = 0; i < msgs.size();++i) {
        Json::Value message;
        Json::Value user;
        message["id"] = msgs[i].id;
        message["content"] = msgs[i].content;
        user["id"] = this->userid;
        user["username"] = this->username;
        message["user"] = user;
        message["timestamp"] = (Json::UInt64)msgs[i].timestamp;
        messages[i] = message;
    }

    if (msgs.size() > 0) {
        payload["message"] = messages;
    }
    else {
        // the front-end will be error if payload["message"] is nullptr
        payload["message"] = Json::arrayValue;
    }

    root["type"] = "serverMessages";
    root["payload"] = payload;
    Json::FastWriter writer;
    string str_json = writer.write(root);

    auto callback = [&str_json, &roomId, &msgs, this](std::unordered_set<int32_t>& userIds)->void {
        LOG_INFO << "userIds.size = " << userIds.size();
        // traverse all userIds
        for (int32_t userId : userIds) {
            LOG_INFO << "userId: " << userId << ", str_json: " << str_json;
            s_mtx_user_ws_conn_map.lock();
            CHttpConnPtr ws_conn_ptr = s_user_ws_conn_map[userId];
            s_mtx_user_ws_conn_map.unlock();

            if (ws_conn_ptr) {
                // encapsulated websocket frame
                string response = BuildWebSocketFrame(str_json);
                ws_conn_ptr->send(response);
            }
            else {
                LOG_WARN << "userId: " << userId << " connection can't find";
            }
        }
        };

    LOG_INFO << "broadcast messages to chatroom: " << roomId;
    PubSubService::GetInstance().PubSubPublish(roomId, callback);
}