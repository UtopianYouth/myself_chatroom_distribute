#include "comet_service.h"
#include "muduo/base/Logging.h"
#include "websocket_conn.h"
#include "../service/pub_sub_service.h"
#include <json/json.h>

extern std::unordered_map<string, CHttpConnPtr> s_user_ws_conn_map;
extern std::mutex s_mtx_user_ws_conn_map;


namespace ChatRoom {
    grpc::Status CometServiceImpl::PushMsg(grpc::ServerContext* context,
        const Comet::PushMsgReq* request,
        Comet::PushMsgReply* response) {
        LOG_INFO << "PushMsg called";

        // 获取消息内容
        const Protocol::Proto& proto = request->proto();

        // 发送给指定的keys
        for (const auto& key : request->keys()) {
            // TODO: 实现具体的消息推送逻辑
        }

        return grpc::Status::OK;
    }

    grpc::Status CometServiceImpl::Broadcast(grpc::ServerContext* context,
        const Comet::BroadcastReq* request,
        Comet::BroadcastReply* response) {
        LOG_INFO << "Broadcast called";

        // 获取广播消息
        const Protocol::Proto& proto = request->proto();
        int speed = request->speed();
        int operation = proto.op();  // 获取操作码
        const std::string& message_json = proto.body();
        
        LOG_INFO << "Broadcasting message to all users:";
        LOG_INFO << "  Operation: " << operation;
        LOG_INFO << "  Message: " << message_json;
        
        // 检查是否是房间创建消息
        if (operation == 5) {
            LOG_INFO << "Processing room creation broadcast";
            // 解析 JSON 消息获取房间信息
            Json::Value root;
            Json::Reader reader;
            if (reader.parse(message_json, root)) {
                if (root["type"].asString() == "serverCreateRoom" && !root["payload"].isNull()) {
                    Json::Value payload = root["payload"];
                    string room_id = payload["roomId"].asString();
                    string room_name = payload["roomName"].asString();
                    string creator_id = payload["creatorId"].asString();
                    string creator_username = payload["creatorUsername"].asString();
                    
                    LOG_INFO << "Adding new room to subscription management:";
                    LOG_INFO << "  Room ID: " << room_id;
                    LOG_INFO << "  Room Name: " << room_name;
                    LOG_INFO << "  Creator ID: " << creator_id;
                    LOG_INFO << "  Creator Username: " << creator_username;
                    
                    // 添加房间主题到PubSubService
                    PubSubService& pubsub = PubSubService::GetInstance();
                    pubsub.AddRoomTopic(room_id, room_name, creator_id);
                    
                    // 为所有在线用户订阅该房间
                    {
                        std::lock_guard<std::mutex> lock(s_mtx_user_ws_conn_map);
                        for (const auto& user_pair : s_user_ws_conn_map) {
                            const string& userId = user_pair.first;
                            pubsub.AddSubscriber(room_id, userId);
                            LOG_DEBUG << "Added user " << userId << " to room " << room_id;
                        }
                        LOG_INFO << "Added " << s_user_ws_conn_map.size() << " users to room " << room_id;
                    }
                    
                    // 添加房间到全局房间列表
                    Room room;
                    room.room_id = room_id;
                    room.room_name = room_name;
                    room.creator_id = creator_id;
                    PubSubService::AddRoom(room);
                    
                    LOG_INFO << "Room " << room_id << " (created by " << creator_username << ") successfully added to subscription management";
                }
                else {
                    LOG_WARN << "Invalid room creation message format";
                }
            }
            else {
                // 以后待扩展的功能 ==> 删除房间，修改房间
            }
        }
        
        // 构造WebSocket帧
        std::string ws_frame = BuildWebSocketFrame(message_json, 0x01);
        
        // 广播给所有在线用户
        {
            std::lock_guard<std::mutex> lock(s_mtx_user_ws_conn_map);
            LOG_INFO << "Broadcast to " << s_user_ws_conn_map.size() << " online users";
            
            for (const auto& user_pair : s_user_ws_conn_map) {
                const string& userId = user_pair.first;
                CHttpConnPtr ws_conn_ptr = user_pair.second;
                
                if (ws_conn_ptr) {
                    ws_conn_ptr->send(ws_frame);
                    LOG_DEBUG << "Broadcast message sent to user: " << userId;
                }
                else {
                    LOG_WARN << "Invalid connection for user: " << userId;
                }
            }
        }
        
        LOG_INFO << "Broadcast completed successfully";

        return grpc::Status::OK;
    }

    grpc::Status CometServiceImpl::BroadcastRoom(grpc::ServerContext* context, 
        const Comet::BroadcastRoomReq* request,
        Comet::BroadcastRoomReply* response) {

        // 获取房间ID和消息内容
        const std::string& room_id = request->roomid();
        const Protocol::Proto& proto = request->proto();
        const std::string& message_json = proto.body();
        
        LOG_INFO << "BroadcastRoom called, roomID: " << request->roomid() << " proto: " << proto.body();
        
        // 使用PubSubService广播到指定房间
        PubSubService& pubsub = PubSubService::GetInstance();

        // proto.body()包含完整的serverMessages格式JSON
        std::string ws_frame = BuildWebSocketFrame(proto.body(), 0x01);
        
        auto callback = [ws_frame, room_id](std::unordered_set<string>& user_ids) {
            LOG_INFO << "room_id:" << room_id << ", callback " << ", user_ids.size(): " << user_ids.size();
            for (const string& userId : user_ids) {
                CHttpConnPtr ws_conn_ptr = nullptr;

                {
                    std::lock_guard<std::mutex> lock(s_mtx_user_ws_conn_map);
                    auto it = s_user_ws_conn_map.find(userId);
                    if (it != s_user_ws_conn_map.end()) {
                        ws_conn_ptr = it->second;
                    }
                }

                if (ws_conn_ptr) {
                    ws_conn_ptr->send(ws_frame);
                    LOG_DEBUG << "Message sent to user: " << userId;
                }
                else {
                    LOG_WARN << "can't find userid: " << userId;
                }
            }
        };

        // 广播给房间内所有用户
        PubSubService::GetInstance().PubSubMessage(room_id, callback);

        return grpc::Status::OK;
    }

    grpc::Status CometServiceImpl::Rooms(grpc::ServerContext* context,
        const Comet::RoomsReq* request,
        Comet::RoomsReply* response) {
        LOG_INFO << "Rooms called";

        // 获取所有房间列表
        auto& pubsub = PubSubService::GetInstance();
        // const auto& rooms = pubsub.GetAllRooms();

        // // 填充响应
        // for (const auto& room : rooms) {
        //     (*response->mutable_rooms())[room.first] = true;
        // }

        return grpc::Status::OK;
    }

} // namespace ChatRoom