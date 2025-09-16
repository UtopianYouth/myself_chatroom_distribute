

插入的消息
```
{"content":"0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ","timestamp":1739953118586568237,"user_id":54,"username":"test_user_54"}
```

# 使用命令测试stream

1. 先清除对应房间的数据
2. 使用命令插入消息



redis-benchmark -n 100000 -c 1 XADD mystream \* payload  0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ

性能：
Summary:
  throughput summary: 7412.35 requests per second
  latency summary (msec):
          avg       min       p50       p95       p99       max
        0.126     0.040     0.111     0.231     0.383     3.639



测试XADD命令，插入10万条消息，2个并发连接
redis-benchmark -n 100000 -c 2 XADD mystream \* payload  0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ

性能：
Summary:
  throughput summary: 19047.62 requests per second
  latency summary (msec):
          avg       min       p50       p95       p99       max
        0.090     0.008     0.087     0.199     0.359     4.463

测试XADD命令，插入10万条消息，4个并发连接
redis-benchmark -n 100000 -c 4 XADD mystream \* payload  0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ

性能：
Summary:
  throughput summary: 50505.05 requests per second
  latency summary (msec):
          avg       min       p50       p95       p99       max
        0.064     0.008     0.047     0.167     0.327     2.247



测试XADD命令，插入10万条消息，8个并发连接
redis-benchmark -n 100000 -c 8 XADD mystream \* payload  0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ


Summary:
  throughput summary: 76045.62 requests per second
  latency summary (msec):
          avg       min       p50       p95       p99       max
        0.085     0.016     0.063     0.215     0.527     2.143


测试XADD命令，插入10万条消息，16个并发连接
redis-benchmark -n 100000 -c 16 XADD mystream \* payload  0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ


Summary:
  throughput summary: 88105.73 requests per second
  latency summary (msec):
          avg       min       p50       p95       p99       max
        0.135     0.032     0.111     0.271     0.519     3.295

测试XADD命令，插入10万条消息，32个并发连接
redis-benchmark -n 100000 -c 16 XADD mystream \* payload  0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ
Summary:
  throughput summary: 85616.44 requests per second
  latency summary (msec):
          avg       min       p50       p95       p99       max
        0.137     0.032     0.111     0.271     0.607    12.503




3. 使用命令读取消息

# 测试XREAD读取最新消息
redis-benchmark -n 100000 -c 2 XREAD COUNT 100 STREAMS mystream 0

Summary:
  throughput summary: 7724.39 requests per second
  latency summary (msec):
          avg       min       p50       p95       p99       max
        0.167     0.040     0.135     0.359     0.671     2.183


redis-benchmark -n 100000 -c 2 XREAD COUNT 1 STREAMS mystream 0

Summary:
  throughput summary: 23952.09 requests per second
  latency summary (msec):
          avg       min       p50       p95       p99       max
        0.062     0.008     0.047     0.143     0.327     1.695



# 测试异步同步性能
sudo apt-get install libevent-dev

/hiredis/adapters/libevent.h:90:31: error: ‘hi_malloc’ was not declared in this scope; did you mean ‘sds_malloc’?
报错解决方法
$ git clone https://github.com/redis/hiredis.git
$ cd hiredis
$ git checkout v0.14.1 
$ make
# make install