#include <memory>
#include <muduo/net/EventLoop.h>
#include <muduo/base/ThreadPool.h>
#include <muduo/base/Logging.h>
#include <thread>
#include <chrono>
#include <librdkafka/rdkafkacpp.h>
#include <grpcpp/grpcpp.h>
#include <json/json.h>
#include "service/kafka_consumer.h"
#include "base/config_file_reader.h"
#include "proto/ChatRoom.Job.pb.h"
#include "proto/ChatRoom.Comet.pb.h"  
#include "proto/ChatRoom.Comet.grpc.pb.h"  // 使用现有的proto

using namespace muduo;
using namespace muduo::net;
 
// gRPC客户端类
class CometClient {
public:
    CometClient(const std::string& server_address) : server_address_(server_address) {
        auto channel = grpc::CreateChannel(
            server_address, grpc::InsecureChannelCredentials());
        stub_ = ChatRoom::Comet::Comet::NewStub(channel);
    }

    bool broadcastRoom(const std::string& roomId, const std::string& msgContent) {
        const int MAX_RETRIES = 3;
        
        for (int retry = 0; retry < MAX_RETRIES; ++retry) {
            // 创建广播请求
            ChatRoom::Comet::BroadcastRoomReq request;
            request.set_roomid(roomId); //房间ID

            // 创建并设置Proto消息
            ChatRoom::Protocol::Proto *proto = request.mutable_proto();
            proto->set_ver(1);        // 设置版本号
            proto->set_op(4);         // 房间内发送消息
            proto->set_seq(0);        // 序列号，可以根据需要设置，用房间作为key, 使用redis 自增
            proto->set_body(msgContent);  // 设置消息内容

            // 发送gRPC请求
            ChatRoom::Comet::BroadcastRoomReply response;
            grpc::ClientContext context;
            
            // 设置超时时间
            auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
            context.set_deadline(deadline);

            grpc::Status status = stub_->BroadcastRoom(&context, request, &response);
            if (status.ok()) {
                LOG_INFO << "BroadcastRoom success";
                return true;
            }
            
            LOG_WARN << "RPC failed (attempt " << (retry + 1) << "/" << MAX_RETRIES 
                     << "): " << status.error_message();
            
            // 如果是连接问题，重新创建连接
            if (status.error_code() == grpc::StatusCode::UNAVAILABLE || 
                status.error_code() == grpc::StatusCode::CANCELLED) {
                LOG_INFO << "Recreating gRPC connection...";
                recreateConnection();
            }
            
            // 重试前等待一小段时间
            if (retry < MAX_RETRIES - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100 * (retry + 1)));
            }
        }
        
        LOG_ERROR << "RPC failed after " << MAX_RETRIES << " retries";
        return false;
    }

    bool broadcast(const std::string& msgContent) {
        const int MAX_RETRIES = 3;
        
        for (int retry = 0; retry < MAX_RETRIES; ++retry) {
            // 创建全局广播请求
            ChatRoom::Comet::BroadcastReq request;

            // 创建并设置Proto消息
            ChatRoom::Protocol::Proto *proto = request.mutable_proto();
            proto->set_ver(1);        // 设置版本号
            proto->set_op(5);         // 房间创建
            proto->set_seq(0);        // 序列号
            proto->set_body(msgContent);  // 设置消息内容

            request.set_speed(0);     // 0: 立即发送

            // 发送gRPC请求
            ChatRoom::Comet::BroadcastReply response;
            grpc::ClientContext context;
            
            // 设置超时时间
            auto deadline = std::chrono::system_clock::now() + std::chrono::seconds(5);
            context.set_deadline(deadline);

            grpc::Status status = stub_->Broadcast(&context, request, &response);
            if (status.ok()) {
                LOG_INFO << "Broadcast success";
                return true;
            }
            
            LOG_WARN << "Broadcast RPC failed (attempt " << (retry + 1) << "/" << MAX_RETRIES 
                     << "): " << status.error_message();
            
            // 如果是连接问题，重新创建连接
            if (status.error_code() == grpc::StatusCode::UNAVAILABLE || 
                status.error_code() == grpc::StatusCode::CANCELLED) {
                LOG_INFO << "Recreating gRPC connection...";
                recreateConnection();
            }
            
            // 重试前等待一小段时间
            if (retry < MAX_RETRIES - 1) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100 * (retry + 1)));
            }
        }
        
        LOG_ERROR << "Broadcast RPC failed after " << MAX_RETRIES << " retries";
        return false;
    }
    
    void recreateConnection() {
        auto channel = grpc::CreateChannel(
            server_address_, grpc::InsecureChannelCredentials());
        stub_ = ChatRoom::Comet::Comet::NewStub(channel);
    }

private:
    std::string server_address_;
    std::unique_ptr<ChatRoom::Comet::Comet::Stub> stub_;
};

// 管理与chatroom的连接
class CometManager {
public:
    static CometManager& getInstance() {
        static CometManager instance;
        return instance;
    }

    CometClient* getClient() {
        if (!m_client) {
            // 从配置文件读取chatroom地址
            CConfigFileReader config_reader("job.conf");
            const char* comet_server_c = config_reader.GetConfigName("comet_server");
            std::string comet_server = comet_server_c ? comet_server_c : "chatroom-app:50051";
            LOG_INFO << "Connecting to comet server: " << comet_server;
            m_client = std::make_unique<CometClient>(comet_server);  
        }
        
        return m_client.get();
    }

private:
    std::unique_ptr<CometClient> m_client;
};

int main() {
    EventLoop loop;
    ThreadPool threadPool("KafkaThreadPool");
    threadPool.start(4);

    // 从配置文件读取Kafka配置
    CConfigFileReader config_reader("job.conf");
    const char* brokers_c = config_reader.GetConfigName("kafka_brokers");
    const char* topic_c = config_reader.GetConfigName("kafka_topic");
    std::string kafka_brokers = brokers_c ? brokers_c : "localhost:9092";
    std::string kafka_topic = topic_c ? topic_c : "my-topic";

    LOG_INFO << "Initializing Kafka consumer with brokers: " << kafka_brokers << ", topic: " << kafka_topic;

    KafkaConsumer consumer(kafka_brokers, kafka_topic);
    if (!consumer.init() || !consumer.subscribe()) {
        LOG_ERROR << "Failed to initialize Kafka consumer";
        return -1;
    }

    // 处理消息
    threadPool.run([&]() {
        while (true) {
            std::string message = consumer.consume();
            if (!message.empty()) {
                ChatRoom::Job::PushMsg pushMsg;
                if (pushMsg.ParseFromString(message)) {
                    LOG_INFO << "Received PushMsg:";
                    LOG_INFO << "  Type: " << ChatRoom::Job::PushMsg_Type_Name(pushMsg.type());
                    LOG_INFO << "  Operation: " << pushMsg.operation();
                    LOG_INFO << "  roomId: " << pushMsg.room();
                    LOG_INFO << "  msg: " << pushMsg.msg();
                    // 获取chatroom的客户端并根据消息类型发送消息
                    auto client = CometManager::getInstance().getClient();
                    if (client) {
                        bool success = false;
                        
                        // 根据消息类型选择不同的广播方式
                        if (pushMsg.type() == ChatRoom::Job::PushMsg_Type_BROADCAST) {
                            // 全局广播
                            LOG_INFO << "Processing BROADCAST message";
                            success = client->broadcast(pushMsg.msg());
                            if (!success) {
                                LOG_ERROR << "Failed to broadcast message globally";
                            }
                        } else if (pushMsg.type() == ChatRoom::Job::PushMsg_Type_PUSH || 
                                   pushMsg.type() == ChatRoom::Job::PushMsg_Type_ROOM) {
                            // 房间内广播
                            LOG_INFO << "Processing ROOM/PUSH message to room: " << pushMsg.room();
                            success = client->broadcastRoom(pushMsg.room(), pushMsg.msg());
                            if (!success) {
                                LOG_ERROR << "Failed to broadcast message to room: " << pushMsg.room();
                            }
                        } else {
                            LOG_ERROR << "Unknown message type: " << pushMsg.type();
                        }
                    } else {
                        LOG_ERROR << "Failed to get comet client";
                    }
                } else {
                    LOG_ERROR << "Failed to parse message as PushMsg";
                }
            }
        }
    });

    loop.loop(); // 启动事件循环
    return 0;
}
