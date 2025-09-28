#include "logic_client.h"
#include <curl/curl.h>
#include <sstream>

/**
 * CURL callback function
 * @param contents curl缓冲区响应内容
 * @param size 数据块大小
 * @param nmemb 数据块数量
 * @param response_ptr 响应字符串指针
 * @return 返回实际写入的数据长度
 */
static size_t WriteCallback(void* contents, size_t data_block_len, 
    size_t data_block_n, string* response_ptr) {
    response_ptr->append((char*)contents, data_block_len * data_block_n);
    return data_block_len * data_block_n;
}

LogicClient::LogicClient(const string& logic_server_url) 
    : logic_server_url_(logic_server_url) {
    // 初始化CURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

LogicClient::~LogicClient() {
    curl_global_cleanup();
}

LogicAuthResult LogicClient::verifyUserAuth(const string& cookie) {
    LogicAuthResult result;
    result.success = false;
    result.user_id = "";

    if (cookie.empty()) {
        result.error_message = "Cookie is empty";
        return result;
    }

    // 构造请求JSON
    Json::Value request;
    request["cookie"] = cookie;
    
    Json::FastWriter writer;
    string post_data = writer.write(request);

    // 发送HTTP请求
    string response;
    string url = logic_server_url_ + "/logic/verify";
    
    if (!sendHttpRequest(url, post_data, response)) {
        result.error_message = "Failed to connect to logic server";
        LOG_ERROR << "Failed to send HTTP request to logic server: " << url;
        return result;
    }

    // 解析响应
    result = parseAuthResponse(response);
    return result;
}

int LogicClient::loginUser(const string& post_data, string& response) {
    string url = logic_server_url_ + "/logic/login";
    
    if (sendHttpRequest(url, post_data, response)) {
        return 0; // 成功
    } else {
        return -1; // 失败
    }
}

int LogicClient::registerUser(const string& post_data, string& response) {
    string url = logic_server_url_ + "/logic/register";
    
    if (sendHttpRequest(url, post_data, response)) {
        return 0; // 成功
    } else {
        return -1; // 失败
    }
}

int LogicClient::handleSend(Json::Value& root, const string& user_id, const string& username, string& response) {
    // 验证payload字段
    if (!root.isMember("payload") || root["payload"].isNull()) {
        LOG_ERROR << "Missing payload in clientMessages";
        return -1;
    }
    
    Json::Value payload = root["payload"];
    
    // 验证必要字段
    if (!payload.isMember("roomId") || !payload.isMember("messages")) {
        LOG_ERROR << "Missing roomId or messages in payload";
        return -1;
    }
    
    string room_id = payload["roomId"].asString();
    Json::Value messages = payload["messages"];
    
    Json::Value request_data;
    request_data["roomId"] = room_id;
    request_data["userId"] = user_id;
    request_data["username"] = username;
    request_data["messages"] = messages;
    
    Json::FastWriter writer;
    string post_data = writer.write(request_data);
    
    LOG_DEBUG << "Sending to logic layer: " << post_data;
    
    // 调用logic层的send接口
    string url = logic_server_url_ + "/logic/send";
    if (!sendHttpRequest(url, post_data, response)) {
        LOG_ERROR << "Failed to send message to logic layer";
        return -1;
    }
    
    // 解析logic层返回的响应
    Json::Value response_root;
    Json::Reader reader;
    if (!reader.parse(response, response_root)) {
        LOG_ERROR << "Failed to parse logic response: " << response;
        return -1;
    }
    
    // 检查logic层是否成功处理
    if (!response_root.isMember("status") || response_root["status"].asString() != "success") {
        LOG_ERROR << "Logic layer failed to process message: " << response;
        return -1;
    }
    
    // logic-> Kafka -> Job -> gRPC -> comet_service.cc -> broadcastRoom
    LOG_INFO << "Message sent to logic layer successfully, will be broadcasted via Kafka->Job->gRPC";

    return 0;
}


int LogicClient::handleRoomHistory(Json::Value& root, const string& user_id, const string& username, string& response) {
    // 验证payload字段
    if (!root.isMember("payload") || root["payload"].isNull()) {
        LOG_ERROR << "Missing payload in requestRoomHistory";
        return -1;
    }
    
    Json::Value payload = root["payload"];
    
    // 验证必要字段
    if (!payload.isMember("roomId") || payload["roomId"].isNull()) {
        LOG_ERROR << "Missing roomId in requestRoomHistory payload";
        return -1;
    }
    
    string room_id = payload["roomId"].asString();
    string first_message_id = payload.get("firstMessageId", "").asString();
    int count = payload.get("count", 10).asInt();
    if (count <= 0) count = 10; // 默认值
    
    // 构造请求数据，添加用户信息用于权限验证
    Json::Value request_data;
    request_data["type"] = "requestRoomHistory";
    
    Json::Value request_payload;
    request_payload["roomId"] = room_id;
    request_payload["firstMessageId"] = first_message_id;
    request_payload["count"] = count;
    request_payload["userId"] = user_id;
    request_payload["username"] = username;
    
    request_data["payload"] = request_payload;
    
    Json::FastWriter writer;
    string post_data = writer.write(request_data);
    
    LOG_DEBUG << "Sending room history request to logic layer: " << post_data;
    
    // 调用logic层的room_history接口
    string url = logic_server_url_ + "/logic/room_history";
    if (!sendHttpRequest(url, post_data, response)) {
        LOG_ERROR << "Failed to send room history request to logic layer";
        return -1;
    }
    
    Json::Value response_root;
    Json::Reader reader;
    if (!reader.parse(response, response_root)) {
        LOG_ERROR << "Failed to parse logic response: " << response;
        return -1;
    }
    
    // 检查WebSocket格式响应
    if (response_root.isMember("type") && response_root["type"].asString() == "serverRoomHistory") {
        // 直接返回WebSocket格式的响应给调用方
        LOG_INFO << "Room history request processed successfully by logic layer";
        return 0;
    } else {
        LOG_ERROR << "Logic layer returned unexpected response format: " << response;
        return -1;
    }
}

int LogicClient::handleCreateRoom(Json::Value& root, const string& user_id, const string& username, string& response) {
    // 验证payload字段
    if (!root.isMember("payload") || root["payload"].isNull()) {
        LOG_ERROR << "Missing payload in clientCreateRoom";
        return -1;
    }
    
    Json::Value payload = root["payload"];
    
    // 验证必要字段
    if (!payload.isMember("roomName") || payload["roomName"].isNull()) {
        LOG_ERROR << "Missing roomName in clientCreateRoom payload";
        return -1;
    }
    
    string room_name = payload["roomName"].asString();
    
    // 构造请求数据，添加用户信息
    Json::Value request_data;
    request_data["type"] = "clientCreateRoom";
    
    Json::Value request_payload;
    request_payload["roomName"] = room_name;
    request_payload["creatorId"] = user_id;
    request_payload["creatorUsername"] = username;
    
    request_data["payload"] = request_payload;
    
    Json::FastWriter writer;
    string post_data = writer.write(request_data);
    
    LOG_DEBUG << "Sending create room request to logic layer: " << post_data;
    
    // 调用logic层的room/create接口
    string url = logic_server_url_ + "/logic/room/create";
    if (!sendHttpRequest(url, post_data, response)) {
        LOG_ERROR << "Failed to send create room request to logic layer";
        return -1;
    }
    
    // 验证响应格式
    Json::Value response_root;
    Json::Reader reader;
    if (!reader.parse(response, response_root)) {
        LOG_ERROR << "Failed to parse logic response: " << response;
        return -1;
    }
    
    if (response_root.isMember("type") && response_root["type"].asString() == "serverCreateRoom") {
        LOG_INFO << "Create room request processed successfully by logic layer";
        return 0;
    } else {
        LOG_ERROR << "Logic layer returned unexpected response format: " << response;
        return -1;
    }
}

bool LogicClient::sendHttpRequest(const string& url, const string& post_data, string& response) {
    CURL* curl;
    CURLcode res;
    bool success = false;

    curl = curl_easy_init();
    if (curl) {
        // 设置URL
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        
        // 设置POST数据
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_data.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, post_data.length());
        
        // 设置HTTP头
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        
        // 设置写回调
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        
        // 设置超时
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 5L);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
        
        // 执行请求
        res = curl_easy_perform(curl);
        
        if (res == CURLE_OK) {
            long response_code;
            curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
            if (response_code == 200 || response_code == 401) {
                success = true;
            } else {
                LOG_ERROR << "HTTP request failed with response code: " << response_code;
            }
        } else {
            LOG_ERROR << "CURL request failed: " << curl_easy_strerror(res);
        }
        
        // 清理
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
    }

    return success;
}

LogicAuthResult LogicClient::parseAuthResponse(const string& response_json) {
    LogicAuthResult result;
    result.success = false;
    result.user_id = "";

    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(response_json, root)) {
        result.error_message = "Failed to parse response JSON";
        LOG_ERROR << "Failed to parse auth response JSON: " << response_json;
        return result;
    }

    if (root["success"].isBool() && root["success"].asBool()) {
        result.success = true;
        result.user_id = root["user_id"].asString();
        result.username = root["username"].asString();
        result.email = root["email"].asString();
        LOG_INFO << "User auth success via logic server, user_id: " << result.user_id 
                 << ", username: " << result.username;
    } else {
        result.error_message = root["error"].asString();
        LOG_WARN << "User auth failed via logic server: " << result.error_message;
    }

    return result;
}