#include <WiFi.h>

// ---------------------------------
// !!! 修改为你的 Wi-Fi 凭据 !!!
const char* ssid = "TP-LINK_LHX"; 
const char* password = "YOUR_PASSWORD";
// ---------------------------------

// 服务器端口配置
const int tcpPort = 8080;
WiFiServer server(tcpPort);
WiFiClient client; // 用于保存已连接的客户端

// 测试数据包和时长
byte data_packet[2] = {0xDE, 0xAD};
const long TEST_DURATION_US = 1000000; // 1,000,000 微秒 = 1 秒

void setup() {
  Serial.begin(115200);
  Serial.println("\nTCP 2字节最大频率测试 (服务器模式)");

  // 连接到 Wi-Fi
  WiFi.begin(ssid, password);
  Serial.print("正在连接到 ");
  Serial.println(ssid);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi 已连接!");
  Serial.print("ESP32 IP 地址: ");
  Serial.println(WiFi.localIP()); // <-- 记下这个 IP 地址

  // 启动服务器
  server.begin();
  Serial.printf("服务器已在端口 %d 上启动\n", tcpPort);
  Serial.println("等待客户端连接...");
}

// 测试函数，对指定的客户端进行操作
void performTest(WiFiClient& currentClient) {
  Serial.println("为已连接的客户端开始 1 秒测试批次...");
  
  long packetCount = 0;
  long startTime = micros();
  long endTime = startTime + TEST_DURATION_US;

  // 运行一个紧凑的循环 1 秒钟
  while (micros() < endTime) {
    // 尝试写入 2 字节
    size_t written = currentClient.write(data_packet, 2);

    if (written == 2) {
      // 成功写入（缓冲区有空间）
      packetCount++;
    } else {
      // 写入失败 (written == 0)，意味着 TCP 发送缓冲区已满
      // 我们不增加计数器，循环将立即再次尝试
      // 这会“自旋”，直到缓冲区有空间
    }
  }
  
  long actualDuration = micros() - startTime;

  // --- 计算并打印结果 ---
  float pps = (float)packetCount / (actualDuration / 1000000.0f);
  float dataRate_Mbps = (packetCount * 2.0 * 8.0) / actualDuration; // 兆比特/秒

  Serial.println("--- 批次完成 ---");
  Serial.printf("总计发送: %ld 个数据包\n", packetCount);
  Serial.printf("总时长: %ld us (约 1 秒)\n", actualDuration);
  Serial.printf("平均速率: %.0f 包/秒 (PPS)\n", pps);
  Serial.printf("数据吞吐量: %.2f Mbps\n", dataRate_Mbps);
  Serial.println("--------------------");
}


void loop() {
  // 检查是否有客户端连接或现有客户端是否断开
  while (!client || !client.connected()) {
    // 如果之前有客户端，则停止它
    if (client) {
      client.stop();
      Serial.println("客户端已断开。等待新的连接...");
    }
    // 检查是否有新的客户端连接进来
    client = server.available();
    if (client) {
      Serial.print("新的客户端已连接，来自: ");
      Serial.println(client.remoteIP());
      // !!! 关键 !!!
      // 禁用 Nagle 算法，强制立即发送小数据包
      client.setNoDelay(true);
    }
  }
  
  // 如果代码运行到这里，说明我们有一个已连接的客户端
  
  // 运行测试
  performTest(client);
  
  // 在批次之间暂停 2 秒，以便查看结果
  delay(2000); 
}
