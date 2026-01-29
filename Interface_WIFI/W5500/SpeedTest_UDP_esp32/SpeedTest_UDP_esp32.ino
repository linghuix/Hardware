#include <SPI.h>
#include <Ethernet2.h>   // 已安装的库

// === 引脚定义（建议使用 VSPI） ===
#define ETH_CS   22
#define ETH_RST  21
#define ETH_SCK  18
#define ETH_MISO 19
#define ETH_MOSI 23

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };  // 随意定义 MAC

EthernetUDP udp;

// --- 目标服务器设置 ---
IPAddress serverIP(192, 168, 1, 2); // 服务器 IP
const int serverPort = 12348;       // 目标端口

// --- 测试参数 ---
const unsigned long totalData = 1024UL * 1024UL; // 发送 1MB 数据
byte buffer[2048];                               // UDP 一次发送缓冲区，建议 512~1460

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[ESP32 + W5500 UDP 初始化中...]");

  Ethernet.init(ETH_CS);  // 告诉库使用哪个 CS 引脚

  // === 复位 W5500 ===
  pinMode(ETH_RST, OUTPUT);
  digitalWrite(ETH_RST, LOW);
  delay(100);
  digitalWrite(ETH_RST, HIGH);
  delay(200);

  // === DHCP 启动 ===
  Ethernet.begin(mac, IPAddress(192,168,1,177));
  Serial.print("静态 IP: ");
  Serial.println(Ethernet.localIP());

  delay(1000);
  Serial.println("🌐 W5500 已准备就绪，输入 'g' 开始测试...");

  udp.begin(serverPort); // 本地端口，任意未占用的端口

}

void loop() {
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'g') runTest();
  }
}

void runTest() {
  Serial.println("\n=== UDP 发送测试 ===");

  unsigned long bytesSent = 0;
  unsigned long startTime = micros();

  while (bytesSent < totalData) {
    size_t toSend = (totalData - bytesSent < sizeof(buffer)) ? (totalData - bytesSent) : sizeof(buffer);

    udp.beginPacket(serverIP, serverPort);
    udp.write(buffer, toSend);
    udp.endPacket();  // UDP 发送完成，不阻塞等待 ACK

    bytesSent += toSend;
  }

  unsigned long endTime = micros();

  // --- 计算结果 ---
  float seconds = (endTime - startTime) / 1000000.0;
  float speedKBps = (totalData / 1024.0) / seconds;      // KB/s
  float speedMbps = (totalData * 8.0) / 1000000.0 / seconds; // Mbps

  Serial.println("--- 结果 ---");
  Serial.print("发送数据: "); Serial.print(totalData / 1024UL); Serial.println(" KB");
  Serial.print("耗时: "); Serial.print(seconds, 6); Serial.println(" 秒");
  Serial.print("平均速度: "); Serial.print(speedKBps); Serial.println(" KB/s");
  Serial.print("平均速度: "); Serial.print(speedMbps); Serial.println(" Mbps");
  Serial.println("--- 测试完成 ---\n再次输入 'g' 可测试");
}
