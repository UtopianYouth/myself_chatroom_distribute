#ifndef __LOGIC_CONFIG_H__
#define __LOGIC_CONFIG_H__

#include <string>
#include "config_file_reader.h"

using std::string;

/**
 * Logic服务配置管理类
 */
class LogicConfig {
public:
    static LogicConfig& getInstance();
    
    /**
     * init logic server url
     * @param config_file 配置文件路径
     * @return true成功，false失败
     */
    bool init(const string& config_file);
    
    const string& getLogicServerUrl() const { return logic_server_url_; }
    
    // 获取连接超时时间（秒）
    int getConnectTimeout() const { return connect_timeout_; }
    
    // 获取请求超时时间（秒）
    int getRequestTimeout() const { return request_timeout_; }

private:
    LogicConfig() = default;
    ~LogicConfig() = default;
    LogicConfig(const LogicConfig&) = delete;
    LogicConfig& operator=(const LogicConfig&) = delete;
    
    string logic_server_url_ = "http://localhost:8090";  // 默认logic服务器地址
    int connect_timeout_ = 3;                            // 连接超时时间（秒）
    int request_timeout_ = 5;                            // 请求超时时间（秒）
};

#endif // __LOGIC_CONFIG_H__