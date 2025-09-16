#include "httplib.h"
#include <iostream>
#include <fstream>
#include <sys/time.h>
#include <cmath>  // 添加这行来使用 std::floor
#include <json/json.h> // jsoncpp头文件

#include "muduo/base/md5.h"
#include "muduo/base/Logging.h"
#include "ws_client.h"


// 默认值定义为常量
const char* DEFAULT_EMAIL = "111@qq.com";   // 默认用户邮箱
const char* DEFAULT_PASSWORD = "1234";      // 默认密码

using namespace std;


class User {
public:
    User(string email, string password) {
        email_ = email;
        MD5 md5(password);
        password_ = md5.toString();
        LOG_INFO << "email: " << email_ << ", password: " << password_;
    }
    void SetCookie(string &cookie) {
        cookie_ = cookie;
    }
    string &GetName() {
        return email_;
    }
    string &GetPassword() {
        return password_;
    }
    string &getCookie() {
        return cookie_;
    }
private:
    string email_;           //用户邮箱
    string password_;       //密码
    string cookie_;
    string  file_full_path_;    //文件路径
};
 


int decodeLoginRespone(string &str_json, string &id, string &message) {
    bool res;
    Json::Value root;
    Json::Reader jsonReader;
    res = jsonReader.parse(str_json, root);
    if (!res) {
        LOG_ERROR << "parse reg json failed ";
        return -1;
    }
  
    if (root["id"].isNull()) {
        LOG_ERROR << "code null";
        return -1;
    }
    id = root["id"].asString();
 
    if (root["message"].isNull()) {
        LOG_ERROR << "message null";
        return -1;
    }
    message = root["message"].asString();

    return 0;
}
 

static uint64_t getMicroseconds() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    long long microseconds = tv.tv_sec * 1000000LL + tv.tv_usec;
    return microseconds;
}  

static uint64_t getMs() {
    return getMicroseconds()/1000;
}  

// 添加解析cookie的函数
string parseCookie(const httplib::Headers& headers) {
    auto it = headers.find("Set-Cookie");
    if (it != headers.end()) {
        string cookie = it->second;
        LOG_INFO << "完整的Set-Cookie内容: [" << cookie << "]";
        // 查找cookie值的开始位置
        size_t start = cookie.find("Cookie");
        if (start != string::npos) {
            start += 12; // "Cookie: sid="的长度
            LOG_INFO << "start: " << start;
            // 查找cookie值的结束位置(分号前)
            size_t end = cookie.find(";", start);
            if (end != string::npos) {
                return cookie.substr(start, end - start);
            } else {
                return cookie.substr(start);
            }
        } else {
            // 如果没找到"Cookie"，尝试直接查找"sid="
            start = cookie.find("sid=");
            if (start != string::npos) {
                start += 4; // "sid="的长度
                LOG_INFO << "start: " << start;
                size_t end = cookie.find(";", start);
                if (end != string::npos) {
                    return cookie.substr(start, end - start);
                } else {
                    return cookie.substr(start);
                }
            }
            LOG_WARN << "can't find Cookie: sid= or sid=";
        }
    }
    return "";
}

int main(int argc, char* argv[]) {
    // 设置日志级别为 INFO
    muduo::Logger::setLogLevel(muduo::Logger::INFO);
    
    // 获取邮箱和密码参数
    string email = DEFAULT_EMAIL;
    string password = DEFAULT_PASSWORD;
    
    if(argc >= 3) {
        email = argv[1];
        password = argv[2];
    } else if(argc == 2) {
        LOG_INFO << "Usage: " << argv[0] << " <email> <password>";
        LOG_INFO << "Using default email and password";
    }

    httplib::Client cli = httplib::Client{HTTP_SERVER_IP, HTTP_SERVER_PORT};
    User user(email, password);

    LOG_INFO << "用户邮箱: " << email << ", 密码: " << password;
    LOG_INFO << "服务器地址: " << HTTP_SERVER_IP << ":" << HTTP_SERVER_PORT;

    //请求登录
    //封装请求
    Json::Value root;
    root["email"] = user.GetName();
    root["password"] = user.GetPassword();
  
    Json::FastWriter writer;
    string req_json = writer.write(root);
    LOG_INFO << "req_json: " << req_json;

    // 创建header
    httplib::Headers headers = {
        { "content-type", "application/json" }
    };

    httplib::Result res = cli.Post("/api/login", headers, req_json, "application/json");
    LOG_INFO << "status:" <<  res->status;
    LOG_INFO << "body:" << res->body;

    if(!res->body.empty()) {
        string id;
        string message;
        int ret = decodeLoginRespone(res->body, id, message);
        if(ret < 0 ) {
            LOG_ERROR << "res->body parse failed";
            
        }
        LOG_ERROR << "id: " << id << ", message: " << message;
        exit(1);
    }

    // 解析cookie
    if (res->status == 204) { // 登录成功
        string cookie = parseCookie(res->headers);
        if (!cookie.empty()) {
            LOG_INFO << "获取到cookie: " << cookie;
            user.SetCookie(cookie);
            
            // 启动聊天客户端
            startChat(cookie);
        } else {
            LOG_ERROR << "未找到cookie";
            exit(1);
        }
    } else {
        LOG_ERROR << "登录失败, status: " << res->status;
        exit(1);
    }

    return 0;
}
