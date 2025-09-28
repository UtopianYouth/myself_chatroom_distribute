#ifndef API_AUTH_H
#define API_AUTH_H

#include <string>
#include "api_common.h"
#include "db_pool.h"

using std::string;

/**
 * 验证用户认证信息的结构体
 */
struct AuthResult {
    bool success;               // 认证是否成功
    string user_id;             // 用户ID (UUID)
    string username;            // 用户名
    string email;               // 邮箱
    string error_message;       // 错误信息
};

/**
 * 通过cookie验证用户身份
 * @param cookie 用户cookie
 * @return AuthResult 认证结果
 */
AuthResult ApiVerifyUserByCookie(const string& cookie);

/**
 * 编码认证结果为JSON
 * @param result 认证结果
 * @param json_str 输出的JSON字符串
 */
void EncodeAuthResultJson(const AuthResult& result, string& json_str);

/**
 * 解析认证请求JSON
 * @param json_str 输入的JSON字符串
 * @param cookie 输出的cookie
 * @return 0成功，-1失败
 */
int DecodeAuthRequestJson(const string& json_str, string& cookie);

/**
 * 处理用户认证验证请求
 * @param post_data 请求数据
 * @param response_data 响应数据
 * @return 0成功，-1失败
 */
int ApiVerifyAuth(const string& post_data, string& response_data);

#endif // API_AUTH_H