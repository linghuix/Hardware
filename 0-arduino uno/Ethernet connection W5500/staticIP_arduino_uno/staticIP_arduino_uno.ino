#include <SPI.h>
#include <Ethernet.h>
// 静态IP

/* wire connection
W5500 模块,   Arduino Uno
MISO,         D12
MOSI,         D11
SCK (SCLK),   D13
CS (SCS/SS),  D10
VCC / 5V,     5V
GND,          GND
*/

// 1. MAC 地址 (保持不变)
byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};

// 2. CS 和 RST 引脚 (保持不变)
const int chipSelectPin = 10;
const int resetPin = 9; 

// -----------------------------------------------------------------
// 3. 配置您的静态 IP 地址 (在此处修改！)
// -----------------------------------------------------------------

// 根据您在 "步骤 A" 中找到的信息修改这里
IPAddress ip(192, 168, 1, 177);     // <<< 更改这里：设置一个未使用的 IP
IPAddress gateway(192, 168, 1, 1); // <<< 更改这里：您的默认网关
IPAddress subnet(255, 255, 255, 0);  // <<< 更改这里：您的子网掩码
IPAddress myDns(192, 168, 1, 1);     // (可选) DNS, 通常和网关相同

// -----------------------------------------------------------------

void setup() {
  Serial.begin(9600);
  while (!Serial) { ; }

  // (可选) 硬件复位
  pinMode(resetPin, OUTPUT);
  digitalWrite(resetPin, LOW);
  delay(100);
  digitalWrite(resetPin, HIGH);
  delay(100);

  // 初始化 W5500
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
}

void loop() {
  // 无操作
}