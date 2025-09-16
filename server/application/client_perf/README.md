
# 从数据库删除房间的命令
```
DELETE FROM room_info WHERE room_name LIKE '%room%';
```

#注册账号

密码这里实际是1234，即是注册的时候 密码已经md5加密了
{
  "username": "test8",
  "email": "test8@test.com",
  "password": "81dc9bdb52d04dc20036dbd8313ed055"
}

# 登录
{
  "email": "test8@test.com",
  "password": "81dc9bdb52d04dc20036dbd8313ed055"
}