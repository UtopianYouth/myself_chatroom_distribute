#include "pub_sub_service.h"

// 房间列表: Comet层的订阅管理和消息推送
static std::vector<Room> s_room_list;

static std::mutex s_mutex_room_list;

// =====================RoomTopic=======================
RoomTopic::RoomTopic(string room_id, string room_topic, const string& creator_id) {
    this->room_id = room_id;
    this->room_topic = room_topic;
    this->creator_id = creator_id;
}

RoomTopic::~RoomTopic() {
    this->user_ids.clear();
}

// add user to room topic
void RoomTopic::AddSubscriber(const string& user_id) {
    this->user_ids.insert(user_id);
}

// delete user to room topic 
void RoomTopic::DeleteSubscriber(const string& user_id) {
    this->user_ids.erase(user_id);
}

// get all users in room topic
std::unordered_set<string>& RoomTopic::GetSubscribers() {
    return this->user_ids;
}

// =============================PubSubService===========================
std::vector<Room>& PubSubService::GetRoomList() {
    std::lock_guard<std::mutex> lock(s_mutex_room_list);
    return s_room_list;
}

int PubSubService::AddRoom(const Room& room) {
    std::lock_guard<std::mutex> lock(s_mutex_room_list);

    // check if room exists
    for (const auto& r : s_room_list) {
        if (r.room_id == room.room_id) {
            LOG_DEBUG << "room_id " << r.room_id << " already exists";
            return -1;
        }
    }
    s_room_list.push_back(room);
    return 0;
}


