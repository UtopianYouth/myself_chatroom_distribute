-- 授权用户管理脚本
USE myself_chatroom;

-- ========================================
-- 添加授权用户示例
-- ========================================
-- INSERT INTO authorized_users (email, is_enabled) VALUES ('user1@example.com', 1);
-- INSERT INTO authorized_users (email, is_enabled) VALUES ('user2@example.com', 1);
-- INSERT INTO authorized_users (email, is_enabled) VALUES ('admin@example.com', 1);

-- ========================================
-- 查询所有授权用户
-- ========================================
-- SELECT * FROM authorized_users ORDER BY create_time DESC;

-- ========================================
-- 禁用某个用户（不删除，只是禁用）
-- ========================================
-- UPDATE authorized_users SET is_enabled = 0 WHERE email = 'user1@example.com';

-- ========================================
-- 重新启用某个用户
-- ========================================
-- UPDATE authorized_users SET is_enabled = 1 WHERE email = 'user1@example.com';

-- ========================================
-- 删除授权用户
-- ========================================
-- DELETE FROM authorized_users WHERE email = 'user1@example.com';

-- ========================================
-- 查看所有已注册但未授权的用户
-- ========================================
-- SELECT u.email, u.username, u.create_time
-- FROM user_infos u
-- LEFT JOIN authorized_users a ON u.email = a.email
-- WHERE a.email IS NULL
-- ORDER BY u.create_time DESC;

-- ========================================
-- 批量添加授权用户（从已有用户表）
-- ========================================
-- INSERT INTO authorized_users (email, is_enabled)
-- SELECT email, 1 FROM user_infos
-- WHERE email IN ('user1@example.com', 'user2@example.com', 'user3@example.com');
