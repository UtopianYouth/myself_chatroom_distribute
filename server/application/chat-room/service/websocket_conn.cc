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

std::unordered_map<int64_t, CHttpConnPtr> s_user_ws_conn_map;
std::mutex s_mtx_user_ws_conn_map;

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

    // get end location of sid
    size_t sid_end = input.find_first_of(";", sid_start);

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
    LOG_INFO << "Destructor CWebSocketConn";
}

void CWebSocketConn::OnRead(Buffer* buf) {
    if (!this->handshake_completed) {
        // WebSocket handshake
        string request = buf->retrieveAllAsString();
        LOG_INFO << "request: " << request;

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
            LOG_DEBUG << "sid:" << sid;
            int ret = ApiGetUsernameAndUseridByCookie(sid, this->username, this->user_id, email);
            if (Cookie.empty() || ret < 0) {
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
                LOG_INFO << "cookie validation ok";

                s_mtx_user_ws_conn_map.lock();
                // insert current userid and websocket
                // shared_from_this() ==> ensure one ControlBlock, one WebSocketConn
                auto existing_conn = s_user_ws_conn_map.find(this->user_id);
                if (existing_conn != s_user_ws_conn_map.end()) {
                    s_user_ws_conn_map.erase(existing_conn);
                    LOG_INFO << "old s_user_ws_conn_map.erase(), userid:" << this->user_id;
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
        // parse websocket frame
        std::string request = buf->retrieveAllAsString();
        WebSocketFrame frame = ParseWebSocketFrame(request);
        LOG_INFO << "client to server, parse websocket frame after: " << frame.payload_data;

        if (frame.opcode == 0x01) {
            // 0x01: text frame
            //parse type field, handle various message by type field 
            bool res;
            Json::Value root;
            Json::Reader jsonReader;
            res = jsonReader.parse(frame.payload_data, root);
            if (!res) {
                LOG_ERROR << "parse websocket message json failed";
                this->Disconnect();
                return;
            }

            // get type field
            string  type;
            if (root["type"].isNull()) {
                LOG_ERROR << "type null";
                this->Disconnect();
                return;
            }
            type = root["type"].asString();

            if (type == "clientMessages") {
                this->HandleClientMessages(root);
            }
            else if (type == "requestRoomHistory") {
                this->HandleRequestRoomHistory(root);
            }
            else {
                LOG_ERROR << "unknown type: " << type;
            }
        }
        else if (frame.opcode == 0x08) {
            // 0x8: close frame
            LOG_INFO << "Received close frame, closing connection...";
            this->Disconnect();
        }
        else if (frame.opcode == 0x09) {
            // 0x09: ping frame
            LOG_INFO << "Received Ping frame, sending Pong frame...";
            this->SendPongFrame();
        }
        else {
            LOG_ERROR << "can't handle opcode: " << frame.opcode;
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
    Json::Value root;
    root["type"] = "hello";

    Json::Value me;
    me["id"] = static_cast<uint32_t>(this->user_id);
    me["username"] = this->username;

    Json::Value payload;
    payload["me"] = me;

    Json::Value rooms;
    int it_index = 0;
    for (auto it = this->rooms_map.begin(); it != this->rooms_map.end(); ++it) {
        Room& room_item = it->second;
        PubSubService::GetInstance().AddSubscriber(room_item.room_id, this->user_id);
        string last_message_id;
        MessageBatch message_batch;

        // get chat-room message
        int ret = ApiGetRoomHistory(room_item, message_batch);
        if (ret < 0) {
            LOG_ERROR << "ApiGetRoomHistory failed";
            return -1;
        }
        LOG_INFO << "room: " << room_item.room_name << ", history_last_message_id:" << it->second.history_last_message_id;

        Json::Value  room;
        room["id"] = room_item.room_id;             // chatroom id
        room["name"] = room_item.room_name;         // chatroom name
        room["hasMoreMessages"] = message_batch.has_more;

        Json::Value messages;
        for (int j = 0; j < message_batch.messages.size(); ++j) {
            Json::Value message;
            Json::Value user;
            message["id"] = message_batch.messages[j].id;
            message["content"] = message_batch.messages[j].content;
            user["id"] = (Json::Int64)message_batch.messages[j].user_id;
            user["username"] = message_batch.messages[j].username;
            message["user"] = user;
            message["timestamp"] = (Json::UInt64)message_batch.messages[j].timestamp;
            messages[j] = message;
        }
        if (message_batch.messages.size() > 0) {
            room["messages"] = messages;
        }
        else {
            // front-end will be error if room["messages"] is nullptr
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

int CWebSocketConn::HandleClientMessages(Json::Value& root) {
    string room_id;
    if (root["payload"].isNull()) {
        return -1;
    }
    Json::Value payload = root["payload"];

    if (payload["roomId"].isNull()) {
        return -1;
    }
    room_id = payload["roomId"].asString();

    if (payload["messages"].isNull()) {
        return -1;
    }
    Json::Value arrayObj = payload["messages"];

    if (arrayObj.isNull()) {
        return -1;
    }

    std::vector<Message> msgs;
    uint64_t timestamp = getCurrentTimestamp();

    // why many messages?
    for (int i = 0; i < arrayObj.size(); ++i) {
        Json::Value message = arrayObj[i];
        Message msg;
        msg.content = message["content"].asString();
        msg.timestamp = timestamp;
        msg.user_id = this->user_id;
        msg.username = this->username;
        msgs.push_back(msg);
    }

    // store to redis
    ApiStoreMessage(room_id, msgs);

    // json encode
    root = Json::Value();
    payload = Json::Value();
    root["type"] = "serverMessages";
    payload["roomId"] = room_id;
    Json::Value messages;
    for (int i = 0; i < msgs.size();++i) {
        Json::Value message;
        Json::Value user;
        message["id"] = msgs[i].id;
        message["content"] = msgs[i].content;
        user["id"] = static_cast<uint32_t>(this->user_id);
        user["username"] = this->username;
        message["user"] = user;
        message["timestamp"] = (Json::UInt64)msgs[i].timestamp;
        messages[i] = message;
    }

    if (msgs.size() > 0) {
        payload["messages"] = messages;
    }
    else {
        // the front-end will be error if payload["messages"] is nullptr
        payload["messages"] = Json::arrayValue;
    }

    root["payload"] = payload;
    Json::FastWriter writer;
    string str_json = writer.write(root);
    LOG_INFO << "serverMessages: " << str_json;
    string response = BuildWebSocketFrame(str_json);

    auto callback = [&response, &room_id, this](const std::unordered_set<int64_t>& user_ids)
        {
            LOG_INFO << "room_id:" << room_id << ", callback " << ", user_ids.size(): " << user_ids.size();
            // traverse user_ids, user_ids is from RoomTopic
            for (int64_t user_id : user_ids) {
                CHttpConnPtr ws_conn_ptr = nullptr;
                {
                    std::lock_guard<std::mutex> lock(s_mtx_user_ws_conn_map);
                    ws_conn_ptr = s_user_ws_conn_map[user_id];

                }
                if (ws_conn_ptr) {
                    ws_conn_ptr->send(response);
                }
                else {
                    LOG_WARN << "can't find userid: " << user_id;
                }
            }
        };

    PubSubService::GetInstance().PubSubMessage(room_id, callback);
}

void CWebSocketConn::SendPongFrame() {
    if (!tcp_conn_) {
        return;
    }
    // 0x8A: Pong frame
    char frame[2] = { 0x8A, 0x00 };
    tcp_conn_->send(frame, sizeof(frame));
}

bool CWebSocketConn::IsCloseFrame(const string& frame) {
    if (frame.empty()) {
        return false;
    }
    uint8_t opcode = static_cast<uint8_t>(frame[0]) & 0x0F;

    // 0x8 is close frame 
    return opcode == 0x8;
}

int CWebSocketConn::HandleRequestRoomHistory(Json::Value& root) {
    string roomId = root["payload"]["roomId"].asString();
    string firstMessageId = root["payload"]["firstMessageId"].asString();
    int count = root["payload"]["count"].asInt();
    count = 10;

    // get history message from redis
    Room& room = this->rooms_map[roomId];
    MessageBatch  message_batch;
    room.history_last_message_id = firstMessageId;

    int ret = ApiGetRoomHistory(room, message_batch);
    if (ret < 0) {
        LOG_ERROR << "ApiGetRoomHistory failed";
        return -1;
    }

    //封装消息
    root = Json::Value(); //重新置空
    Json::Value payload;

    root["type"] = "serverRoomHistory";
    payload["roomId"] = roomId;
    payload["name"] = room.room_name;
    payload["hasMoreMessages"] = message_batch.has_more;

    // create json of message array
    Json::Value messages;
    for (int j = 0; j < message_batch.messages.size(); j++) {
        Json::Value  message;
        Json::Value user;
        message["id"] = message_batch.messages[j].id;
        message["content"] = message_batch.messages[j].content;
        user["id"] = (Json::Int64)message_batch.messages[j].user_id;
        user["username"] = message_batch.messages[j].username;
        message["user"] = user;
        message["timestamp"] = (Json::UInt64)message_batch.messages[j].timestamp;
        messages.append(message);
    }

    if (message_batch.messages.size() > 0) {
        payload["messages"] = messages;
    }
    else {
        // front-end whill error if payload["meassages"] is nullptr, payload["messages"] = []
        payload["messages"] = Json::arrayValue;
    }

    root["payload"] = payload;
    Json::FastWriter writer;
    string str_json = writer.write(root);
    string response = BuildWebSocketFrame(str_json);
    send(response);
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
            LOG_INFO << "s_user_ws_conn_map.erase(), userid:" << this->user_id;
            s_user_ws_conn_map.erase(existing_conn);
        }
    }
}