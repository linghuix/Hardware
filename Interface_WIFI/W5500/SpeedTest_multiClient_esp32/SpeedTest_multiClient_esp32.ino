#include <SPI.h>
#include <Ethernet2.h>

// === 引脚定义（建议使用 VSPI） ===
#define ETH_CS   22
#define ETH_RST  21
#define ETH_SCK  18
#define ETH_MISO 19
#define ETH_MOSI 23

// --- 网络配置 ---
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
IPAddress ip(192, 168, 1, 177);       // ESP32 静态 IP
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);

// --- 服务器配置 ---
IPAddress serverIP(192, 168, 1, 2);   // PC IP
const int basePort = 12348;

// --- 测试参数 ---
const unsigned long totalDataPerSocket = 1024UL * 1024UL; // 每个 Socket 发送 1MB
const int socketCount = 4;                                  // 并行 Socket 数量
byte buffer[512];                                           // 发送缓冲区

EthernetClient clients[socketCount];

void setup() {
  Serial.begin(115200);
  while(!Serial);
  Serial.println("--- ESP32 + W5500 多 Socket 吞吐量测试 ---");

  // === 初始化 SPI ===
  Ethernet.init(ETH_CS);  // 告诉库使用哪个 CS 引脚

  // 硬件复位 W5500
  pinMode(ETH_RST, OUTPUT);
  digitalWrite(ETH_RST, LOW);
  delay(100);
  digitalWrite(ETH_RST, HIGH);
  delay(200);

  // 启动以太网
    Ethernet.begin(mac, IPAddress(192,168,1,177));
    Serial.print("静态 IP: ");
    Serial.println(Ethernet.localIP());

  // 填充发送缓冲区
  for (size_t i = 0; i < sizeof(buffer); i++) buffer[i] = i % 256;
}

void loop() {
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'g') runTest();
  }
}

void runTest() {

  Serial.println("\n=== 连接所有 Socket（不同端口） ===");

  for (int i=0; i<socketCount; i++) {
    int port = basePort + i; // 每个 Socket 不同端口
    if (clients[i].connect(serverIP, port)) {
      Serial.print("Socket "); Serial.print(i);
      Serial.print(" 连接成功，端口: "); Serial.println(port);
    } else {
      Serial.print("Socket "); Serial.print(i);
      Serial.print(" 连接失败，端口: "); Serial.println(port);
    }
  }

  Serial.println("--- 开始发送数据 ---");
  unsigned long startTime = millis();
  unsigned long bytesSent[socketCount] = {0};

  bool allDone = false;
  while(!allDone) {
    allDone = true;
    for(int i=0; i<socketCount; i++) {
      if(bytesSent[i] < totalDataPerSocket) {
        size_t toSend = (totalDataPerSocket - bytesSent[i] < sizeof(buffer)) ? (totalDataPerSocket - bytesSent[i]) : sizeof(buffer);
        size_t written = clients[i].write(buffer, toSend);
        if(written > 0) bytesSent[i] += written;
        allDone = false;
      }
    }
  }

  unsigned long endTime = millis();

  // 关闭 Socket
  for(int i=0; i<socketCount; i++) clients[i].stop();

  // 统计总吞吐量
  unsigned long totalBytes = 0;
  for(int i=0;i<socketCount;i++) totalBytes += bytesSent[i];

  float seconds = (endTime - startTime)/1000.0;
  float speedKBps = totalBytes / 1024.0 / seconds;
  float speedMbps = totalBytes * 8.0 / 1000000.0 / seconds;

  Serial.println("--- 测试结果 ---");
  Serial.print("Socket 数量: "); Serial.println(socketCount);
  Serial.print("总发送数据: "); Serial.print(totalBytes/1024); Serial.println(" KB");
  Serial.print("耗时: "); Serial.print(seconds); Serial.println(" 秒");
  Serial.print("平均速度: "); Serial.print(speedKBps); Serial.println(" KB/s");
  Serial.print("平均速度: "); Serial.print(speedMbps); Serial.println(" Mbps");
  Serial.println("--- 测试完成 ---\n输入 'g' 可再次测试");
}
