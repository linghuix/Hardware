/*
 * ESP32 TCP 发送速率测试  (和 udp_speed_test 同样的结构，方便对比)
 *
 * TCP 有重传，不会丢数据；限制速度的是 Arduino 核心里只有 5 744 字节的
 * TCP 发送缓冲区:  速度 ≈ 5 744 B ÷ 往返延迟 (RTT)。
 * 所以本测试除了吞吐量，还测 RTT 和 "卡顿" (一次 send 超过 300 ms，
 * 通常是丢包后 TCP 等重传计时器)。
 *
 * 用法:
 *   1. 上传本程序，串口监视器 921600，记下打印的 ESP32 IP
 *   2. 电脑上运行同目录下的脚本 (需要 Python 3):
 *        python tcp_receiver.py 192.168.137.110 3
 *      脚本连上 ESP32 的 TCP 端口 8080 后:
 *        a. 先 "ping" 20 次，测空闲时的应用层往返延迟 (RTT)
 *        b. 每轮发 "start"，ESP32 连续发送 TEST_DURATION_MS 毫秒，
 *           最后发一个结束块，附带自己的统计
 *   3. 两边对比:
 *        ESP32 端: 发送字节数 / 卡顿次数 / 最慢一次 send
 *        电脑端:   实际收到的 Mbps / RTT / 数据是否连续
 *
 * 数据格式: 固定 BLOCK_SIZE 字节的块，前 4 字节是序号 (小端)。
 *   结束块: 序号 = 0xFFFFFFFF，后面是文本 "END <块数> <毫秒> <卡顿> <最慢ms>"。
 *   BLOCK_SIZE 必须和 tcp_receiver.py 里的一致。
 */

#include <WiFi.h>              // Arduino 的 Wi-Fi 库: 连接热点、获取 IP、RSSI
#include "lwip/sockets.h"      // lwIP 的 BSD socket 接口: socket/bind/listen/accept/send/recv

// ---------------------------------
// Wi-Fi: 电脑的 Windows 移动热点 (频段要设成 2.4 GHz, ESP32 不支持 5 GHz)
const char* ssid = "YOUR_SSID";             // 热点名称 (SSID)
const char* password = "YOUR_PASSWORD";    // 热点密码 (不要提交到公开仓库)
// ---------------------------------

const int TCP_PORT = 8080;             // ESP32 监听的 TCP 端口，电脑脚本连这个端口
const int BLOCK_SIZE = 4096;           // 每次 send() 的字节数 (= 一个数据块)，必须和 .py 一致
const uint32_t TEST_DURATION_MS = 5000;  // 每轮测试发送多久 (毫秒)
const uint32_t STALL_MS = 300;         // 一次 send 超过这个时间 (毫秒) 就算一次 "卡顿"

int listenSock = -1;                   // 监听 socket 的文件描述符，-1 表示还没创建
uint8_t block[BLOCK_SIZE];             // 发送用的数据块缓冲区 (4 KB，放在全局避免占用任务栈)

void setup() {
  Serial.begin(921600);                // 串口波特率 921600，串口监视器也要选 921600
  Serial.println("\nTCP 发送速率测试");  // 开机提示 (前面加换行，和启动乱码分开)

  WiFi.mode(WIFI_STA);                 // 设为 Station 模式: ESP32 作为客户端连接热点
  WiFi.begin(ssid, password);          // 开始连接热点 (异步，下面循环等待结果)
  WiFi.setSleep(false);                // 关闭 Wi-Fi 省电，否则 ACK 被延迟，速率严重偏低
  Serial.print("正在连接到 ");          // 打印正在连接的提示
  Serial.println(ssid);                // 打印热点名称
  while (WiFi.status() != WL_CONNECTED) {  // 一直等到连接成功
    delay(500);                        // 每 0.5 秒检查一次
    Serial.print(".");                 // 打印一个点表示还在等
  }
  Serial.println("\nWiFi 已连接!");      // 连接成功
  Serial.print("ESP32 IP 地址: ");       // 打印分配到的 IP，电脑脚本要用这个地址
  Serial.println(WiFi.localIP());      // 热点下一般是 192.168.137.x
  Serial.printf("信号强度 RSSI: %d dBm\n", WiFi.RSSI());  // 信号强度，越接近 0 越好

  // 直接用 lwIP 的 socket 接口 (和 udp_speed_test 一致)，能拿到 errno
  listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);  // 创建 IPv4 TCP socket
  int yes = 1;                         // setsockopt 用的 "开启" 值
  setsockopt(listenSock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));  // 允许端口立即复用
  sockaddr_in addr = {};               // 本机监听地址结构体，先全部清零
  addr.sin_family = AF_INET;           // 地址族: IPv4
  addr.sin_port = htons(TCP_PORT);     // 端口号，htons 转成网络字节序 (大端)
  addr.sin_addr.s_addr = htonl(INADDR_ANY);  // 监听本机所有 IP 地址
  if (listenSock < 0 ||                // socket 创建失败
      bind(listenSock, (sockaddr *)&addr, sizeof(addr)) < 0 ||  // 或绑定端口失败
      listen(listenSock, 1) < 0) {     // 或开始监听失败 (最多排队 1 个连接)
    Serial.println("TCP socket 创建失败!");  // 报错
    while (true) delay(1000);          // 停在这里，不再继续
  }

  for (int i = 0; i < BLOCK_SIZE; i++) block[i] = (uint8_t)i;  // 用 0,1,2,...255,0,1... 填充数据块
  Serial.printf("在 TCP 端口 %d 等待电脑连接...\n", TCP_PORT);   // 提示可以运行电脑脚本了
}

// 发送完整的 len 字节 (TCP 的 send 可能只发一部分)。失败返回 false。
bool sendAll(int fd, const uint8_t *data, int len) {
  while (len > 0) {                    // 还有没发完的数据就继续
    int r = send(fd, data, len, 0);    // 尝试发送; 缓冲区满时会阻塞等待 (最多 SO_SNDTIMEO)
    if (r <= 0) return false;          // 返回 0 或负数: 连接断开 / 超时 / 出错
    data += r;                         // 指针后移，跳过已经发出的部分
    len -= r;                          // 剩余字节数减少
  }
  return true;                         // 全部发完
}

void runTest(int fd) {
  Serial.printf("开始发送 %u ms (每块 %d 字节)...\n", (unsigned)TEST_DURATION_MS, BLOCK_SIZE);  // 本轮开始

  uint32_t seq = 0, stalls = 0, maxSendMs = 0;  // 块序号 (也是已发块数)、卡顿次数、最慢一次 send 的毫秒数
  bool ok = true;                      // 发送是否一直成功
  uint32_t start = millis();           // 本轮开始时间

  while (millis() - start < TEST_DURATION_MS) {  // 在 TEST_DURATION_MS 内一直发 (减法比较，溢出安全)
    memcpy(block, &seq, 4);            // 序号放在块头 4 字节 (ESP32 是小端，电脑用 "<I" 解析)
    uint32_t t0 = millis();            // 记录这次 send 开始的时间
    // 发送缓冲区满时 send() 会阻塞，直到电脑的 ACK 腾出空间
    ok = sendAll(fd, block, BLOCK_SIZE);  // 发送一整块
    uint32_t dt = millis() - t0;       // 这次 send 用了多少毫秒
    if (!ok) break;                    // 发送失败 (连接断了)，提前结束本轮
    if (dt > maxSendMs) maxSendMs = dt;  // 更新最慢一次 send
    if (dt > STALL_MS) stalls++;       // 超过 300 ms 算一次卡顿 (一般是丢包后等重传)
    seq++;                             // 下一块的序号
  }
  uint32_t elapsed = millis() - start; // 本轮实际用时 (毫秒)

  float secs = elapsed / 1000.0f;      // 换算成秒
  float kbps = seq * (float)BLOCK_SIZE / 1024.0f / secs;  // 平均速率 KB/s (按 ESP32 发出的字节算)
  Serial.println("--- 批次完成 (ESP32 端) ---");  // 打印本轮结果
  if (!ok) Serial.printf("警告: send 失败 (errno %d)，连接可能已断开\n", errno);  // 失败时打印错误码
  Serial.printf("发送: %u 块 (%u 字节)\n", seq, seq * BLOCK_SIZE);  // 发出的块数和字节数
  Serial.printf("时长: %u ms\n", elapsed);  // 本轮用时
  Serial.printf("ESP32 发送速率: %.2f Mbps (%.0f KB/s)\n", kbps * 1024 * 8 / 1e6, kbps);  // KB/s 换算成 Mbps
  Serial.printf("卡顿 (send > %u ms): %u 次，最慢一次 send: %u ms\n",
                (unsigned)STALL_MS, stalls, maxSendMs);  // 卡顿统计
  Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());  // 测完时的信号强度
  Serial.println("--------------------");  // 分隔线
  if (!ok) return;                     // 连接已断，不再发送结束块

  // 结束块: 序号 0xFFFFFFFF + 统计文本，补齐到 BLOCK_SIZE
  memset(block, 0, BLOCK_SIZE);        // 先把整块清零 (文本后面的部分都是 0)
  uint32_t endSeq = 0xFFFFFFFF;        // 特殊序号，电脑端看到它就知道本轮结束
  memcpy(block, &endSeq, 4);           // 写入块头
  snprintf((char *)block + 4, BLOCK_SIZE - 4, "END %u %u %u %u",
           seq, elapsed, stalls, maxSendMs);  // 块头后面写统计文本: 块数 时长 卡顿 最慢ms
  sendAll(fd, block, BLOCK_SIZE);      // 发送结束块 (也是完整的 BLOCK_SIZE，电脑端按块读取)
  for (int i = 0; i < BLOCK_SIZE; i++) block[i] = (uint8_t)i;  // 恢复填充数据，供下一轮使用
}

// 处理一个已连接的客户端，直到它断开
void serveClient(int fd) {
  int yes = 1;                         // setsockopt 用的 "开启" 值
  // 关闭 Nagle: "pong" 和数据块立即发出，RTT 测量才准
  setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes));
  // send 最多阻塞 5 s，电脑端异常时不会永远卡住
  timeval tv = {5, 0};                 // 5 秒 0 微秒
  setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));  // 设置发送超时

  char line[32];                       // 存放一行命令 ("ping" / "start")
  int len = 0;                         // 当前这行已收到的字符数
  while (true) {                       // 一直处理命令，直到电脑断开
    char c;                            // 收到的一个字节
    int r = recv(fd, &c, 1, 0);        // 阻塞读命令，一次一个字节，按行处理
    if (r <= 0) break;                 // 返回 0 = 电脑正常断开，负数 = 出错; 都退出
    if (c == '\r') continue;           // 忽略回车符 (兼容 "\r\n" 换行)
    if (c != '\n') {                   // 还没到行尾
      if (len < (int)sizeof(line) - 1) line[len++] = c;  // 存入缓冲区 (留 1 字节给结尾的 0)，太长的部分丢弃
      continue;                        // 继续读下一个字节
    }
    line[len] = 0;                     // 收到换行: 加上字符串结束符
    len = 0;                           // 为下一行重置

    if (strcmp(line, "ping") == 0) {   // 电脑测 RTT 的命令
      sendAll(fd, (const uint8_t *)"pong\n", 5);   // 立即回应，电脑端计时
    } else if (strcmp(line, "start") == 0) {  // 电脑要求开始一轮测试
      runTest(fd);                     // 发送 TEST_DURATION_MS 毫秒的数据 + 结束块
    }                                  // 其他命令忽略
  }
}

void loop() {
  sockaddr_in from = {};               // 用来接收电脑的地址
  socklen_t fromLen = sizeof(from);    // 地址结构体长度 (accept 会改写)
  int fd = accept(listenSock, (sockaddr *)&from, &fromLen);   // 阻塞等待连接，返回新连接的描述符
  if (fd < 0) { delay(100); return; }  // accept 出错: 稍等再试 (loop 会再次被调用)

  char ip[16];                         // 存放电脑 IP 的文本 ("xxx.xxx.xxx.xxx" + 结尾 0)
  inet_ntoa_r(from.sin_addr, ip, sizeof(ip));  // 把二进制 IP 转成文本
  Serial.printf("客户端已连接: %s:%d\n", ip, ntohs(from.sin_port));  // 打印电脑地址和端口

  serveClient(fd);                     // 处理命令，直到电脑断开

  close(fd);                           // 关闭这个连接，释放资源
  Serial.printf("客户端已断开。在 TCP 端口 %d 等待下一次连接...\n", TCP_PORT);  // 回到等待状态
}
