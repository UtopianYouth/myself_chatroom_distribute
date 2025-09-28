#ifndef __LOGIC_CLIENT_H__
#define __LOGIC_CLIENT_H__

#include <string>
#include <json/json.h>
#include "muduo/base/Logging.h"

using std::string;

struct LogicAuthResult {
    bool success;               // 认证是否成功
    string user_id;             // 用户ID (UUID)
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
     * @param root WebSocket消息的JSON根对象
     * @param user_id 用户ID
     * @param username 用户名
     * @param response 响应数据
     * @return 0成功，非0失败
     */
    int handleSend(Json::Value& root, const string& user_id, const string& username, string& response);

    /**
     * 处理房间历史消息请求（分布式模式）
     * @param root WebSocket消息的JSON根对象
     * @param user_id 用户ID
     * @param username 用户名
     * @param response 响应数据
     * @return 0成功，非0失败
     */
    int handleRoomHistory(Json::Value& root, const string& user_id, const string& username, string& response);
    
    /**
     * 处理创建房间请求（分布式模式）
     * @param root WebSocket消息的JSON根对象
     * @param user_id 用户ID
     * @param username 用户名
     * @param response 响应数据
     * @return 0成功，非0失败
     */
    int handleCreateRoom(Json::Value& root, const string& user_id, const string& username, string& response);

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