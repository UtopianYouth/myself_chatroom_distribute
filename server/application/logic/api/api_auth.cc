#include "api_auth.h"
#include "muduo/base/Logging.h"
#include <json/json.h>

/**
 * 通过cookie验证用户身份
 */
AuthResult ApiVerifyUserByCookie(const string& cookie) {
    AuthResult result;
    result.success = false;
    result.user_id = 0;
    
    if (cookie.empty()) {
        result.error_message = "Cookie is empty";
        return result;
    }
    
    // 调用现有的cookie验证函数
    string username, email;
    int64_t user_id;
    int ret = ApiGetUsernameAndUseridByCookie(cookie, username, user_id, email);
    
    if (ret == 0) {
        result.success = true;
        result.user_id = user_id;
        result.username = username;
        result.email = email;
        LOG_INFO << "User auth success, user_id: " << user_id << ", username: " << username;
    } else {
        result.error_message = "Invalid cookie or user not found";
        LOG_WARN << "User auth failed for cookie: " << cookie;
    }
    
    return result;
}

/**
 * 编码认证结果为JSON
 */
void EncodeAuthResultJson(const AuthResult& result, string& json_str) {
    Json::Value root;
    root["success"] = result.success;
    
    if (result.success) {
        root["user_id"] = (Json::UInt64)result.user_id;
        root["username"] = result.username;
        root["email"] = result.email;
    } else {
        root["error"] = result.error_message;
    }
    
    Json::FastWriter writer;
    json_str = writer.write(root);
}

/**
 * 解析认证请求JSON
 */
int DecodeAuthRequestJson(const string& json_str, string& cookie) {
    Json::Value root;
    Json::Reader reader;
    
    if (!reader.parse(json_str, root)) {
        LOG_ERROR << "Failed to parse auth request JSON";
        return -1;
    }
    
    if (root["cookie"].isNull()) {
        LOG_ERROR << "Cookie field is missing in auth request";
        return -1;
    }
    
    cookie = root["cookie"].asString();
    return 0;
}

/**
 * 处理用户认证验证请求
 */
int ApiVerifyAuth(const string& post_data, string& response_data) {
    string cookie;
    
    // 解析请求JSON
    if (DecodeAuthRequestJson(post_data, cookie) < 0) {
        AuthResult result;
        result.success = false;
        result.error_message = "Invalid request format";
        EncodeAuthResultJson(result, response_data);
        return -1;
    }
    
    // 验证用户身份
    AuthResult result = ApiVerifyUserByCookie(cookie);
    
    // 编码响应
    EncodeAuthResultJson(result, response_data);
    
    return result.success ? 0 : -1;
}