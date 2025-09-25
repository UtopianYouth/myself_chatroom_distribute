#ifndef __CHAT_ROOM_API_MESSAGES_H__
#define __CHAT_ROOM_API_MESSAGES_H__
#include "api_common.h"
#include "api_types.h"

// const when compiling
const constexpr size_t k_message_batch_size = 5;     // the max count of history messages

int ApiGetRoomHistory(Room& room, MessageBatch& message_batch, const int msg_count = k_message_batch_size);

int ApiStoreMessage(string room_id, std::vector<Message>& msgs);

// Message to json
string SerializeMessageToJson(const Message& msg);

#endif