
#include <SimpleFOC.h>

// magnetic sensor instance - IIC
MagneticSensorI2C sensor = MagneticSensorI2C(0x36, 12, 0x0E, 4);
//MagneticSensorI2C sensor = MagneticSensorI2C(AS5600_I2C);
// MagneticSensorI2C(uint8_t _chip_address, float _cpr, uint8_t _angle_register_msb)
//  chip_address         - I2C chip address
//  bit_resolution       - resolution of the sensor
//  angle_register_msb   - angle read register msb
//  bits_used_msb        - number of used bits in msb register

void setup(){
  
  Wire1.begin(23, 5, 400000);
  
  sensor.init(&Wire1);
  
  // use monitoring with serial
  Serial.begin(115200);
  Serial.println("Sensor ready");
  
  delay(500);
} 

void loop() {
  // iterative function updating the sensor internal variables
  // it is usually called in motor.loopFOC()
  // this function reads the sensor hardware and 
  // has to be called before getAngle nad getVelocity
  sensor.update();

  _delay(10);
  
  // display the angle and the angular velocity to the terminal
  Serial.print(sensor.getAngle());                        // 0-2pi rad
  Serial.print("\t");
  
  Serial.print(sensor.getAngle() / 3.1415926 * 180.0);    // 0-360°
  Serial.print("\t");
  
  Serial.println(sensor.getVelocity());                   // rad/s
}
