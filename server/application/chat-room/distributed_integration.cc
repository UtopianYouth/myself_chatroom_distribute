// #pragma once

// #include "main_improved.cc"
// #include "rpc/comet_service.h"
// #include "kafka_consumer.h"
// #include "kafka_producer.h"
// #include <thread>
// #include <atomic>

// // 分布式聊天室集成类
// class DistributedChatRoomApplication : public ChatRoomApplication {
// public:
//     DistributedChatRoomApplication() 
//         : kafka_enabled_(false)
//         , grpc_enabled_(false)
//         , shutdown_requested_(false) {}

//     ~DistributedChatRoomApplication() {
//         shutdown();
//     }

//     // 启用分布式功能
//     void enableDistributedMode(const DistributedConfig& dist_config) {
//         dist_config_ = dist_config;
//         kafka_enabled_ = true;
//         grpc_enabled_ = true;
//     }

//     int run(int argc, char* argv[]) override {
//         try {
//             // 基础初始化
//             if (ChatRoomApplication::run(argc, argv) != 0) {
//                 return -1;
//             }

//             // 启动分布式组件
//             if (kafka_enabled_) {
//                 if (!startKafkaComponents()) {
//                     LOG_ERROR << "Failed to start Kafka components";
//                     return -1;
//                 }
//             }

//             if (grpc_enabled_) {
//                 if (!startGrpcService()) {
//                     LOG_ERROR << "Failed to start gRPC service";
//                     return -1;
//                 }
//             }

//             LOG_INFO << "Distributed ChatRoom started successfully";
//             return 0;

//         } catch (const std::exception& e) {
//             LOG_ERROR << "Distributed application error: " << e.what();
//             return -1;
//         }
//     }

// private:
//     // 分布式配置
//     struct DistributedConfig {
//         std::string kafka_brokers = "localhost:9092";
//         std::string kafka_topic = "chatroom-messages";
//         std::string kafka_group_id = "chatroom-consumer-group";
//         std::string grpc_address = "0.0.0.0:50051";
//         int kafka_consumer_threads = 2;
//     };

//     bool startKafkaComponents() {
//         // 启动Kafka消费者线程
//         for (int i = 0; i < dist_config_.kafka_consumer_threads; ++i) {
//             kafka_threads_.emplace_back([this, i]() {
//                 runKafkaConsumer(i);
//             });
//         }

//         // 初始化Kafka生产者（用于转发消息到其他节点）
//         if (!kafka_producer_.init(dist_config_.kafka_brokers, dist_config_.kafka_topic)) {
//             LOG_ERROR << "Failed to initialize Kafka producer";
//             return false;
//         }

//         LOG_INFO << "Kafka components started successfully";
//         return true;
//     }

//     bool startGrpcService() {
//         grpc_thread_ = std::thread([this]() {
//             runGrpcServer();
//         });

//         LOG_INFO << "gRPC service started successfully";
//         return true;
//     }

//     void runKafkaConsumer(int consumer_id) {
//         std::string group_id = dist_config_.kafka_group_id + "-" + std::to_string(consumer_id);
//         KafkaConsumer consumer(dist_config_.kafka_brokers, dist_config_.kafka_topic, group_id);

//         if (!consumer.init() || !consumer.subscribe()) {
//             LOG_ERROR << "Failed to initialize Kafka consumer " << consumer_id;
//             return;
//         }

//         LOG_INFO << "Kafka consumer " << consumer_id << " started";

//         while (!shutdown_requested_.load()) {
//             try {
//                 std::string message = consumer.consume(1000); // 1秒超时
//                 if (!message.empty()) {
//                     processKafkaMessage(message);
//                 }
//             } catch (const std::exception& e) {
//                 LOG_ERROR << "Kafka consumer " << consumer_id << " error: " << e.what();
//                 std::this_thread::sleep_for(std::chrono::seconds(1));
//             }
//         }

//         LOG_INFO << "Kafka consumer " << consumer_id << " stopped";
//     }

//     void processKafkaMessage(const std::string& message) {
//         try {
//             ChatRoom::Job::PushMsg pushMsg;
//             if (!pushMsg.ParseFromString(message)) {
//                 LOG_ERROR << "Failed to parse Kafka message";
//                 return;
//             }

//             LOG_INFO << "Processing message for room: " << pushMsg.room();

//             // 根据消息类型处理
//             switch (pushMsg.type()) {
//                 case ChatRoom::Job::PushMsg_Type_ROOM:
//                     handleRoomMessage(pushMsg);
//                     break;
//                 case ChatRoom::Job::PushMsg_Type_BROADCAST:
//                     handleBroadcastMessage(pushMsg);
//                     break;
//                 case ChatRoom::Job::PushMsg_Type_PUSH:
//                     handlePushMessage(pushMsg);
//                     break;
//                 default:
//                     LOG_WARN << "Unknown message type: " << pushMsg.type();
//                     break;
//             }
//         } catch (const std::exception& e) {
//             LOG_ERROR << "Error processing Kafka message: " << e.what();
//         }
//     }

//     void handleRoomMessage(const ChatRoom::Job::PushMsg& pushMsg) {
//         // 广播到指定房间的所有连接
//         const std::string& room_id = pushMsg.room();
//         const std::string& msg_content = pushMsg.msg();

//         LOG_INFO << "Broadcasting to room: " << room_id;

//         // 构建WebSocket帧
//         std::string ws_frame = BuildWebSocketFrame(msg_content, 0x01);

//         // 使用PubSubService广播到房间
//         auto callback = [ws_frame, room_id](const std::unordered_set<int64_t>& user_ids) {
//             LOG_INFO << "Broadcasting to " << user_ids.size() << " users in room " << room_id;
            
//             for (uint32_t userId : user_ids) {
//                 // 这里需要访问连接管理器
//                 // 由于架构限制，我们需要通过全局变量或单例访问
//                 // 在实际集成时，需要重构连接管理
//                 broadcastToUser(userId, ws_frame);
//             }
//         };

//         PubSubService::GetInstance().PubSubMessage(room_id, callback);
//     }

//     void handleBroadcastMessage(const ChatRoom::Job::PushMsg& pushMsg) {
//         // 广播给所有连接的用户
//         LOG_INFO << "Broadcasting to all users";
        
//         const std::string& msg_content = pushMsg.msg();
//         std::string ws_frame = BuildWebSocketFrame(msg_content, 0x01);

//         // 实现全局广播逻辑
//         broadcastToAllUsers(ws_frame);
//     }

//     void handlePushMessage(const ChatRoom::Job::PushMsg& pushMsg) {
//         // 推送给指定用户
//         LOG_INFO << "Pushing to specific users";
        
//         const std::string& msg_content = pushMsg.msg();
//         std::string ws_frame = BuildWebSocketFrame(msg_content, 0x01);

//         for (const auto& key : pushMsg.keys()) {
//             // 根据key查找用户并推送
//             // 这里需要实现key到用户ID的映射
//             pushToUserByKey(key, ws_frame);
//         }
//     }

//     void runGrpcServer() {
//         try {
//             ChatRoom::CometServiceImpl comet_service;
//             grpc::ServerBuilder builder;
            
//             builder.AddListeningPort(dist_config_.grpc_address, 
//                                    grpc::InsecureServerCredentials());
//             builder.RegisterService(&comet_service);
            
//             grpc_server_ = builder.BuildAndStart();
//             LOG_INFO << "gRPC Server listening on " << dist_config_.grpc_address;
            
//             if (grpc_server_) {
//                 grpc_server_->Wait();
//             }
//         } catch (const std::exception& e) {
//             LOG_ERROR << "gRPC server error: " << e.what();
//         }
//     }

//     void shutdown() {
//         LOG_INFO << "Shutting down distributed components...";
        
//         shutdown_requested_.store(true);

//         // 关闭gRPC服务器
//         if (grpc_server_) {
//             grpc_server_->Shutdown();
//         }

//         // 等待gRPC线程结束
//         if (grpc_thread_.joinable()) {
//             grpc_thread_.join();
//         }

//         // 等待Kafka线程结束
//         for (auto& thread : kafka_threads_) {
//             if (thread.joinable()) {
//                 thread.join();
//             }
//         }

//         // 关闭Kafka生产者
//         kafka_producer_.close();

//         LOG_INFO << "Distributed components shut down successfully";
//     }

//     // 辅助方法（需要与连接管理器集成）
//     void broadcastToUser(uint32_t userId, const std::string& message) {
//         // 实现向特定用户广播的逻辑
//         // 这里需要访问连接管理器
//     }

//     void broadcastToAllUsers(const std::string& message) {
//         // 实现向所有用户广播的逻辑
//     }

//     void pushToUserByKey(const std::string& key, const std::string& message) {
//         // 实现根据key推送的逻辑
//     }

// private:
//     DistributedConfig dist_config_;
//     bool kafka_enabled_;
//     bool grpc_enabled_;
//     std::atomic<bool> shutdown_requested_;

//     // Kafka组件
//     std::vector<std::thread> kafka_threads_;
//     KafkaProducer kafka_producer_;

//     // gRPC组件
//     std::thread grpc_thread_;
//     std::unique_ptr<grpc::Server> grpc_server_;
// };

// // 消息转发服务（类似Logic服务的功能）
// class MessageForwardingService {
// public:
//     MessageForwardingService(const std::string& kafka_brokers, const std::string& topic)
//         : kafka_brokers_(kafka_brokers), kafka_topic_(topic) {}

//     bool init() {
//         return kafka_producer_.init(kafka_brokers_, kafka_topic_);
//     }

//     // 转发消息到Kafka（用于跨节点通信）
//     bool forwardMessage(const std::string& room_id, const std::string& message, 
//                        ChatRoom::Job::PushMsg_Type type = ChatRoom::Job::PushMsg_Type_ROOM) {
//         try {
//             ChatRoom::Job::PushMsg pushMsg;
//             pushMsg.set_type(type);
//             pushMsg.set_operation(4); // 发送消息操作
//             pushMsg.set_room(room_id);
//             pushMsg.set_msg(message);

//             std::string serialized_msg;
//             if (!pushMsg.SerializeToString(&serialized_msg)) {
//                 LOG_ERROR << "Failed to serialize message";
//                 return false;
//             }

//             return kafka_producer_.sendMessage(serialized_msg);
//         } catch (const std::exception& e) {
//             LOG_ERROR << "Error forwarding message: " << e.what();
//             return false;
//         }
//     }

//     void close() {
//         kafka_producer_.close();
//     }

// private:
//     std::string kafka_brokers_;
//     std::string kafka_topic_;
//     KafkaProducer kafka_producer_;
// };

// // 使用示例
// int main(int argc, char* argv[]) {
//     std::cout << "Distributed ChatRoom Server v2.0" << std::endl;

//     DistributedChatRoomApplication app;
    
//     // 配置分布式模式
//     DistributedChatRoomApplication::DistributedConfig dist_config;
//     dist_config.kafka_brokers = "localhost:9092";
//     dist_config.kafka_topic = "chatroom-messages";
//     dist_config.grpc_address = "0.0.0.0:50051";
    
//     app.enableDistributedMode(dist_config);
    
//     return app.run(argc, argv);
// }