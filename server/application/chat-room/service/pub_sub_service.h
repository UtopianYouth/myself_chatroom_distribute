#ifndef __PUB_SUB_SERVICE_H__
#define __PUB_SUB_SERVICE_H__
#include<unordered_map>
#include<unordered_set>
#include<mutex>
#include<vector>
#include<functional>
#include<memory>
#include"api_types.h"

// Room manager
class RoomTopic {
private:
    string room_id;
    string room_topic;
    int64_t creator_id;
    std::unordered_set<int64_t> user_ids;
public:
    RoomTopic(string room_id, string room_topic, int64_t creator_id);
    ~RoomTopic();
    void AddSubscriber(int64_t user_id);
    void DeleteSubscriber(int64_t user_id);
    std::unordered_set<int64_t>& GetSubscribers();
};

using RoomTopicPtr = std::shared_ptr<RoomTopic>;

// callback: based on function
using PubSubCallback = std::function<void(std::unordered_set<int64_t>& user_ids)>;

class PublishSubscribeService {
private:
    std::mutex room_topic_map_mutex;
    std::unordered_map<string, RoomTopicPtr> room_topic_map;

public:
    // single mode 
    static PublishSubscribeService& GetInstance() {
        // local static func is thread secure for cpp11
        static PublishSubscribeService instance;
        return instance;
    }

    PublishSubscribeService() {}
    ~PublishSubscribeService() {}

    // create room topic
    bool AddRoomTopic(const string& room_id, const string& room_topic, int64_t creator_id) {
        std::lock_guard<std::mutex> lock(this->room_topic_map_mutex);
        LOG_DEBUG << "AddRoomTopic(), room_id: " << room_id << ", room_topic: " << room_topic << ", creator_id: " << creator_id;
        // the room topic is exists?
        if (this->room_topic_map.find(room_id) != this->room_topic_map.end()) {
            LOG_DEBUG << "AddRoomTopic(), room_id: " << room_id << " already exists";
            return false;
        }
        auto room_topic_ptr = std::make_shared<RoomTopic>(room_id, room_topic, creator_id);
        room_topic_map[room_id] = room_topic_ptr;
        return true;
    }

    // delete room topic ==> the creator or administor can do
    void DeleteRoomTopic(const string& room_id) {
        std::lock_guard<std::mutex> lock(this->room_topic_map_mutex);

        if (this->room_topic_map.find(room_id) != this->room_topic_map.end()) {
            return;
        }

        this->room_topic_map.erase(room_id);
    }

    // add user to room topic
    bool AddSubscriber(const string& room_id, int64_t user_id) {
        LOG_DEBUG << "AddSubscriber(), room_id: " << room_id << ", user_id: " << user_id;
        std::lock_guard<std::mutex> lock(this->room_topic_map_mutex);
        if (this->room_topic_map.find(room_id) == this->room_topic_map.end()) {
            LOG_WARN << "AddSubscriber(), can't find room_id: " << room_id;
            return false;
        }

        this->room_topic_map[room_id]->AddSubscriber(user_id);
    }

    // delete user from room topic 
    void DeleteSubscriber(const string& room_id, int64_t user_id) {
        std::lock_guard<std::mutex> lock(this->room_topic_map_mutex);
        if (this->room_topic_map.find(room_id) == this->room_topic_map.end()) {
            return;
        }
        this->room_topic_map[room_id]->DeleteSubscriber(user_id);
    }

    // send message to all user in room topic
    void PubSubMessage(const string& room_id, PubSubCallback callback) {
        std::unordered_set<int64_t> user_ids;
        {
            std::lock_guard<std::mutex> lock(this->room_topic_map_mutex);
            if (this->room_topic_map.find(room_id) == this->room_topic_map.end()) {
                return;
            }
            user_ids = this->room_topic_map[room_id]->GetSubscribers();
        }
        callback(user_ids);
    }
    // 房间列表管理 - 简化版本，只用于订阅管理
    static std::vector<Room>& GetRoomList();
    static int AddRoom(const Room& room);
    
    // 注意：房间的业务逻辑（创建、查询、数据库操作）已迁移到Logic层
    // 这里只保留订阅相关的功能，用于消息推送
};

#endif