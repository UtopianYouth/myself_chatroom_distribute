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
    result.user_id = 0;

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

int LogicClient::handleMessages(const string& post_data, string& response) {
    // 构造WebSocket格式的请求
    Json::Value request;
    request["type"] = "clientMessages";
    
    // 解析传入的post_data
    Json::Reader reader;
    Json::Value input_data;
    if (!reader.parse(post_data, input_data)) {
        LOG_ERROR << "Failed to parse input post_data";
        return -1;
    }
    
    // 构造payload
    Json::Value payload;
    payload["roomId"] = input_data["roomId"];
    payload["messages"] = input_data["messages"];
    
    request["payload"] = payload;
    
    Json::FastWriter writer;
    string formatted_post_data = writer.write(request);
    
    string url = logic_server_url_ + "/logic/messages";
    
    if (sendHttpRequest(url, formatted_post_data, response)) {
        return 0; // 成功
    } else {
        return -1; // 失败
    }
}

int LogicClient::createRoom(const string& post_data, string& response) {
    // 构造WebSocket格式的请求
    Json::Value request;
    request["type"] = "clientCreateRoom";
    
    // 解析传入的post_data
    Json::Reader reader;
    Json::Value input_data;
    if (!reader.parse(post_data, input_data)) {
        LOG_ERROR << "Failed to parse input post_data";
        return -1;
    }
    
    // 构造payload
    Json::Value payload;
    payload["roomName"] = input_data["roomName"];
    
    request["payload"] = payload;
    
    Json::FastWriter writer;
    string formatted_post_data = writer.write(request);
    
    string url = logic_server_url_ + "/logic/room/create";
    
    if (sendHttpRequest(url, formatted_post_data, response)) {
        return 0; // 成功
    } else {
        return -1; // 失败
    }
}

int LogicClient::getRoomHistory(const string& room_id, const string& first_message_id, 
                               int count, string& response) {
    // 构造WebSocket格式的请求JSON数据
    Json::Value request;
    request["type"] = "requestRoomHistory";
    
    Json::Value payload;
    payload["roomId"] = room_id;
    if (!first_message_id.empty()) {
        payload["firstMessageId"] = first_message_id;
    }
    payload["count"] = count;
    
    request["payload"] = payload;
    
    Json::FastWriter writer;
    string post_data = writer.write(request);
    
    // 使用POST接口
    string url = logic_server_url_ + "/logic/room_history";
    
    if (sendHttpRequest(url, post_data, response)) {
        return 0; // 成功
    } else {
        return -1; // 失败
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
    result.user_id = 0;

    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(response_json, root)) {
        result.error_message = "Failed to parse response JSON";
        LOG_ERROR << "Failed to parse auth response JSON: " << response_json;
        return result;
    }

    if (root["success"].isBool() && root["success"].asBool()) {
        result.success = true;
        result.user_id = root["user_id"].asInt64();
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