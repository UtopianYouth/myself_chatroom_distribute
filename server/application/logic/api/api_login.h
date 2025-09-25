#ifndef API_LOGIN_H
#define API_LOGIN_H

#include <string>
#include "api_common.h"
#include "db_pool.h"
#include "util.h"

using std::string;

/**
 * 解析登录JSON请求
 */
int DecodeLoginJson(const string& str_json, string& email, string& password);

/**
 * 编码登录响应JSON
 */
int EncodeLoginJson(api_error_id input, string message, string& str_json);

/**
 * 验证用户密码
 */
int VerifyUserPassword(string& email, string& password);

/**
 * 用户登录API
 */
int ApiLoginUser(string& post_data, string& response_data);

#endif // API_LOGIN_H