#ifndef __CHAT_ROOM_API_API_ROOM_H__
#define __CHAT_ROOM_API_API_ROOM_H__
#include<string>
#include<vector>
#include<ctime>
#include "db_pool.h"
#include "api_types.h"

bool ApiCreateRoom(const std::string& room_id, const string& room_name, const string& creator_id, string& error_msg);

bool ApiGetRoomInfo(const std::string& room_id, string& room_name, string& creator_id, string& create_time,
    string& update_time, string& error_msg);

bool ApiGetAllRooms(std::vector<Room>& rooms, string& error_msg, const string& order_by = "update_time DESC");

#endif