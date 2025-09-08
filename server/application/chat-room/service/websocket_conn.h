
#ifndef __WEBSOCKET_CONN_H__
#define __WEBSOCKET_CONN_H__

#include "http_conn.h"
#include <sstream> // 包含 istringstream 的头文件
#include <openssl/sha.h>
#include "muduo/base/Logging.h" // Logger日志头文件

class CWebSocketConn: public CHttpConn {
public:
    CWebSocketConn(const TcpConnectionPtr& conn)
    : CHttpConn(conn) {

    }
    
    virtual void OnRead( Buffer* buf) {

    }
    virtual ~CWebSocketConn() {

    }
private:
    
};

 
#endif