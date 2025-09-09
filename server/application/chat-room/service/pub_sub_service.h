#ifndef __PUB_SUB_SERVICE_H__
#define __PUB_SUB_SERVICE_H__
#include<unordered_map>
#include<unordered_set>
#include<mutex>
#include<vector>
#include<functional>
#include"api_types.h"

// The theme and topic of room
class RoomTopic {
public:
    string room_id;
    string room_topic;
    int32_t owner_user_id;
    std::unordered_set<int32_t> user_ids;
    RoomTopic(string room_id, string room_topic, int32_t owner_user_id);
    void AddSubscriber(int32_t user_id);
    void DeleteSubscriber(int32_t user_id);
    std::unordered_set<int32_t>& getSubscribers();
};
using RoomTopicPtr = std::shared_ptr<RoomTopic>;

// callback: based on function
using PubSubCallback = std::function<void(std::unordered_set<int32_t>&)>;

// This is an interface to reduce compile times
class PubSubService {
public:
    std::mutex mtx;
    std::unordered_map<string, RoomTopicPtr> room_topic_ids;

    // static func: get single instance
    static PubSubService& GetInstance() {
        // local static func is thread secure for cpp11
        static PubSubService instance;
        return instance;
    }

    virtual ~PubSubService() {}

    // create room topic
    bool AddRoomTopic(string room_id, string room_topic, int32_t owner_user_id) {
        RoomTopicPtr room_topic_ptr = std::make_shared<RoomTopic>(room_id,
            room_topic, owner_user_id);
        std::lock_guard<std::mutex> lock(mtx);

        // the room topic is exists?

        room_topic_ids.insert({ room_id, room_topic_ptr }); // key: room_id
        return true;
    }

    // delete room topic ==> the owner or administor can do
    bool DeleteRoomTopic(string room_id, int32_t owner_user_id) {
        std::lock_guard<std::mutex> lock(mtx);
        room_topic_ids.erase(room_id);
        return true;
    }

    // add user to room topic
    bool AddSubscriber(string room_id, int32_t user_id) {
        std::lock_guard<std::mutex> lock(mtx);
        RoomTopicPtr& room_topic_ptr = room_topic_ids[room_id];
        if (room_topic_ptr) {
            room_topic_ptr->AddSubscriber(user_id);
            return true;
        }
        else {
            return false;
        }
    }

    // delete user from room topic 
    bool DeleteSubscriber(string room_id, int32_t user_id) {
        std::lock_guard<std::mutex> lock(mtx);
        RoomTopicPtr& room_topic_ptr = room_topic_ids[room_id];
        if (room_topic_ptr) {
            room_topic_ptr->DeleteSubscriber(user_id);
            return true;
        }
        else {
            return false;
        }
    }

    // send message to all user in room topic 
    bool PubSubPublish(string room_id, PubSubCallback callback) {
        mtx.lock();

        RoomTopicPtr& room_topic_ptr = room_topic_ids[room_id];
        if (room_topic_ptr) {
            std::unordered_set<int32_t> user_ids = room_topic_ptr->getSubscribers();
            mtx.unlock();
            callback(user_ids);
            return true;
        }
        else {
            mtx.unlock();
            return false;
        }
    }
};

std::vector<Room>& GetRoomList();

#endif