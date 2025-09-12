#ifndef __WEBSOCKET_CONN_H__
#define __WEBSOCKET_CONN_H__

#include <sstream>           
#include <openssl/sha.h>
#include "http_conn.h"
#include "muduo/base/Logging.h" 
#include"api_types.h"

class CWebSocketConn : public CHttpConn {
public:
    CWebSocketConn(const TcpConnectionPtr& conn);
    virtual ~CWebSocketConn();
    virtual void OnRead(Buffer* buf);
    void Disconnect();

private:
    int64_t user_id = -1;       // userid
    string username;           // username
    std::unordered_map<string, Room> rooms_map;    // had joined the chatrooms

    void SendCloseFrame(uint16_t code, const string& reason);
    void SendPongFrame();       // Pong frame
    bool IsCloseFrame(const std::string& frame);

    int SendHelloMessage();
    int HandleClientMessages(Json::Value& root);
    int HandleRequestRoomHistory(Json::Value& root);
    bool handshake_completed = false;   // the websocket conn of client with server 
};

using CWebSocketConnPtr = std::shared_ptr<CWebSocketConn>;

#endif