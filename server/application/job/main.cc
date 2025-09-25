#include <memory>
#include <muduo/net/EventLoop.h>
#include <muduo/base/ThreadPool.h>
#include <muduo/base/Logging.h>
#include <thread>
#include <chrono>

#include <librdkafka/rdkafkacpp.h>
#include <grpcpp/grpcpp.h>
#include "kafka_consumer.h"
#include "ChatRoom.Job.pb.h"
#include "ChatRoom.Comet.pb.h"  
#include "ChatRoom.Comet.grpc.pb.h"  // 使用现有的proto
#include <json/json.h>

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
            proto->set_op(4);         // 4代表发送消息操作
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
            // chatroom address
            m_client = std::make_unique<CometClient>("127.0.0.1:50051");  
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

    KafkaConsumer consumer("localhost:9092", "my-topic");
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
                    // 获取chatroom的客户端并发送消息
                    auto client = CometManager::getInstance().getClient();
                    if (client) {
                        if (!client->broadcastRoom(pushMsg.room(), pushMsg.msg())) {
                            LOG_ERROR << "Failed to broadcast message to room: " << pushMsg.room();
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
