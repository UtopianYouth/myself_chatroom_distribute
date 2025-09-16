#include "db_pool.h"
#include "cache_pool.h"
#include <muduo/base/Logging.h>

// 初始化数据库和缓存连接池
bool initPools() {
    // 设置配置文件路径
    CDBManager::SetConfPath("dbserver.conf");
    CacheManager::SetConfPath("redis.conf");
    
    // 初始化数据库连接池
    CDBManager* dbManager = CDBManager::getInstance();
    if (!dbManager) {
        LOG_ERROR << "init db manager failed";
        return false;
    }
    
    // 初始化Redis连接池
    CacheManager* cacheManager = CacheManager::getInstance();
    if (!cacheManager) {
        LOG_ERROR << "init cache manager failed";
        return false;
    }
    
    return true;
} 