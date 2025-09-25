#ifndef API_REGISTER_H
#define API_REGISTER_H

#include <string>
#include "api_common.h"
#include "db_pool.h"
#include "util.h"

using std::string;

/**
 * 解析注册JSON请求
 */
int DecodeRegisterJson(const string& str_json, string& username,
    string& email, string& password);

/**
 * 编码注册响应JSON
 */
void EncodeRegisterJson(api_error_id input, string message, string& str_json);

/**
 * 注册用户
 */
int RegisterUser(string& username, string& email, string& password, api_error_id& error_id);

/**
 * 用户注册API
 */
int ApiRegisterUser(string& post_data, string& response_data);

#endif // API_REGISTER_H