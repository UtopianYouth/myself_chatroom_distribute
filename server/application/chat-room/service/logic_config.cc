#include "logic_config.h"
#include "muduo/base/Logging.h"

LogicConfig& LogicConfig::getInstance() {
    static LogicConfig instance;
    return instance;
}

bool LogicConfig::init(const string& config_file) {
    CConfigFileReader config_reader(config_file.c_str());
    
    // logic server host
    char* logic_server_host = config_reader.GetConfigName("logic_server_host");
    
    // logic server port
    char* logic_server_port = config_reader.GetConfigName("logic_server_port");
    
    if (logic_server_host && logic_server_port) {
        logic_server_url_ = "http://" + string(logic_server_host) + ":" + string(logic_server_port);
        LOG_INFO << "Logic server URL configured: " << logic_server_url_;
    } else {
        LOG_WARN << "Logic server config not found, using default: " << logic_server_url_;
    }
    
    // 读取超时配置
    char* connect_timeout_str = config_reader.GetConfigName("logic_connect_timeout");
    if (connect_timeout_str) {
        connect_timeout_ = atoi(connect_timeout_str);
        LOG_INFO << "Logic connect timeout configured: " << connect_timeout_ << "s";
    }
    
    char* request_timeout_str = config_reader.GetConfigName("logic_request_timeout");
    if (request_timeout_str) {
        request_timeout_ = atoi(request_timeout_str);
        LOG_INFO << "Logic request timeout configured: " << request_timeout_ << "s";
    }
    
    return true;
}