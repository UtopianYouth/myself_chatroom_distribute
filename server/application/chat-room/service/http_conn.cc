#include "http_conn.h"
#include <fstream>
#include <sstream>
#include <string.h>
#include <regex>
#include "muduo/base/Logging.h" // Logger日志头文件
#include "logic_client.h"
#include "logic_config.h"



CHttpConn::CHttpConn(TcpConnectionPtr tcp_conn) :
    tcp_conn_(tcp_conn)
{
    this->uuid_ = std::any_cast<uint32_t>(tcp_conn_->getContext());
    LOG_DEBUG << "Constructor CHttpConn uuid: " << this->uuid_;
}

CHttpConn::~CHttpConn() {
    LOG_DEBUG << "Destructor CHttpConn uuid: " << uuid_;
}

void CHttpConn::OnRead(Buffer* buf) // CHttpConn业务层面的OnRead
{
    const char* in_buf = buf->peek();
    int32_t len = buf->readableBytes();

    http_parser_.ParseHttpContent(in_buf, len);
    if (http_parser_.IsReadAll()) {
        string url = http_parser_.GetUrlString();
        string content = http_parser_.GetBodyContentString();
        LOG_INFO << "url: " << url << ", content: " << content;

        if (strncmp(url.c_str(), "/api/login", 10) == 0) { // 登录
            _HandleLoginRequest(url, content);
        }
        else if (strncmp(url.c_str(), "/api/create-account", 18) == 0) {   //  创建账号
            _HandleRegisterRequest(url, content);
        }
        else {
            char* resp_content = new char[256];
            string str_json = "{\"code\": 1}";
            uint32_t len_json = str_json.size();
            //暂时先放这里
#define HTTP_RESPONSE_REQ                                                     \
                "HTTP/1.1 404 OK\r\n"                                                      \
                "Connection:close\r\n"                                                     \
                "Content-Length:%d\r\n"                                                    \
                "Content-Type:application/json;charset=utf-8\r\n\r\n%s"
            snprintf(resp_content, 256, HTTP_RESPONSE_REQ, len_json, str_json.c_str());
            tcp_conn_->send(resp_content);
        }
    }

}

void CHttpConn::send(const string& data) {
    LOG_DEBUG << "send:" << data;
    tcp_conn_->send(data.c_str(), data.size());
}
/**
假设输入的 HTTP 请求头为：

GET /chat/subdir HTTP/1.1
Host: 127.0.0.1:8080
Upgrade: websocket
Connection: Upgrade
Sec-WebSocket-Key: dGhlIHNhbXBsZSBub25jZQ==
Sec-WebSocket-Version: 13

程序运行后会输出：
Subdirectory: /chat/subdir
 */
std::string  CHttpConn::getSubdirectoryFromHttpRequest(const std::string& httpRequest) {
    // 正则表达式匹配请求行中的路径部分
    std::regex pathRegex(R"(GET\s+([^\s]+)\s+HTTP\/1\.1)");
    std::smatch pathMatch;

    // 查找路径
    if (std::regex_search(httpRequest, pathMatch, pathRegex)) {
        return pathMatch[1];  // 返回路径部分
    }

    return "";  // 如果没有找到路径，返回空字符串
}

// register 
int CHttpConn::_HandleRegisterRequest(string& url, string& post_data) {
    string response_json;
    
    // 通过HTTP客户端转发到logic层
    string logic_server_addr = LogicConfig::getInstance().getLogicServerUrl();
    LogicClient logic_client(logic_server_addr);
    int ret = logic_client.registerUser(post_data, response_json);
    
    char* http_response_data = new char[HTTP_RESPONSE_JSON_MAX];
    int http_code = 200;
    string http_code_msg;

    if (ret == 0) {
        // register success
        LOG_INFO << "register success, cookie: " << response_json;
        http_code = 204;
        http_code_msg = "No content";

        // encapsulate http response
        snprintf(http_response_data, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_WITH_COOKIE,
            http_code, http_code_msg.c_str(), response_json.c_str(), (size_t)0, "");
    }
    else {
        // register failed
        LOG_INFO << "register failed, response_json: " << response_json;
        http_code = 400;
        http_code_msg = "Bad Request";

        // encapsulate http response
        snprintf(http_response_data, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_WITH_CODE,
            http_code, http_code_msg.c_str(), (size_t)response_json.length(), response_json.c_str());
    }

    tcp_conn_->send(http_response_data);
    LOG_INFO << " http_response_data: " << http_response_data;
    delete[] http_response_data;

    return 0;
}

// login
int CHttpConn::_HandleLoginRequest(string& url, string& post_data) {
    string response_json;
    
    // 通过HTTP客户端转发到logic层
    string logic_server_addr = LogicConfig::getInstance().getLogicServerUrl();
    LogicClient logic_client(logic_server_addr);
    int ret = logic_client.loginUser(post_data, response_json);
    
    char* http_response_data = new char[HTTP_RESPONSE_JSON_MAX];
    int http_code = 200;
    string http_code_msg;

    if (ret == 0) {
        LOG_INFO << "login success, cookie: " << response_json;
        http_code = 204;
        http_code_msg = "No Content";
        snprintf(http_response_data, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_WITH_COOKIE,
            http_code, http_code_msg.c_str(), response_json.c_str(), (size_t)0, "");
    }
    else {
        LOG_INFO << "login failed, repsonse json: " << response_json;
        uint32_t len = response_json.length();
        http_code = 400;
        http_code_msg = "Bad Request";
        snprintf(http_response_data, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_WITH_CODE,
            http_code, http_code_msg.c_str(), (size_t)response_json.length(), response_json.c_str());
    }

    tcp_conn_->send(http_response_data);
    LOG_INFO << "http_response_data: " << http_response_data;
    delete[] http_response_data;

    return 0;
}

// html data
int CHttpConn::_HandleHtml(string& url, string& post_data) {
    std::ifstream fileStream("index.html");
    if (!fileStream.is_open()) {
        std::cerr << "无法打开文件" << std::endl;
    }
    std::stringstream buffer;
    buffer << fileStream.rdbuf();

    char* szContent = new char[HTTP_RESPONSE_JSON_MAX];
    uint32_t ulen = buffer.str().size();
    snprintf(szContent, HTTP_RESPONSE_JSON_MAX, HTTP_RESPONSE_HTML, (size_t)ulen,
        buffer.str().c_str());

    tcp_conn_->send(szContent);
    delete[] szContent;
    return 0;
}

char htmlStr[] = "<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"<title>Welcome to nginx!</title>\n"
"<style>\n"
"    body {\n"
"        width: 35em;\n"
"        margin: 0 auto;\n"
"        font-family: Tahoma, Verdana, Arial, sans-serif;\n"
"    }\n"
"</style>\n"
"</head>\n"
"<body>\n"
"<h1>Welcome to nginx!</h1>\n"
"<p>If you see this page,零声教育 the nginx web server is successfully installed and\n"
"working. Further configuration is required.</p>\n"
"<p>For online documentation and support please refer to\n"
"<a href=\"http://nginx.org/\">nginx.org</a>.<br/>\n"
"Commercial support is available at\n"
"<a href=\"http://nginx.com/\">nginx.com</a>.</p>\n"
"<p><em>Thank you for using nginx.</em></p>\n"
"</body>\n"
"</html>";

int CHttpConn::_HandleMemHtml(string& url, string& post_data) {

    char* szContent = new char[HTTP_RESPONSE_HTML_MAX];

    snprintf(szContent, HTTP_RESPONSE_HTML_MAX, HTTP_RESPONSE_HTML, (size_t)strlen(htmlStr),
        htmlStr);

    tcp_conn_->send(szContent);
    delete[] szContent;
    return 0;
}