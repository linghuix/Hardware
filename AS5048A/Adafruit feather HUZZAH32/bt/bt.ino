#include "BluetoothSerial.h"
#include <AS5048A.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to enable it
#endif

BluetoothSerial SerialBT;

AS5048A angleSensor(04, false);

static float neutral_position = 0;
// Kalman filter variables
float x_hat = 0;        // Estimate of the state
float P = 1;            // Estimate error covariance
const float Q = 0.001;  // Process noise covariance (adjust as needed)
const float R = 0.01;   // Measurement noise covariance (adjust as needed)

void setup() {
  Serial.begin(115200);
  SerialBT.begin("ESP32test_right");
  Serial.println("The device started, now you can pair it with Bluetooth!");

  angleSensor.begin();

  // Determine the zero position dynamically
  determineZeroPosition();

  //Serial.print("Neutral Position: ");
  //Serial.println(neutral_position);

  Serial.print("New Zero Position: ");
  Serial.println(angleSensor.getZeroPosition());
}

void determineZeroPosition() {
  const int numSamples = 100;
  float sum = 0;

  Serial.println("Determining Zero Position...");

  for (int i = 0; i < numSamples; i++) {
    float reading = angleSensor.getRotationInDegrees();
    sum += reading;
    delay(10);
  }

  neutral_position = sum / numSamples;
}

float kalmanFilter(float measurement) {
  // Prediction step
  float x_hat_minus = x_hat;
  float P_minus = P + Q;

  // Update step
  float K = P_minus / (P_minus + R);
  x_hat = x_hat_minus + K * (measurement - x_hat_minus);
  P = (1 - K) * P_minus;

  return x_hat;
}

void loop() {
  float readingVal = angleSensor.getRotationInDegrees();
  //float filtered_reading = kalmanFilter(reading - neutral_position);

  // compensate initial offset
  readingVal = readingVal - neutral_position;

  if (readingVal > 180) 
  {
    readingVal = readingVal - 360;
  } 
  else if (readingVal < -180) 
  {
    readingVal = readingVal + 360;
  }

  // Send the filtered angle data to MATLAB over Bluetooth
  SerialBT.println(readingVal);
  Serial.println(readingVal);
  delay(10);  // Set sample frequency=100hz
}

