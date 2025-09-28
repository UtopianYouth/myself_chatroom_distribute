#include "api_common.h"
#include<uuid/uuid.h>

/**
 * 1. to cookie ==> the key of redis
 *
 * @return: the unique uuid based on time and mac
 */
string GenerateUUID() {
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
 *  set cookie for login user and store in redis==> (key, values) = (cookie, email)
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

    cookie = GenerateUUID(); // unique ==> the key of redis

    // key:cookie value:email
    string ret = cache_conn->SetEx(cookie, 86400, email);

    if (!ret.empty()) {
        return 0;
    }
    else {
        return -1;
    }
}

string ApiErrorIdToString(api_error_id input) {
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

/**
 * query redis
 */
int ApiGetUsernameAndUseridByCookie(string cookie, string& username, string& userid, string& email) {
    int ret = 0;
    CacheManager* cache_manager = CacheManager::getInstance();
    CacheConn* cache_conn = cache_manager->GetCacheConn("token");
    AUTO_REL_CACHECONN(cache_manager, cache_conn);

    if (cache_conn) {
        email = cache_conn->Get(cookie);
        LOG_INFO << "cookie: " << cookie << ", email: " << email;
        if (email.empty()) {
            return -1;
        }
        else {
            return GetUserNameAndUseridByEmail(email, username, userid);
        }
    }
    else {
        return -1;
    }
}

/**
 * query mysql
 */
int GetUserNameAndUseridByEmail(string& email, string& username, string& userid) {
    int ret = 0;
    CDBManager* db_manager = CDBManager::getInstance();
    CDBConn* db_conn = db_manager->GetDBConn("chatroom_slave");
    AUTO_REL_DBCONN(db_manager, db_conn);

    // get user id and username
    string strSql = FormatString("select user_id, username from user_infos where email='%s'", email.c_str());
    CResultSet* result_set = db_conn->ExecuteQuery(strSql.c_str());

    if (result_set && result_set->Next()) {
        username = result_set->GetString("username");
        userid = result_set->GetString("user_id");

        LOG_DEBUG << "username: " << username;
        ret = 0;
    }
    else {
        ret = -1;
    }

    if (result_set) {
        delete result_set;
    }

    return ret;
}
