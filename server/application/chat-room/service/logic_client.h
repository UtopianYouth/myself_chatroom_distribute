#ifndef __LOGIC_CLIENT_H__
#define __LOGIC_CLIENT_H__

#include <string>
#include <json/json.h>
#include "muduo/base/Logging.h"

using std::string;

struct LogicAuthResult {
    bool success;               // 认证是否成功
    int64_t user_id;            // 用户ID
    string username;            // 用户名
    string email;               // 邮箱
    string error_message;       // 错误信息
};

/**
 * Logic client, interaction with logic server
 */
class LogicClient {
public:
    LogicClient(const string& logic_server_url);
    ~LogicClient();

    /**
     * 通过cookie验证用户身份
     * @param cookie user login cookie
     * @return LogicAuthResult 
     */
    LogicAuthResult verifyUserAuth(const string& cookie);

    /**
     * 用户登录
     * @param post_data 登录请求数据
     * @param response 响应数据
     * @return 0成功，非0失败
     */
    int loginUser(const string& post_data, string& response);

    /**
     * 用户注册
     * @param post_data 注册请求数据
     * @param response 响应数据
     * @return 0成功，非0失败
     */
    int registerUser(const string& post_data, string& response);

    /**
     * 处理客户端消息
     * @param post_data 消息请求数据
     * @param response 响应数据
     * @return 0成功，非0失败
     */
    int handleMessages(const string& post_data, string& response);

    /**
     * 创建房间
     * @param post_data 创建房间请求数据
     * @param response 响应数据
     * @return 0成功，非0失败
     */
    int createRoom(const string& post_data, string& response);

    /**
     * 获取房间历史消息
     * @param room_id 房间ID
     * @param first_message_id 起始消息ID
     * @param count 消息数量
     * @param response 响应数据
     * @return 0成功，非0失败
     */
    int getRoomHistory(const string& room_id, const string& first_message_id, 
                      int count, string& response);

    /**
     * 发送HTTP POST请求
     * @param url 请求URL
     * @param post_data POST数据
     * @param response 响应数据
     * @return true成功，false失败
     */
    bool sendHttpRequest(const string& url, const string& post_data, string& response);

private:
    string logic_server_url_;

    /**
     * 解析认证响应
     * @param response_json 响应JSON
     * @return LogicAuthResult 认证结果
     */
    LogicAuthResult parseAuthResponse(const string& response_json);
};

#endif // __LOGIC_CLIENT_H__