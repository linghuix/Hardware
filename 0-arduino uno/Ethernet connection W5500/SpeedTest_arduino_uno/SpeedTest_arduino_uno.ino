#include <SPI.h>
#include <Ethernet.h>

// --- 您的网络设置 ---
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED };
const int chipSelectPin = 10;
const int resetPin = 9; 

// 静态 IP (来自您之前的测试)
IPAddress ip(192, 168, 1, 177);     // <<< 更改这里：设置一个未使用的 IP
IPAddress gateway(192, 168, 1, 1); // <<< 更改这里：您的默认网关
IPAddress subnet(255, 255, 255, 0);  // <<< 更改这里：您的子网掩码
IPAddress myDns(192, 168, 1, 1);     // (可选) DNS, 通常和网关相同

// --- 目标服务器设置 ---
IPAddress serverIP(192, 168, 1, 2); // <<< 更改这里：您 PC 的 IP 地址
const int serverPort = 12348;         // <<< 必须匹配您 PC 上的 Ncat/NC 命令

// --- 测试参数 ---
// 发送 1 MB 的数据
const unsigned long totalData = 1024UL * 1024UL; 
// 数据包缓冲区大小。
// 尝试 512, 1024, 或 1460 (以太网典型 MTU)
// W5500 的内部缓冲区最大为 2048 (2K)
byte buffer[512]; 

EthernetClient client;

void setup() {
  // *** 关键：使用更高的波特率 ***
  Serial.begin(115200); 
  while (!Serial) { ; }
  Serial.println("--- W5500 吞吐量测试 (客户端) ---");

  // (可选) 硬件复位
  pinMode(resetPin, OUTPUT);
  digitalWrite(resetPin, LOW); delay(100);
  digitalWrite(resetPin, HIGH); delay(100);

  Ethernet.init(chipSelectPin);

  Serial.println("正在尝试使用静态 IP 地址...");

  // 使用静态 IP 启动以太网
  Ethernet.begin(mac, ip, myDns, gateway, subnet);

  // 短暂等待硬件启动
  delay(1000);

  // 检查连接状态 (Ethernet.linkStatus() 是一个更可靠的检查)
  if (Ethernet.linkStatus() == LinkOFF) {
    Serial.println("以太网连接失败! (未检测到网线或链接)");
  } else if (Ethernet.localIP() == IPAddress(0, 0, 0, 0)) {
    Serial.println("无法配置静态 IP 地址 (检查 SPI 接线)");
  } else {
    Serial.print("静态 IP 配置成功! IP 地址: ");
    Serial.println(Ethernet.localIP());
  }
  
  Serial.print("连接到服务器: ");
  Serial.println(serverIP);
  Serial.println("在串口监视器中输入 'g' 并按回车键开始测试...");

  // 用一些数据填充缓冲区（内容不重要）
  for (size_t i = 0; i < sizeof(buffer); i++) {
    buffer[i] = i % 256;
  }
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
      size_t bytesToSend = min(sizeof(buffer), totalData - bytesSent);
      
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