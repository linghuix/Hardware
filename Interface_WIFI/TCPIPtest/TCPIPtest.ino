#include <WiFi.h>
extern "C" {
  #include "esp_wifi.h"
}

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
#define byteNum 4096
byte data_packet[byteNum] = {0xDE, 0xAD};
const long TEST_DURATION_US = 1000000; // 1,000,000 微秒 = 1 秒

void setup() {
  Serial.begin(115200);
  Serial.println("\nTCP 2字节最大频率测试 (服务器模式)");
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
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

  wifi_ap_record_t ap;
  if (esp_wifi_sta_get_ap_info(&ap) == ESP_OK) {
    Serial.printf("RSSI: %d dBm\n", ap.rssi);
    Serial.printf("phy_11b=%d, phy_11g=%d, phy_11n=%d\n",
                  ap.phy_11b, ap.phy_11g, ap.phy_11n);
  } else {
    Serial.println("esp_wifi_sta_get_ap_info failed");
  }
  esp_wifi_set_ps(WIFI_PS_NONE);
}

// 测试函数，对指定的客户端进行操作
void performTest(WiFiClient& currentClient) {
  Serial.println("为已连接的客户端开始 1 秒测试批次...");
  
  long packetCount = 0;
  long startTime = micros();
  long endTime = startTime + TEST_DURATION_US;

  //size_t space = client.availableForWrite();
  //Serial.printf("当前可写空间: %u 字节\n", space);

  // 运行一个紧凑的循环 1 秒钟
  while (micros() < endTime) {
    // 尝试写入 2 字节
    size_t written = currentClient.write(data_packet, byteNum);

    if (written == byteNum) {
      // 成功写入（缓冲区有空间）
      packetCount++;
    } else if (written > 0) {
        // 部分写入（少见），视为一次成功小包
        packetCount++;
    } else {
      // 没写入，短暂让出
      // 用短延迟让出 CPU，避免 busy spin
      delayMicroseconds(50);
    }
  }
  
  long actualDuration = micros() - startTime;

  // --- 计算并打印结果 ---
  float pps = (float)packetCount / (actualDuration / 1000000.0f);
  float dataRate_Mbps = (float)(packetCount * byteNum)  / actualDuration; // 兆比特/秒

  Serial.println("--- 批次完成 ---");
  Serial.printf("总计发送: %ld 个数据包\n", packetCount);
  Serial.printf("总时长: %ld us (约 1 秒)\n", actualDuration);
  Serial.printf("平均速率: %.0f 包/秒 (PPS)\n", pps);
  Serial.printf("数据吞吐量: %.2f MB/s\n", dataRate_Mbps);
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
  delay(10000); 
}
