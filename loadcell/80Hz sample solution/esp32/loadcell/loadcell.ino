
/*
Example using the SparkFun HX711 breakout board with a scale
By: Nathan Seidle
SparkFun Electronics
Date: November 19th, 2014
License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

This is the calibration sketch. Use it to determine the calibration_factor that the main example uses. It also
outputs the zero_factor useful for projects that have a permanent mass on the scale in between power cycles.

Setup your scale and start the sketch WITHOUT a weight on the scale
Once readings are displayed place the weight on the scale
Press +/- or a/z to adjust the calibration_factor until the output readings match the known weight
Use this calibration_factor on the example sketch

This example assumes pounds (lbs). If you prefer kilograms, change the Serial.print(" lbs"); line to kg. The
calibration factor will be significantly different but it will be linearly related to lbs (1 lbs = 0.453592 kg).

Your calibration factor may be very positive or very negative. It all depends on the setup of your scale system
and the direction the sensors deflect from zero state
This example code uses bogde's excellent library: https://github.com/bogde/HX711
bogde's library is released under a GNU GENERAL PUBLIC LICENSE
Arduino pin 2 -> HX711 CLK
3 -> DOUT
5V -> VCC
GND -> GND

Most any pin on the Arduino Uno will be compatible with DOUT/CLK.

The HX711 board can be powered from 2.7V to 5V so the Arduino 5V power should be fine.

*/

#include "HX711.h"  // https://github.com/bogde/HX711

#define DOUT  13
#define CLK  14

#define RTE  15

#define DOUT_B  18
#define CLK_B  19

#define GAIN  128
#define GAIN_B  32

HX711 scale;
HX711 scale_B;

float calibration_factor = 22000; //-7050 worked for my 440lb max scale setup

long reading;

void setup() {

  // Choose output speed as 80 Hz
  pinMode(15, OUTPUT);
  digitalWrite(15, HIGH);

  Serial.begin(115200);
  Serial.println("HX711 calibration sketch");
  Serial.println("Remove all weight from scale");
  Serial.println("After readings begin, place known weight on scale");
  Serial.println("Press + or a to increase calibration factor");
  Serial.println("Press - or z to decrease calibration factor");

  scale.begin(DOUT, CLK, GAIN);
  scale.set_scale();
  scale.tare(); //Reset the scale to 0
  scale.power_up();

  scale_B.begin(DOUT_B, CLK_B, GAIN_B);
  scale_B.set_scale();
  scale_B.tare(); //Reset the scale to 0
  scale_B.power_up();

  long zero_factor = scale.read_average(); //Get a baseline reading
  Serial.print("Zero factor: "); //This can be used to remove the need to tare the scale. Useful in permanent scale projects.
  Serial.println(zero_factor);
  zero_factor = scale_B.read_average(); //Get a baseline reading
  Serial.print("Zero factor: "); //This can be used to remove the need to tare the scale. Useful in permanent scale projects.
  Serial.println(zero_factor);

  delay(1000);

}

void loop() {

  //scale.set_scale(calibration_factor); //Adjust to this calibration factor
  // Serial.print(scale.get_units(), 2);
  // Serial.print("kg_A"); //Change this to kg and re-adjust the calibration factor if you follow SI units like a sane person
  // Serial.print("   ");


  //scale_B.set_scale(calibration_factor); //Adjust to this calibration factor
  // Serial.print(scale_B.get_units(), 2);
  // Serial.print("kg_B"); //Change this to kg and re-adjust the calibration factor if you follow SI units like a sane person
  // Serial.println();


  // 4. Acquire reading without blocking
  if (scale.wait_ready_timeout(20)) {
      scale.set_scale(calibration_factor);
      Serial.print(scale.get_units(), 2);
      //Serial.print("kg_A");
  } else {
      Serial.print("HX711 not found.");
  }

  Serial.print("  ");

  if (scale_B.wait_ready_timeout(20)) {
      scale_B.set_scale(calibration_factor);
      Serial.print(scale_B.get_units(), 2);
      //Serial.print("kg_B");
  } else {
      Serial.print("HX711 not found.");
  }


  if(Serial.available())
  {
    char temp = Serial.read();
    if(temp == '+' || temp == 'a')
      calibration_factor += 10;
    else if(temp == '-' || temp == 'z')
      calibration_factor -= 10;
  }

  //delay(1);
  Serial.println();
}

