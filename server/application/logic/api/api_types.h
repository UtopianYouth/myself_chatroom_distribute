#ifndef __LOGIC_API_API_TYPES_H__
#define __LOGIC_API_API_TYPES_H__

#include <unordered_map>
#include <chrono>
#include <string>
#include <vector>

using std::string;
using timestamp_t = std::chrono::system_clock::time_point;

// chat-room info
typedef struct _room {
    string room_id;
    string room_name;
    string creator_id;
    string create_time;
    string update_time;
    string history_last_message_id;     // the start location of history message
}Room;

// Define Me struct
struct User {
    int id;
    string username;
};

// chat-message info
struct Message {
    string id;              // Message ID
    string username;        // message who send
    string content;         // The actual content of the message
    uint64_t timestamp;     // store seconds
    string user_id;         // UUID string
};

// A room history message batch
struct MessageBatch {
    std::vector<Message> messages;      // The message in the batch
    bool has_more{};                    // default has_more = false, True if there are more messages that could be loaded
};

#endif