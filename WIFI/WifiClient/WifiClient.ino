#include <WiFi.h>


void datasend(void);

const char *ssid = "TP-LINK_LHX";
const char *password = "7758258X@";
//const char *ssid = "water";
//const char *password = "12345678";
//const char *server_ip = "192.168.43.1"; //python is server
const char *server_ip = "130.229.139.196"; //python is server
int server_port = 12345;

WiFiClient client;

void setup() {
  Serial.begin(115200);
  delay(10);

  // Set WiFi transmit power in dBm (between 0 and 20)
  WiFi.setTxPower(WIFI_POWER_19_5dBm );
  //  typedef enum {
  //    WIFI_POWER_19_5dBm = 78,// 19.5dBm
  //    WIFI_POWER_19dBm = 76,// 19dBm
  //    WIFI_POWER_18_5dBm = 74,// 18.5dBm
  //    WIFI_POWER_17dBm = 68,// 17dBm
  //    WIFI_POWER_15dBm = 60,// 15dBm
  //    WIFI_POWER_13dBm = 52,// 13dBm
  //    WIFI_POWER_11dBm = 44,// 11dBm
  //    WIFI_POWER_8_5dBm = 34,// 8.5dBm
  //    WIFI_POWER_7dBm = 28,// 7dBm
  //    WIFI_POWER_5dBm = 20,// 5dBm
  //    WIFI_POWER_2dBm = 8,// 2dBm
  //    WIFI_POWER_MINUS_1dBm = -4// -1dBm
  //} wifi_power_t;

  // Connect to WiFi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println("Connecting to WiFi...");
  }
  Serial.println("Connected to WiFi");

}


void loop() {
  
  if (!client.connected()) {
    // Connect to the server
    Serial.println("Connecting to server...");
    delay(1000);
    if (client.connect(server_ip, server_port)) {
      Serial.println("Connected to server");
      //client.println("Hello from ESP32!"); // Send data to the server
    }
    else {
      Serial.println("Connection failed");
    }
  }
  else {
    datasend();
  }
}


int bufferSize=50; // Calculate the required buffer size
int num; 
void datasend(void) {
  int sensorValue = analogRead(32);

  //Serial.println(sensorValue);

  //确保发送的整数在传输过程中保持固定的字节数
  char *buffer = new char[bufferSize]; // Dynamically allocate buffer
  sprintf(buffer, "%04d@", sensorValue);
  
  // Now, you can send the formatted data to the server
  num = client.print(buffer);
  Serial.println(sensorValue);

  delete[] buffer; // Free dynamically allocated memory

  // Now it's used to ensure that all data has been sent before doing anything else.
  client.flush();
  
  delay(2); // Wait for a while before reconnecting
}
