#include <cstdint>
#include <iostream>
#include <signal.h>
#include <memory>
#include <atomic>
#include <unordered_map>
#include <shared_mutex>
#include <json/json.h>
#include "muduo/net/TcpServer.h"
#include "muduo/net/TcpConnection.h"
#include "muduo/base/ThreadPool.h"
#include "muduo/net/EventLoop.h"
#include "muduo/base/Logging.h"
#include "config_file_reader.h"
#include "http_handler.h"
#include "service/pub_sub_service.h"
#include "service/logic_config.h"
#include "service/logic_client.h"

#ifdef ENABLE_RPC
#include <thread>
#include <grpcpp/grpcpp.h>
#include "comet_service.h"
#endif

using namespace muduo;
using namespace muduo::net;

// 常量定义
namespace {
    constexpr int DEFAULT_THREAD_POOL_SIZE = 4;
    constexpr int SYSTEM_USER_ID = 1;
    constexpr const char* DEFAULT_CONFIG_FILE = "chat-room.conf";
    constexpr const char* DEFAULT_BIND_IP = "0.0.0.0";
    constexpr uint16_t DEFAULT_HTTP_PORT = 8080;
    constexpr const char* DEFAULT_GRPC_ADDRESS = "0.0.0.0:50051";
}

// WebSocket and http connection Manager 
class ConnectionManager {
public:
    using HttpHandlerPtr = std::shared_ptr<HttpHandler>;
    uint32_t AddConnection(const HttpHandlerPtr& handler) {
        uint32_t id = m_next_id.fetch_add(1);
        std::unique_lock<std::shared_mutex> lock(mutex);
        m_connections[id] = handler;
        return id;
    }

    void RemoveConnection(uint32_t id) {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_connections.erase(id);
    }

    HttpHandlerPtr GetConnection(uint32_t id) {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        auto it = m_connections.find(id);
        return (it != m_connections.end()) ? it->second : nullptr;
    }

    size_t GetConnectionCount() const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_connections.size();
    }

private:
    std::atomic<uint32_t> m_next_id{1};
    std::unordered_map<uint32_t, HttpHandlerPtr> m_connections;
    mutable std::shared_mutex m_mutex;    
};

// configuration
struct ServerConfig {
    std::string config_file_path = DEFAULT_CONFIG_FILE;
    std::string bind_ip = DEFAULT_BIND_IP;
    uint16_t http_port = DEFAULT_HTTP_PORT;
    int num_event_loops = 0;    // number of event loops
    int num_threads = DEFAULT_THREAD_POOL_SIZE; 
    int timeout_ms = 1000;
    Logger::LogLevel log_level = Logger::INFO;

    bool loadFromFile(const std::string& config_path) {
        try {
            CConfigFileReader config_file(config_path.c_str());
            // get log level
            if (char* str_log_level = config_file.GetConfigName("log_level")) {
                log_level = static_cast<Logger::LogLevel>(atoi(str_log_level));
            }

            // get internal config
            if (char* str_num_event_loops = config_file.GetConfigName("num_event_loops")) {
                num_event_loops = atoi(str_num_event_loops);
            }

            if (char* str_num_threads = config_file.GetConfigName("num_threads")) {
                num_threads = atoi(str_num_threads);
            }

            if (char* str_http_bind_port = config_file.GetConfigName("http_bind_port")) {
                http_port = static_cast<uint16_t>(atoi(str_http_bind_port));
            }

            if (char* str_timeout_ms = config_file.GetConfigName("timeout_ms")) {
                timeout_ms = atoi(str_timeout_ms);
            }
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR << "Failed to load config: " << e.what();
            return false;
        }
    }
};

class HttpServer {
public:
    HttpServer(EventLoop* loop, const InetAddress& addr, const std::string& name, 
               const ServerConfig& config)
        : m_loop(loop)
        , m_server(loop, addr, name)
        , m_config(config)
        , m_connection_manager(std::make_unique<ConnectionManager>())
    {
        m_server.setConnectionCallback(
            std::bind(&HttpServer::OnConnection, this, std::placeholders::_1));
        m_server.setMessageCallback(
            std::bind(&HttpServer::OnMessage, this, std::placeholders::_1, 
                     std::placeholders::_2, std::placeholders::_3));
        m_server.setWriteCompleteCallback(
            std::bind(&HttpServer::OnWriteComplete, this, std::placeholders::_1));

        m_server.setThreadNum(m_config.num_event_loops);
    }

    ~HttpServer() {
        this->Stop();
    }

    bool Start() {
        try {
            if (m_config.num_threads > 0) {
                CWebSocketConn::InitThreadPool(m_config.num_threads);
            }
            m_server.start();
            LOG_INFO << "HttpServer started successfully";
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR << "Failed to start HttpServer: " << e.what();
            return false;
        }
    }

    void Stop() {
        LOG_INFO << "Stopping HttpServer...";
        // 这里可以添加更多清理逻辑
    }

    size_t GetConnectionCount() const {
        return m_connection_manager->GetConnectionCount();
    }

private:
    void OnConnection(const TcpConnectionPtr& conn) {
        try {
            if (conn->connected()) {
                auto http_handler = std::make_shared<HttpHandler>(conn);
                uint32_t conn_id = m_connection_manager->AddConnection(http_handler);
                conn->setContext(conn_id);
                
                LOG_INFO << "New connection established, ID: " << conn_id 
                         << ", Total connections: " << this->GetConnectionCount();
            } else {
                auto conn_id = std::any_cast<uint32_t>(conn->getContext());
                m_connection_manager->RemoveConnection(conn_id);
                
                LOG_INFO << "Connection closed, ID: " << conn_id 
                         << ", Remaining connections: " << this->GetConnectionCount();
            }
        } catch (const std::exception& e) {
            LOG_ERROR << "Error in OnConnection: " << e.what();
        }
    }

    void OnMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp time) {
        try {
            uint32_t conn_id = std::any_cast<uint32_t>(conn->getContext());
            HttpHandlerPtr http_handler = m_connection_manager->GetConnection(conn_id);
            
            if (!http_handler) {
                LOG_WARN << "Handler not found for connection ID: " << conn_id;
                return;
            }

            LOG_DEBUG << "Processing message for connection ID: " << conn_id;

            if (m_config.num_threads > 0) {
                // threadpool
                m_thread_pool.run(std::bind(&HttpHandler::OnRead, http_handler, buf));
            } else {
                // IO thread
                http_handler->OnRead(buf);
            }
        } catch (const std::exception& e) {
            LOG_ERROR << "Error in onMessage: " << e.what();
        }
    }

    void OnWriteComplete(const TcpConnectionPtr& conn) {
        LOG_DEBUG << "Write completed for connection: " << conn.get();
    }

private:
    EventLoop* m_loop;
    TcpServer m_server;
    ServerConfig m_config;
    std::unique_ptr<ConnectionManager> m_connection_manager;
    ThreadPool m_thread_pool;
};

// 房间订阅管理
class RoomSubscriptionManager {
public:

    // 初始化房间订阅
    static int InitializeRoomSubscriptions() {
        PubSubService& pubSubService = PubSubService::GetInstance();
        
        LogicConfig& logic_config = LogicConfig::getInstance();
        string logic_server_url = logic_config.getLogicServerUrl();
        
        Json::Value request_json;
        request_json["type"] = "getAllRooms";
        
        Json::FastWriter writer;
        string post_data = writer.write(request_json);
        
        // 发送HTTP请求到Logic层
        LogicClient logic_client(logic_server_url);
        string response_json;
        string url = logic_server_url + "/logic/rooms";
        
        if (!logic_client.sendHttpRequest(url, post_data, response_json)) {
            LOG_ERROR << "Failed to get room list from Logic layer";
            return -1;
        }
        
        // 解析Logic层返回的房间列表
        Json::Reader reader;
        Json::Value logic_response;
        if (!reader.parse(response_json, logic_response)) {
            LOG_ERROR << "Failed to parse room list response from Logic layer";
            return -1;
        }
        

        if (logic_response.isMember("rooms") && logic_response.isMember("status") && 
            logic_response["status"].asString() == "success") {
            Json::Value rooms = logic_response["rooms"];
            for (int i = 0; i < rooms.size(); ++i) {
                Json::Value room = rooms[i];
                if (room.isMember("id") && room.isMember("name")) {
                    string room_id = room["id"].asString();
                    string room_name = room["name"].asString();
                    
                    pubSubService.AddRoomTopic(room_id, room_name, "1");
                    LOG_DEBUG << "Room subscription initialized: " << room_id << ", name: " << room_name;
                }
            }

            LOG_INFO << "Room subscription manager initialized, total rooms: " << rooms.size();

        } else {

            LOG_WARN << "No rooms found in Logic layer response or request failed";
            return -1;

        }
        
        return 0;
    }
};

// chatroom application
class ChatRoomApplication {
public:
    ChatRoomApplication() = default;
    ~ChatRoomApplication() { 
        CleanUp(); 
    }

    int Run(int argc, char* argv[]) {
        try {
            if (!ParseArguments(argc, argv)) {
                return -1;
            }

            SetupSignalHandlers();

            if (!m_config.loadFromFile(m_config.config_file_path)) {
                std::cerr << "Failed to load configuration" << std::endl;
                return -1;
            }
            Logger::setLogLevel(m_config.log_level);

            if (!InitializeLogicConfig()) {
                return -1;
            }

            if (RoomSubscriptionManager::InitializeRoomSubscriptions() < 0) {
                LOG_ERROR << "Failed to initialize room subscriptions";
                return -1;
            }
            return StartServers();
        } catch (const std::exception& e) {
            LOG_ERROR << "Application error: " << e.what();
            return -1;
        }
    }

private:
    // parse arguments of command
    bool ParseArguments(int argc, char* argv[]) {
        if (argc > 1) {
            m_config.config_file_path = argv[1];
        }
        std::cout << "Configuration file: " << m_config.config_file_path << std::endl;

        return true;
    }

    //  signal handlers
    void SetupSignalHandlers() {
        // 默认情况下，往一个读端关闭的管道或socket连接中写数据将引发SIGPIPE信号。我们需要在代码中捕获并处理该信号，
        // 或者至少忽略它，因为程序接收到SIGPIPE信号的默认行为是结束进程，而我们绝对不希望因为错误的写操作而导致程序退出。
        // SIG_IGN 忽略信号的处理程序
        signal(SIGPIPE, SIG_IGN);
    }



    // init logic config
    bool InitializeLogicConfig() {
        LogicConfig& logic_config = LogicConfig::getInstance();
        if (!logic_config.init(m_config.config_file_path)) {
            LOG_ERROR << "Failed to initialize logic config";
            return false;
        }
        LOG_INFO << "Logic config initialized successfully";
        return true;
    }

    // start servers
    int StartServers() {
#ifdef ENABLE_RPC
        // start gRPC server
        std::string grpc_server_address("0.0.0.0:50051");
        ChatRoom::CometServiceImpl comet_service;

        grpc::ServerBuilder builder;
        builder.AddListeningPort(grpc_server_address, grpc::InsecureServerCredentials());
        builder.RegisterService(&comet_service);
        std::unique_ptr<grpc::Server> grpc_server(builder.BuildAndStart());
        LOG_INFO << "gRPC Server listening on " << grpc_server_address;
#endif

        EventLoop loop;
        InetAddress addr(m_config.bind_ip, m_config.http_port);
        LOG_INFO << "Starting HTTP server on " << m_config.bind_ip 
                 << ":" << m_config.http_port;
        HttpServer server(&loop, addr, "ChatRoomServer", m_config);

        if (!server.Start()) {
            return -1;
        }

#ifdef ENABLE_RPC
        // handle HTTP and gRPC requests concurrently
        std::thread grpc_thread([&grpc_server]() {
            grpc_server->Wait();
            });
#endif

        LOG_INFO << "ChatRoom server is running...";

        loop.loop(m_config.timeout_ms);     // 1000ms
        
#ifdef ENABLE_RPC
    grpc_server->Shutdown();
    grpc_thread.join();
#endif
        return 0;
    }

    void CleanUp() {
        LOG_INFO << "Cleaning up application resources...";
        // ...
    }

private:
    ServerConfig m_config;

};

int main(int argc, char* argv[]) {
    std::cout << "Myself ChatRoom Server v1.0" << std::endl;
    std::cout << "Usage: " << argv[0] << " [config_file]" << std::endl;

    ChatRoomApplication app;

    return app.Run(argc, argv);
}