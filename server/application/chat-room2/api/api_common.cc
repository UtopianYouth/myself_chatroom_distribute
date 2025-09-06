#include "api_common.h"
#include<uuid/uuid.h>

/**
 * 1. to cookie ==> the key of redis
 *
 * @return: the unique uuid based on time and mac
 */
string generateUUID() {
    uuid_t uuid;
    // thread safe: generate binary format uuid
    uuid_generate_time_safe(uuid);

    char uuidStr[40] = { 0 };

    //  binary format converts ==> 550e8400-e29b-41d4-a716-446655440000
    uuid_unparse(uuid, uuidStr);

    return string(uuidStr);
}

/**
 * 1. to salt value
 *
 * @param len: the length of str
 * @return: the random string of generating
 */
string RandomString(const int len) {
    string str;
    char c;
    int idx;

    for (idx = 0; idx < len; ++idx) {
        c = 'a' + rand() % 26;
        str.push_back(c);
    }

    return str;
}

/**
 *
 */
int ApiSetCookie(string email, string& cookie) {
    // get redis connection pool
    CacheManager* cache_manager = CacheManager::getInstance();
    CacheConn* cache_conn = cache_manager->GetCacheConn("token");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    if (!cache_conn) {
        LOG_ERROR << "get cache conn failed";
        return -1;
    }

    cookie = generateUUID(); // unique ==> the key of redis

    // key:cookie value:email
    string ret = cache_conn->SetEx(cookie, 86400, email);

    if (!ret.empty()) {
        return 0;
    }
    else {
        return -1;
    }
}

string api_error_id_to_string(api_error_id input) {
    switch (input) {
    case api_error_id::login_failed:
        return "LOGIN_FAILED";
    case api_error_id::username_exists:
        return "USERNAME_EXISTS";
    case api_error_id::email_exists:
        return "EMAIL_EXISTS";
    case api_error_id::bad_request:
        return "";
    }
}
