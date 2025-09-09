#include "pub_sub_service.h"

static std::vector<Room> s_room_list = {
    {"0001", "utopianyouth1", ""},
    {"0002", "utopianyouth2", ""},
    {"0003", "utopianyouth3", ""},
    {"0004", "utopianyouth4", ""}
};


RoomTopic::RoomTopic(string room_id, string room_topic, int32_t owner_user_id) :
    room_id(room_id),
    room_topic(room_topic),
    owner_user_id(owner_user_id) {
}

// add user to room topic
void RoomTopic::AddSubscriber(int32_t user_id) {
    this->user_ids.insert(user_id);
}

// delete user to room topic 
void RoomTopic::DeleteSubscriber(int32_t user_id) {
    this->user_ids.erase(user_id);  //删除订阅者
}

// get all users in room topic
std::unordered_set<int32_t>& RoomTopic::getSubscribers() {
    return this->user_ids;
}

std::vector<Room>& GetRoomList() {
    return s_room_list;
}
