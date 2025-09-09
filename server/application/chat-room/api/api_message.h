#ifndef __CHAT_ROOM_API_MESSAGES_H__
# define __CHAT_ROOM_API_MESSAGES_H__
#include "api_common.h"
#include "api_types.h"

// const when compiling
const constexpr size_t message_batch_size = 50;     // 聊天室登录后最多拉取的消息数量

int ApiGetRoomHistory(Room& room, MessageBatch& message_batch);

int ApiStoreMessage(string room_id, std::vector<Message>& msgs);

// Message to json
string SerializeMessageToJson(const std::vector<Message>& msgs);

#endif