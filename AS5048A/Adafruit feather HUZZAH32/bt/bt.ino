#include "BluetoothSerial.h"
#include <AS5048A.h>

#if !defined(CONFIG_BT_ENABLED) || !defined(CONFIG_BLUEDROID_ENABLED)
#error Bluetooth is not enabled! Please run `make menuconfig` to enable it
#endif

BluetoothSerial SerialBT;

AS5048A angleSensor(04, false);

static float neutral_position = 0;

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


// Kalman filter variables
float x_hat = 0;        // Estimate of the statec:\Users\lingh\Downloads\libraries\AS5048\src\AS5048A.h
float P = 1;            // Estimate error covariance
const float Q = 0.001;  // Process noise covariance (adjust as needed)
const float R = 0.01;   // Measurement noise covariance (adjust as needed)
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


// filter info
const float samplingFrequency = 100.0;   // Sampling frequency in Hz
const float cutoffFrequency = 5.0;     // Cutoff frequency in Hz
const float tau = 1.0 / (2.0 * PI * cutoffFrequency);   // Time constant for the filter
const float alpha = 1.0 / (1.0 + tau * samplingFrequency);
float filtered_measurement = 0;
float lowPassFilter(float measurement){

      filtered_measurement = alpha * measurement + (1.0 - alpha) * filtered_measurement;
      return filtered_measurement;
}

void loop() {
  float readingVal = angleSensor.getRotationInDegrees();

  // compensate initial offset
  readingVal = readingVal - neutral_position;

  float filtered_reading = kalmanFilter(readingVal);

  if (filtered_reading > 180) 
  {
    filtered_reading = filtered_reading - 360;
  } 
  else if (filtered_reading < -180) 
  {
    filtered_reading = filtered_reading + 360;
  }

  // Send the filtered angle data to MATLAB over Bluetooth
  SerialBT.println(filtered_reading);
  Serial.println(filtered_reading);
  delay(10);  // Set sample frequency=100hz
}

