#include <SPI.h>
#include <Ethernet2.h>   // 你已安装的库！

// === 引脚定义（建议使用 VSPI） ===
#define ETH_CS   22
#define ETH_RST  21
#define ETH_SCK  18
#define ETH_MISO 19
#define ETH_MOSI 23

byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };  // 随意定义 MAC

SPIClass vspi(VSPI);     // 使用 VSPI 控制器
EthernetClient client;

// --- 目标服务器设置 ---
IPAddress serverIP(192, 168, 1, 2); // <<< 更改这里：您 PC 的 IP 地址
const int serverPort = 12348;         // <<< 必须匹配您 PC 上的 Ncat/NC 命令


// --- 测试参数 ---
// 发送 1 MB 的数据
const unsigned long totalData = 1024UL * 1024UL; 
// 数据包缓冲区大小。
// 尝试 512, 1024, 或 1460 (以太网典型 MTU)
// W5500 的内部缓冲区最大为 2048 (2K)
byte buffer[2048]; 


void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n[ESP32 + W5500 初始化中...]");

  // === 初始化 SPI ===
  //vspi.begin(ETH_SCK, ETH_MISO, ETH_MOSI, ETH_CS);
  Ethernet.init(ETH_CS);  // 告诉库使用哪个 CS 引脚

  // === 复位 W5500 ===
  pinMode(ETH_RST, OUTPUT);
  digitalWrite(ETH_RST, LOW);
  delay(100);
  digitalWrite(ETH_RST, HIGH);
  delay(200);

  // === DHCP 启动 ===
  /*if (Ethernet.begin(mac)) {
    Serial.print(" DHCP 成功, IP 地址: ");
    Serial.println(Ethernet.localIP());
  } else {
    Serial.println(" DHCP 失败，使用静态 IP。");*/
    Ethernet.begin(mac, IPAddress(192,168,1,177));
    Serial.print("静态 IP: ");
    Serial.println(Ethernet.localIP());
  //}

  delay(1000);
  Serial.println("🌐 W5500 已准备就绪。");
}

void loop() {
  if (Serial.available() > 0) {
    char c = Serial.read();
    if (c == 'g') {
      runTest();
    }
  }
}

void runTest() {
  Serial.println("\n正在连接到服务器...");
  
  if (client.connect(serverIP, serverPort)) {
    Serial.println("连接成功! 开始发送数据...");

    unsigned long bytesSent = 0;
    unsigned long startTime = micros();

    while (bytesSent < totalData) {
      // 计算这次要发送多少
      size_t bytesToSend = (totalData - bytesSent < sizeof(buffer)) ? (totalData - bytesSent) : sizeof(buffer);
      
      // 发送数据
      // client.write() 是一个阻塞调用
      // 它会等待 W5500 确认数据已放入其 Tx 缓冲区
      size_t wrote = client.write(buffer, bytesToSend);

      if (wrote <= 0) {
        Serial.println("发送失败, 连接断开?");
        break; // 退出循环
      }
      
      bytesSent += wrote;
    }

    unsigned long endTime = micros();
    client.stop();

    Serial.println("...发送完成!");

    // --- 计算结果 ---
    unsigned long duration = endTime - startTime;
    float seconds = duration / 1000000.0;
    float speedKBps = (totalData / 1024.0) / seconds; // 千字节/秒
    float speedMbps = (totalData * 8.0) / seconds / 1000000.0; // 兆比特/秒

    Serial.println("--- 结果 ---");
    Serial.print("发送数据: ");
    Serial.print(totalData / 1024UL); Serial.println(" KB");
    Serial.print("耗时: ");
    Serial.print(seconds); Serial.println(" 秒");
    Serial.println("---");
    Serial.print("平均速度: ");
    Serial.print(speedKBps); Serial.println(" KB/s");
    Serial.print("平均速度: ");
    Serial.print(speedMbps); Serial.println(" Mbps");
    Serial.println("\n再次输入 'g' 进行测试...");

  } else {
    Serial.println("连接失败!");
    Serial.println("请检查:");
    Serial.println("1. 您的 PC IP 是否正确?");
    Serial.println("2. PC 上的 netcat/ncat 是否已在端口 5000 上运行?");
    Serial.println("3. 防火墙是否允许端口 5000?");
  }
}
