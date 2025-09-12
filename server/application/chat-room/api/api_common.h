#ifndef __CHAT_ROOM_API_API_COMMON_H__
#define __CHAT_ROOM_API_API_COMMON_H__
#include<json/json.h>
#include<memory>
#include<vector>
#include "db_pool.h"
#include "cache_pool.h"
#include "muduo/base/Logging.h"
#include "muduo/base/md5.h"

using std::string;

// 加 class: 强类型，限制作用域
enum class api_error_id {
    bad_request = 0,
    login_failed,
    email_exists,
    username_exists,
};

template<typename...Args>
string FormatString(const string& format, Args... args) {
    auto size = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;

    // Extra space for '\0'
    std::unique_ptr<char[]> buf(new char[size]);
    // we don't want the '\0' inside
    std::snprintf(buf.get(), size, format.c_str(), args...);

    return string(buf.get(), buf.get() + size - 1);
}

/**
 * @param len: the length of str
 * @return: the random string of generating
 */
string RandomString(const int len);

int ApiSetCookie(string email, string& cookie);

string api_error_id_to_string(api_error_id input);

int ApiGetUsernameAndUseridByCookie(string cookie, string& username, int64_t& user_id, string& email);
int GetUserNameAndUseridByEmail(string& email, string& username, int64_t& user_id);

#endif
