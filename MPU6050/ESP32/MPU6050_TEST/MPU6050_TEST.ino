#include <Arduino.h>

#include "IIC.h"
#include "Kalman.h"

/**
 * @date   2022/5/13
 * @author lhx
 * @brief  MPU6050芯片模块测试。 角速度，和角加速度的读取，以及角度的计算。MPU6050参考D:\整理\5-专业知识\0-硬件\100-硬件资料\1-传感器\MPU6050\Register Map and Descriptions-MPU-6000A.pdf。
 */

/* ----IMU Data---- */
Kalman kalmanZ;

#define gyroZ_OFF -0.19
double  accX, accY, accZ;
double  gyroX, gyroY, gyroZ;
int16_t tempRaw;

double pitch;

bool     stable = 0, battery_low = 0;
uint32_t last_unstable_time;
uint32_t last_stable_time;

uint8_t  i2cData[14];  // Buffer for I2C data
double   gyroZangle;   // Angle calculate using the gyro only
double   compAngleZ;   // Calculated angle using a complementary filter
double   kalAngleZ;    // Calculated angle using a Kalman filter
uint32_t timer;

float target_angle;

void setup()
{
  // put your setup code here, to run once:
  Serial.begin(115200);

  // kalman mpu6050 init
  Wire.begin(19, 18, 400000);  // 19-SDA, 18-SCL, Set I2C frequency to 400kHz

  i2cData[0] = 7;     // Sample Rate Divider Register. Set the sample  rate to 1000Hz - 8kHz(determined
                      // by Configuration Register)/(7+1) = 1000Hz
  i2cData[1] = 0x00;  // Configuration Register. Disable FSYNC and set 260 Hz Acc filtering, 256 Hz
                      // Gyro filtering, 8 KHz sampling
  i2cData[2] = 0x00;  // Gyroscope Configuration Register. Set Gyro Full Scale Range to ±250deg/s
  i2cData[3] = 0x00;  // Set Accelerometer Full Scale Range to ±2g
  while (i2cWrite(0x19, i2cData, 4, false))  // Write to all four registers at once
    ;
	
  // Power Management 1 Register. PLL with X axis gyroscope reference and disable sleep mode
  while (i2cWrite(0x6B, 0x01, true))
    ;

  // Read "WHO_AM_I" register
  while (i2cRead(0x75, i2cData, 1))
    ;
  if (i2cData[0] != 0x68) {
    Serial.print(F("Error reading sensor"));
    while (1)
      ;
  }
  delay(100);  // Wait for sensor to stabilize

  // Accelerometer Measurements register. Set kalman and gyro starting angle
  while (i2cRead(0x3B, i2cData, 6))
    ;
  accX         = (int16_t)((i2cData[0] << 8) | i2cData[1]);
  accY         = (int16_t)((i2cData[2] << 8) | i2cData[3]);
  accZ         = (int16_t)((i2cData[4] << 8) | i2cData[5]);
  
  double pitch = acc2rotation(accX, accY);
  kalmanZ.setAngle(pitch);
  gyroZangle = pitch;
  timer      = micros();
  Serial.println("kalman mpu6050 init");
}

void loop()
{
  // put your main code here, to run repeatedly:

  // 读取MPU6050数据
  while (i2cRead(0x3B, i2cData, 14))
    ;
  accX = (int16_t)((i2cData[0] << 8) | i2cData[1]);
  accY = (int16_t)((i2cData[2] << 8) | i2cData[3]);
  accZ = (int16_t)((i2cData[4] << 8) | i2cData[5]);
  // tempRaw = (int16_t)((i2cData[6] << 8) | i2cData[7]);
  gyroX = (int16_t)((i2cData[8] << 8) | i2cData[9]);
  gyroY = (int16_t)((i2cData[10] << 8) | i2cData[11]);
  gyroZ = (int16_t)((i2cData[12] << 8) | i2cData[13]);

  
  Serial.printf("%.4fg\t", accX/65535.0*4.0);
  Serial.printf("%.4fg\t", accY/65535.0*4.0);
  Serial.printf("%.4fg\t", accZ/65535.0*4.0);
  Serial.printf("%.4f\t", gyroX/65535.0*500.0);
  Serial.printf("%.4f\t", gyroY/65535.0*500.0);
  Serial.printf("%.4f\t", gyroZ/65535.0*500.0);
  Serial.printf("\r\n");


  delay(50);
}

