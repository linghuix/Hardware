/*
 * ESP32 UDP 发送速率测试
 *
 * 和 NetworkTest (TCP) 对比用。UDP 不等 ACK，不受 Arduino 核心里 5.7 KB
 * TCP 发送缓冲区的限制，所以能测出这块板子 Wi-Fi 的真实发送上限。
 *
 * 用法:
 *   1. 上传本程序，串口监视器 921600，记下打印的 ESP32 IP
 *   2. 电脑上运行同目录下的接收脚本 (需要 Python 3):
 *        python udp_receiver.py 192.168.137.110
 *      脚本先发一个 "start" 包给 ESP32，ESP32 记下电脑的地址，然后连续
 *      发送 TEST_DURATION_MS 毫秒，最后发 "END" 包附带自己的统计。
 *   3. 比较两边的数字:
 *        ESP32 端: 成功交给网络栈的包数 / 发送失败数
 *        电脑端:   实际收到的包数 / 丢包率 / 实际 Mbps   <- 真实速度看这个
 *
 * 每个包: [4 字节序号 (小端)] + 填充数据，共 PAYLOAD_SIZE 字节。
 * 电脑端用序号算丢包。
 */

#include <WiFi.h>
#include "lwip/sockets.h"

// ---------------------------------
// Wi-Fi: 电脑的 Windows 移动热点 (频段要设成 2.4 GHz)
const char* ssid = "YOUR_SSID";
const char* password = "YOUR_PASSWORD";
// ---------------------------------

const int UDP_PORT = 5005;            // ESP32 监听 "start" 的端口
const int PAYLOAD_SIZE = 1400;        // 每包字节数。<= 1472 才不会被 IP 分片
const uint32_t TEST_DURATION_MS = 5000;

// 发送速率限制 (KB/s)。0 = 不限速，尽可能快。
// 不限速时 ESP32 的发送队列会经常满 ("发送失败" 很多) 且丢包率高；
// 想找"不丢包的最高速率"，可以从 500 开始逐步调高 (1000, 1500, 2000 ...)。
const uint32_t TARGET_KBPS = 0;

int sock = -1;
uint8_t packet[PAYLOAD_SIZE];

void setup() {
  Serial.begin(921600);
  Serial.println("\nUDP 发送速率测试");

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);   // 关闭 Wi-Fi 省电，否则速率严重偏低
  Serial.print("正在连接到 ");
  Serial.println(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi 已连接!");
  Serial.print("ESP32 IP 地址: ");
  Serial.println(WiFi.localIP());
  Serial.printf("信号强度 RSSI: %d dBm\n", WiFi.RSSI());

  // 直接用 lwIP 的 socket 接口: 比 WiFiUDP 少一次拷贝，失败时也能拿到 errno
  sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  sockaddr_in addr = {};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(UDP_PORT);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);
  if (sock < 0 || bind(sock, (sockaddr *)&addr, sizeof(addr)) < 0) {
    Serial.println("UDP socket 创建失败!");
    while (true) delay(1000);
  }

  for (int i = 0; i < PAYLOAD_SIZE; i++) packet[i] = (uint8_t)i;  // 填充数据
  Serial.printf("在 UDP 端口 %d 等待电脑发送 \"start\"...\n", UDP_PORT);
}

void runTest(const sockaddr_in &pc) {
  char ip[16];
  inet_ntoa_r(pc.sin_addr, ip, sizeof(ip));
  Serial.printf("开始向 %s:%d 发送 %u ms (限速: %s)...\n", ip, ntohs(pc.sin_port),
                (unsigned)TEST_DURATION_MS,
                TARGET_KBPS ? String(TARGET_KBPS).c_str() : "不限");

  uint32_t seq = 0, sentOk = 0, sendFail = 0;
  // 限速: 每个包之间的间隔 (微秒)
  uint32_t gapUs = 0;
  if (TARGET_KBPS > 0) {
    uint64_t bytesPerSec = (uint64_t)TARGET_KBPS * 1024ULL;
    gapUs = (uint32_t)(PAYLOAD_SIZE * 1000000ULL / bytesPerSec);
  }
  uint32_t start = millis();
  uint32_t nextUs = micros();

  while (millis() - start < TEST_DURATION_MS) {
    if (gapUs) {
      while ((int32_t)(micros() - nextUs) < 0) { }   // 等到下一个发送时刻
      nextUs += gapUs;
    }
    memcpy(packet, &seq, 4);   // 序号放在包头 (ESP32 是小端)
    int r = sendto(sock, packet, PAYLOAD_SIZE, 0, (const sockaddr *)&pc, sizeof(pc));
    if (r == PAYLOAD_SIZE) {
      sentOk++;
      seq++;
    } else {
      // 通常是 errno 12 (ENOMEM): Wi-Fi 发送队列满了。稍等让它发出去再试。
      // 序号不增加，所以电脑端算出的丢包只包括"空中丢失"的包。
      sendFail++;
      delayMicroseconds(100);
    }
  }
  uint32_t elapsed = millis() - start;

  float secs = elapsed / 1000.0f;
  float kbps = sentOk * (float)PAYLOAD_SIZE / 1024.0f / secs;
  Serial.println("--- 批次完成 (ESP32 端) ---");
  Serial.printf("成功交给网络栈: %u 包 (%u 字节)\n", sentOk, sentOk * PAYLOAD_SIZE);
  Serial.printf("发送失败 (队列满): %u 次\n", sendFail);
  Serial.printf("时长: %u ms\n", elapsed);
  Serial.printf("ESP32 发送速率: %.2f Mbps (%.0f KB/s)\n", kbps * 1024 * 8 / 1e6, kbps);
  Serial.println("(真实速率以电脑端收到的为准)");
  Serial.printf("RSSI: %d dBm\n", WiFi.RSSI());
  Serial.println("--------------------");

  // 结束包: "END" + 发出的包数 + 时长。发几次以防 UDP 丢失。
  char endMsg[48];
  int n = snprintf(endMsg, sizeof(endMsg), "END %u %u", sentOk, elapsed);
  for (int i = 0; i < 5; i++) {
    sendto(sock, endMsg, n, 0, (const sockaddr *)&pc, sizeof(pc));
    delay(20);
  }
}

void loop() {
  // 非阻塞地检查有没有 "start" 包
  char buf[32];
  sockaddr_in from = {};
  socklen_t fromLen = sizeof(from);
  int n = recvfrom(sock, buf, sizeof(buf) - 1, MSG_DONTWAIT, (sockaddr *)&from, &fromLen);
  if (n > 0) {
    buf[n] = 0;
    if (strncmp(buf, "start", 5) == 0) {
      runTest(from);
      // Discard "start" packets that arrived while we were busy sending,
      // otherwise they would trigger extra rounds nobody is listening to.
      while (recvfrom(sock, buf, sizeof(buf), MSG_DONTWAIT, NULL, NULL) > 0) { }
      Serial.printf("在 UDP 端口 %d 等待下一次 \"start\"...\n", UDP_PORT);
    }
  }
  delay(10);
}
