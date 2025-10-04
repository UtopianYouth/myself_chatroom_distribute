#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/base/Logging.h>
#include <functional>
#include <string>
#include <map>
#include <sstream>
#include <json/json.h>
#include "proto/ChatRoom.Job.pb.h"
#include "service/http_parser.h"
#include "service/kafka_producer.h"
#include "api/api_types.h"
#include "api/api_login.h"
#include "api/api_register.h"
#include "api/api_auth.h"
#include "service/message_service.h"
#include "service/room_service.h"

using namespace muduo;
using namespace muduo::net;

// 封装 JSON 字符串为 HTTP 响应
std::string wrapJsonInHttpResponse(const std::string& jsonContent) {
    std::string httpResponse = "HTTP/1.1 200 OK\r\n";
    httpResponse += "Content-Type: application/json\r\n";
    httpResponse += "Content-Length: " + std::to_string(jsonContent.size()) + "\r\n";
    httpResponse += "\r\n";
    httpResponse += jsonContent;
    return httpResponse;
}

class HttpServer {
public:
    HttpServer(EventLoop* loop, const InetAddress& listenAddr)
        : server_(loop, listenAddr, "HttpServer"),
          loop_(loop) {
        server_.setConnectionCallback(
            std::bind(&HttpServer::onConnection, this, _1));
        server_.setMessageCallback(
            std::bind(&HttpServer::onMessage, this, _1, _2, _3));
        server_.setThreadNum(1);     
        
        // 初始化Kafka连接
        if (!producer_.init("localhost:9092", "my-topic")) {
            LOG_ERROR << "Failed to initialize Kafka producer";
        }
    }

    ~HttpServer() {
        producer_.close();  // 确保关闭Kafka连接
    }

    void start() {
        server_.start();
    }

private:
    void onConnection(const TcpConnectionPtr& conn) {
        if (conn->connected()) {
            LOG_INFO << "New connection from " << conn->peerAddress().toIpPort();
        } else {
            LOG_INFO << "Connection closed from " << conn->peerAddress().toIpPort();
        }
    }

    void onMessage(const TcpConnectionPtr& conn, Buffer* buf,
                  Timestamp receiveTime) {
        string msg = buf->retrieveAllAsString();

        LOG_INFO << "Received message: " << msg;

        // parse http request
        if (msg.find("POST /logic/login") != string::npos) {
            handleLogin(conn, msg);
        }
        else if (msg.find("POST /logic/register") != string::npos) {
            handleRegister(conn, msg);
        }
        else if (msg.find("POST /logic/verify") != string::npos) {
            handleVerifyAuth(conn, msg);
        }
        else if (msg.find("POST /logic/hello") != string::npos) {
            handleHello(conn, msg);
        }
        else if (msg.find("POST /logic/send") != string::npos) {
            handleSend(conn, msg);
        }
        else if (msg.find("POST /logic/room_history") != string::npos) {
            handleRoomHistory(conn, msg);
        }
        else if (msg.find("POST /logic/room/create") != string::npos) {
            handleCreateRoom(conn, msg);
        }
        else if (msg.find("POST /logic/rooms") != string::npos) {
            handleGetAllRooms(conn, msg);
        }
        else {
            // 返回404
            string response = "HTTP/1.1 404 Not Found\r\n";
            response += "Content-Length: 0\r\n\r\n";
            conn->send(response);
        }
    }

    void handleLogin(const TcpConnectionPtr& conn, const string& request) {
        // 解析HTTP请求
        string method, path, body;
        if (!HttpParser::parseHttpRequest(request, method, path, body)) {
            LOG_ERROR << "Failed to parse HTTP request";
            sendErrorResponse(conn, 400, "Bad Request");
            return;
        }

        string response_data;
        int ret = ApiLoginUser(body, response_data);
        
        if (ret == 0) {
            // 登录成功，返回cookie
            string response = "HTTP/1.1 200 OK\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Set-Cookie: sid=" + response_data + "; HttpOnly; Max-Age=86400; SameSite=Strict\r\n";
            response += "Content-Length: " + std::to_string(response_data.length()) + "\r\n";
            response += "Access-Control-Allow-Origin: *\r\n";
            response += "\r\n";
            response += response_data;
            conn->send(response);
            LOG_INFO << "User login success";
        } else {
            string response = "HTTP/1.1 400 Bad Request\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Content-Length: " + std::to_string(response_data.length()) + "\r\n";
            response += "Access-Control-Allow-Origin: *\r\n";
            response += "\r\n";
            response += response_data;
            conn->send(response);
            LOG_WARN << "User login failed";
        }
    }

    void handleRegister(const TcpConnectionPtr& conn, const string& request) {
        // 解析HTTP请求
        string method, path, body;
        if (!HttpParser::parseHttpRequest(request, method, path, body)) {
            LOG_ERROR << "Failed to parse HTTP request";
            sendErrorResponse(conn, 400, "Bad Request");
            return;
        }

        // 调用注册API
        string response_data;
        int ret = ApiRegisterUser(body, response_data);
        
        if (ret == 0) {
            // 注册成功，返回cookie
            string response = "HTTP/1.1 200 OK\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Set-Cookie: sid=" + response_data + "; HttpOnly; Max-Age=86400; SameSite=Strict\r\n";
            response += "Content-Length: " + std::to_string(response_data.length()) + "\r\n";
            response += "Access-Control-Allow-Origin: *\r\n";
            response += "\r\n";
            response += response_data;
            conn->send(response);
            LOG_INFO << "User register success";
        } else {
            // 注册失败
            string response = "HTTP/1.1 400 Bad Request\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Content-Length: " + std::to_string(response_data.length()) + "\r\n";
            response += "Access-Control-Allow-Origin: *\r\n";
            response += "\r\n";
            response += response_data;
            conn->send(response);
            LOG_WARN << "User register failed";
        }
    }

    void handleVerifyAuth(const TcpConnectionPtr& conn, const string& request) {
        // 解析HTTP请求
        string method, path, body;
        if (!HttpParser::parseHttpRequest(request, method, path, body)) {
            LOG_ERROR << "Failed to parse HTTP request";
            sendErrorResponse(conn, 400, "Bad Request");
            return;
        }

        // 调用认证验证API
        string response_data;
        int ret = ApiVerifyAuth(body, response_data);
        
        if (ret == 0) {
            // 验证成功
            string response = "HTTP/1.1 200 OK\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Content-Length: " + std::to_string(response_data.length()) + "\r\n";
            response += "Access-Control-Allow-Origin: *\r\n";
            response += "\r\n";
            response += response_data;
            conn->send(response);
            LOG_INFO << "User auth verification success";
        } else {
            // 验证失败
            string response = "HTTP/1.1 401 Unauthorized\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Content-Length: " + std::to_string(response_data.length()) + "\r\n";
            response += "Access-Control-Allow-Origin: *\r\n";
            response += "\r\n";
            response += response_data;
            conn->send(response);
            LOG_WARN << "User auth verification failed";
        }
    }

    void handleHello(const TcpConnectionPtr& conn, const string& request) {
        // 解析HTTP请求
        string method, path, body;
        if (!HttpParser::parseHttpRequest(request, method, path, body)) {
            LOG_ERROR << "Failed to parse HTTP request";
            sendErrorResponse(conn, 400, "Bad Request");
            return;
        }

        // 解析JSON body
        Json::Value json;
        if (!HttpParser::parseJsonBody(body, json)) {
            LOG_ERROR << "Failed to parse JSON body";
            sendErrorResponse(conn, 400, "Invalid JSON");
            return;
        }

        // 验证用户认证信息
        if (!json.isMember("userId") || !json.isMember("username")) {
            LOG_ERROR << "Missing userId or username";
            sendErrorResponse(conn, 400, "Missing user info");
            return;
        }

        string user_id = json["userId"].asString();
        string username = json["username"].asString();

        Json::Value root;
        root["type"] = "hello";

        Json::Value me;
        me["id"] = user_id;
        me["username"] = username;

        Json::Value payload;
        payload["me"] = me;

        // 从内存中获取房间信息
        Json::Value rooms;
        RoomService& room_service = RoomService::getInstance();
        std::vector<Room>& room_list = room_service.getRoomList();
        
        int it_index = 0;
        for (const auto& room_item : room_list) {
            Room room_copy = room_item;
            MessageBatch message_batch;
            MessageStorageManager& storage_mgr = MessageStorageManager::getInstance();

            int ret = storage_mgr.getRoomHistory(room_copy, message_batch);
            if (ret < 0) {
                LOG_ERROR << "getRoomHistory failed for room: " << room_item.room_id;
                continue;
            }

            Json::Value room;
            room["id"] = room_item.room_id;
            room["name"] = room_item.room_name;
            room["creator_id"] = room_item.creator_id;
            room["hasMoreMessages"] = message_batch.has_more;

            Json::Value messages;
            for (int j = 0; j < message_batch.messages.size(); ++j) {
                Json::Value message;
                Json::Value user;
                message["id"] = message_batch.messages[j].id;
                message["content"] = message_batch.messages[j].content;
                user["id"] = message_batch.messages[j].user_id;
                user["username"] = message_batch.messages[j].username;
                message["user"] = user;
                message["timestamp"] = (Json::UInt64)message_batch.messages[j].timestamp;
                messages[j] = message;
            }
            
            if (message_batch.messages.size() > 0) {
                room["messages"] = messages;
            } else {
                room["messages"] = Json::arrayValue;
            }
            
            rooms[it_index] = room;
            ++it_index;
        }

        payload["rooms"] = rooms;
        root["payload"] = payload;
        
        Json::FastWriter writer;
        string response_json = writer.write(root);

        LOG_DEBUG << "Hello response JSON: " << response_json;

        // 发送响应
        string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(response_json.length()) + "\r\n";
        response += "Access-Control-Allow-Origin: *\r\n";
        response += "\r\n";
        response += response_json;
        conn->send(response);
        LOG_INFO << "Hello message handled successfully";
    }

    void handleGetAllRooms(const TcpConnectionPtr& conn, const string& request) {
        // 解析HTTP请求
        string method, path, body;
        if (!HttpParser::parseHttpRequest(request, method, path, body)) {
            LOG_ERROR << "Failed to parse HTTP request";
            sendErrorResponse(conn, 400, "Bad Request");
            return;
        }

        // 直接从内存中获取房间信息，避免重复数据库查询
        RoomService& room_service = RoomService::getInstance();
        std::vector<Room>& room_list = room_service.getRoomList();
        
        // 构造响应JSON
        Json::Value response_root;
        Json::Value rooms_array(Json::arrayValue);
        
        for (const auto& room : room_list) {
            Json::Value room_obj;
            room_obj["id"] = room.room_id;
            room_obj["name"] = room.room_name;
            room_obj["creator_id"] = room.creator_id;
            room_obj["create_time"] = room.create_time;
            room_obj["update_time"] = room.update_time;
            rooms_array.append(room_obj);
        }
        
        response_root["rooms"] = rooms_array;
        response_root["status"] = "success";
        
        Json::FastWriter writer;
        string response_json = writer.write(response_root);
        
        // 发送响应
        string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: " + std::to_string(response_json.length()) + "\r\n";
        response += "Access-Control-Allow-Origin: *\r\n";
        response += "\r\n";
        response += response_json;
        conn->send(response);
        LOG_INFO << "Get all rooms request handled successfully from memory, returned " << room_list.size() << " rooms";
    }

    void handleSend(const TcpConnectionPtr& conn, const string& request) {
        // 解析HTTP请求
        string method, path, body;
        if (!HttpParser::parseHttpRequest(request, method, path, body)) {
            LOG_ERROR << "Failed to parse HTTP request";
            return;
        }

        // 解析JSON body
        Json::Value json;
        if (!HttpParser::parseJsonBody(body, json)) {
            LOG_ERROR << "Failed to parse JSON body";
            return;
        }

        // 验证必要字段
        if (!json.isMember("roomId") || !json.isMember("userId") || 
            !json.isMember("username") || !json.isMember("messages")) {
            LOG_ERROR << "Missing required fields in request";
            return;
        }

        // 创建并填充PushMsg消息
        ChatRoom::Job::PushMsg pushMsg;  //protobuf
        pushMsg.set_type(ChatRoom::Job::PushMsg_Type_PUSH);
        pushMsg.set_operation(4);  // 房间内消息推送
        pushMsg.set_room(json["roomId"].asString());

        // 构造完整的serverMessages格式
        Json::Value messageWrapper;
        messageWrapper["type"] = "serverMessages";
        
        Json::Value payload;
        payload["roomId"] = json["roomId"].asString();
        
        Json::Value messagesArray(Json::arrayValue);
        
        // 准备存储到数据库的消息列表
        std::vector<Message> msgs_to_store;
        
        // 遍历所有消息，将它们添加到同一个消息数组中
        int messageIndex = 0;
        for (const auto& message : json["messages"]) {
            if (!message.isMember("content")) {
                LOG_ERROR << "Message missing content field";
                continue;
            }

            Json::Value messageObj;
            // 生成消息ID
            uint64_t timestamp = Timestamp::now().microSecondsSinceEpoch() / 1000;
            std::string messageId = std::to_string(timestamp) + "-" + std::to_string(messageIndex++);
            messageObj["id"] = messageId;
            messageObj["content"] = message["content"].asString();
            
            Json::Value userObj;
            userObj["id"] = json["userId"].asString();
            userObj["username"] = json["username"].asString();
            messageObj["user"] = userObj;
            
            // 设置时间戳
            messageObj["timestamp"] = (Json::UInt64)timestamp;
            
            messagesArray.append(messageObj);
            
            // 存储到数据库
            Message msg_to_store;
            msg_to_store.id = messageId;
            msg_to_store.content = message["content"].asString();
            msg_to_store.user_id = json["userId"].asString();
            msg_to_store.username = json["username"].asString();
            msg_to_store.timestamp = timestamp;
            msgs_to_store.push_back(msg_to_store);
        }
        
        // 存储消息到Redis和MySQL
        MessageStorageManager& storage_mgr = MessageStorageManager::getInstance();
        if (!msgs_to_store.empty()) {
            int cache_result = storage_mgr.storeMessagesToCache(json["roomId"].asString(),
             msgs_to_store);
            bool cache_success = (cache_result == 0);
            
            bool db_success = storage_mgr.storeMessagesToDB(json["roomId"].asString(),
             msgs_to_store);
            
            
            if (!db_success || !cache_success) {
                //存储失败，仍发送到Kafka
                LOG_ERROR << "Failed to store messages - DB: " << (db_success ? "OK" : "FAILED") 
                         << ", Cache: " << (cache_success ? "OK" : "FAILED");
            } else {
                LOG_INFO << "Successfully stored " << msgs_to_store.size() << " messages to database and cache";
            }
        }

        payload["messages"] = messagesArray;
        messageWrapper["payload"] = payload;

        // 将整个消息对象序列化并设置到protobuf消息中
        Json::FastWriter writer;
        std::string jsonString = writer.write(messageWrapper); 
        LOG_INFO << "Constructed serverMessages: " << jsonString;
        pushMsg.set_msg(jsonString);

        // 序列化protobuf消息
        std::string serialized_msg;
        if (!pushMsg.SerializeToString(&serialized_msg)) {
            LOG_ERROR << "Failed to serialize message";
            string response = "HTTP/1.1 500 Internal Server Error\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Content-Length: 31\r\n\r\n";
            response += "{\"status\": \"send failed\"}\r\n";
            conn->send(response);
            return;
        }
        
        LOG_INFO << "Serialized protobuf message length: " << serialized_msg.length();
        
        // 发送序列化后的消息到Kafka
        if (!producer_.sendMessage(serialized_msg)) {
            LOG_ERROR << "Failed to send message to Kafka";
            string response = "HTTP/1.1 500 Internal Server Error\r\n";
            response += "Content-Type: application/json\r\n";
            response += "Content-Length: 31\r\n\r\n";
            response += "{\"status\": \"send failed\"}\r\n";
            conn->send(response);
            return;
        }

        // 发送成功响应 - 使用wrapJsonInHttpResponse函数
        std::string jsonStr = "{\"status\": \"success\"}";
        std::string httpResponse = wrapJsonInHttpResponse(jsonStr);
        
        conn->send(httpResponse);
        LOG_INFO << "handleSend completed successfully";
    }

    void handleRoomHistory(const TcpConnectionPtr& conn, const string& request) {
        // 解析HTTP请求
        string method, path, body;
        if (!HttpParser::parseHttpRequest(request, method, path, body)) {
            LOG_ERROR << "Failed to parse HTTP request";
            sendErrorResponse(conn, 400, "Bad Request");
            return;
        }

        // 解析JSON body - 按照WebSocket格式解析
        Json::Value json;
        if (!HttpParser::parseJsonBody(body, json)) {
            LOG_ERROR << "Failed to parse JSON body";
            sendErrorResponse(conn, 400, "Invalid JSON");
            return;
        }

        // 验证WebSocket消息格式: {"type":"requestRoomHistory","payload":{"roomId":"xxx","firstMessageId":"xxx","count":10,"userId":123,"username":"user"}}
        if (!json.isMember("type") || !json.isMember("payload")) {
            LOG_ERROR << "Missing type or payload fields";
            sendErrorResponse(conn, 400, "Missing required fields");
            return;
        }

        string type = json["type"].asString();
        Json::Value payload = json["payload"];

        if (type == "requestRoomHistory") {
            // 验证payload字段
            if (!payload.isMember("roomId") || !payload.isMember("userId") || !payload.isMember("username")) {
                LOG_ERROR << "Missing roomId, userId or username in payload";
                sendErrorResponse(conn, 400, "Missing required fields");
                return;
            }

            string room_id = payload["roomId"].asString();
            string first_message_id = payload.get("firstMessageId", "").asString();
            int count = payload.get("count", 10).asInt();
            string user_id = payload["userId"].asString();
            string username = payload["username"].asString();

            // 调用历史消息获取API（使用单例模式）
            Room room;
            room.room_id = room_id;
            room.history_last_message_id = first_message_id;
            
            MessageBatch message_batch;
            MessageStorageManager& storage_mgr = MessageStorageManager::getInstance();
            int ret = storage_mgr.getRoomHistory(room, message_batch, count);
            
            if (ret == 0) {
                // 构造 serverRoomHistory 响应（WebSocket格式）
                Json::Value response_root;
                response_root["type"] = "serverRoomHistory";
                
                Json::Value response_payload;
                response_payload["roomId"] = room_id;
                response_payload["name"] = ""; // 房间名称，如果需要可以从数据库获取
                response_payload["hasMoreMessages"] = message_batch.has_more;
                
                Json::Value messages_array(Json::arrayValue);
                for (const auto& msg : message_batch.messages) {
                    Json::Value message_obj;
                    message_obj["id"] = msg.id;
                    message_obj["content"] = msg.content;
                    message_obj["timestamp"] = (Json::UInt64)msg.timestamp;
                    
                    Json::Value user_obj;
                    user_obj["id"] = msg.user_id;
                    user_obj["username"] = msg.username;
                    message_obj["user"] = user_obj;
                    
                    messages_array.append(message_obj);
                }
                response_payload["messages"] = messages_array;
                response_root["payload"] = response_payload;
                
                // 直接返回HTTP响应
                Json::FastWriter writer;
                std::string jsonString = writer.write(response_root);
                LOG_INFO << "Returning room history directly via HTTP: " << jsonString;
                
                // 使用标准HTTP响应方式（参考handleCreateRoom）
                string response = "HTTP/1.1 200 OK\r\n";
                response += "Content-Type: application/json\r\n";
                response += "Content-Length: " + std::to_string(jsonString.length()) + "\r\n";
                response += "Access-Control-Allow-Origin: *\r\n";
                response += "\r\n";
                response += jsonString;
                conn->send(response);
                
                LOG_INFO << "Room history returned directly via HTTP for user: " << user_id;
                
            } else {
                // 获取历史消息失败
                LOG_ERROR << "Failed to get room history from storage";
                sendErrorResponse(conn, 500, "Failed to get room history");
            }
        } else {
            sendErrorResponse(conn, 400, "Unsupported message type");
        }
    }

    void handleCreateRoom(const TcpConnectionPtr& conn, const string& request) {
        string method, path, body;
        if (!HttpParser::parseHttpRequest(request, method, path, body)) {
            LOG_ERROR << "Failed to parse HTTP request";
            sendErrorResponse(conn, 400, "Bad Request");
            return;
        }

        Json::Value json;
        if (!HttpParser::parseJsonBody(body, json)) {
            LOG_ERROR << "Failed to parse JSON body";
            sendErrorResponse(conn, 400, "Invalid JSON");
            return;
        }

        // 验证WebSocket消息格式: {"type":"clientCreateRoom","payload":{"roomName":"xxx"}}
        if (!json.isMember("type") || !json.isMember("payload")) {
            LOG_ERROR << "Missing type or payload fields";
            sendErrorResponse(conn, 400, "Missing required fields");
            return;
        }

        string type = json["type"].asString();
        Json::Value payload = json["payload"];

        if (type == "clientCreateRoom") {
            // 验证payload字段
            if (!payload.isMember("roomName")) {
                LOG_ERROR << "Missing roomName in payload";
                sendErrorResponse(conn, 400, "Missing roomName field");
                return;
            }
            
            // 验证创建者信息字段
            if (!payload.isMember("creatorId") || !payload.isMember("creatorUsername")) {
                LOG_ERROR << "Missing creatorId or creatorUsername in payload";
                sendErrorResponse(conn, 400, "Missing creator information");
                return;
            }

            string room_name = payload["roomName"].asString();
            string creator_id = payload["creatorId"].asString();
            string creator_username = payload["creatorUsername"].asString();
            
            LOG_INFO << "Creating room: " << room_name << " by user: " << creator_username << " (ID: " << creator_id << ")";

            string response_json;

            string room_id;
            RoomService& room_service = RoomService::getInstance();
            string error_msg;

            // 检查房间名是否已存在
            if (room_service.roomExistByName(room_name)) {
                LOG_WARN << "Room name already exists: " << room_name;
                
                Json::Value failed_response;
                failed_response["type"] = "serverCreateRoom";
                
                Json::Value response_payload;
                response_payload["roomId"] = ""; // roomId = "" 表示房间名已存在
                response_payload["roomName"] = "";
                response_payload["creatorId"] = "";
                response_payload["creatorUsername"] = "";
                failed_response["payload"] = response_payload;
                
                Json::FastWriter writer;
                string failed_json = writer.write(failed_response);
                
                string response = "HTTP/1.1 200 OK\r\n";
                response += "Content-Type: application/json\r\n";
                response += "Content-Length: " + std::to_string(failed_json.length()) + "\r\n";
                response += "Access-Control-Allow-Origin: *\r\n";
                response += "\r\n";
                response += failed_json;
                conn->send(response);
                return;
            }

            int ret = room_service.createRoom(room_name, creator_id, creator_username, room_id, error_msg);
            if (ret != 0) {
                response_json = error_msg;
            }
            bool success = (ret == 0);
            
            if (success) {
                Json::Value success_response;
                success_response["type"] = "serverCreateRoom";
                
                Json::Value response_payload;
                response_payload["roomId"] = room_id;
                response_payload["roomName"] = room_name;
                response_payload["creatorId"] = creator_id;
                response_payload["creatorUsername"] = creator_username;
                success_response["payload"] = response_payload;
                
                Json::FastWriter writer;
                string success_json = writer.write(success_response);
                
                string response = "HTTP/1.1 200 OK\r\n";
                response += "Content-Type: application/json\r\n";
                response += "Content-Length: " + std::to_string(success_json.length()) + "\r\n";
                response += "Access-Control-Allow-Origin: *\r\n";
                response += "\r\n";
                response += success_json;
                conn->send(response);
                
                // kafka 广播房间创建消息
                ChatRoom::Job::PushMsg pushMsg;
                pushMsg.set_type(ChatRoom::Job::PushMsg_Type_BROADCAST);
                pushMsg.set_operation(5);  // 5: 创建房间
                pushMsg.set_room("global");  // 全局广播
                pushMsg.set_msg(success_json);
                
                LOG_INFO << "Preparing Kafka broadcast message:";
                LOG_INFO << "  Room: " << pushMsg.room();
                LOG_INFO << "  Message: " << pushMsg.msg();
                
                // 序列化protobuf消息
                std::string serialized_msg;
                if (pushMsg.SerializeToString(&serialized_msg)) {
                    // 发送到Kafka
                    if (producer_.sendMessage(serialized_msg)) {
                        LOG_INFO << "Room creation broadcast sent to Kafka successfully";
                    }
                    else {
                        LOG_ERROR << "Failed to send room creation broadcast to Kafka";
                    }
                }
                else {
                    LOG_ERROR << "Failed to serialize room creation broadcast message";
                }

                LOG_INFO << "Room created successfully: " << room_id;

            } 
            else {
                sendErrorResponse(conn, 500, "Failed to create room: " + response_json);
            }
        }
        else {
            sendErrorResponse(conn, 400, "Unsupported message type");
        }
    }

    void sendErrorResponse(const TcpConnectionPtr& conn, int code, const string& message) {
        string response = "HTTP/1.1 " + std::to_string(code) + " " + message + "\r\n";
        response += "Content-Type: application/json\r\n";
        response += "Content-Length: 0\r\n";
        response += "Access-Control-Allow-Origin: *\r\n";
        response += "\r\n";
        conn->send(response);
    }

    void parseRoomHistoryParams(const string& query, string& room_id, string& first_message_id, int& count) {
        std::istringstream iss(query);
        string param;
        
        while (std::getline(iss, param, '&')) {
            size_t eq_pos = param.find('=');
            if (eq_pos != string::npos) {
                string key = param.substr(0, eq_pos);
                string value = param.substr(eq_pos + 1);
                
                if (key == "room_id") {
                    room_id = value;
                } else if (key == "first_message_id") {
                    first_message_id = value;
                } else if (key == "count") {
                    count = std::stoi(value);
                }
            }
        }
    }

private:
    TcpServer server_;
    EventLoop* loop_;
    std::map<string, bool> loggedInUsers_; // 存储已登录用户
    KafkaProducer producer_;  // 添加producer成员变量

};

int main() {
    LOG_INFO << "Logic server starting...";

    // 初始化mysql和redis 
    MessageStorageManager& storage_mgr = MessageStorageManager::getInstance();
    if (!storage_mgr.init("logic.conf")) {
        LOG_ERROR << "Failed to initialize MessageStorageManager";
        return -1;
    }
    LOG_INFO << "Database and cache pools initialized successfully";

    // 初始化房间服务
    RoomService& room_service = RoomService::getInstance();
    if (room_service.initialize() < 0) {
        LOG_ERROR << "Failed to initialize rooms in Logic layer";
        return -1;
    }
    LOG_INFO << "Room service initialized successfully in Logic layer";

    // 创建Kafka实例
    KafkaProducer producer;

    LOG_INFO << "HTTP server starting...";
    EventLoop loop;
    InetAddress listenAddr(8090);
    HttpServer server(&loop, listenAddr);
    
    server.start();
    loop.loop();
    return 0;
}
