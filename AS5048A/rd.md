<img src="rd.assets/AS5048A_IM000200_1-00.png" style="zoom: 33%;" /> 

**Th**e **AS50**48A **i**s **a**n **ea**sy **t**o **us**e **36**0° **ang**le **posi**tion **sen**sor **(abso**lute **enco**der) **wi**th **a** **14-**bit **hi**gh **resol**ution **out**put **an**d **SP**I **inter**face. **Th**e **maxi**mum **sys**tem **accu**racy **i**s **0.0**5° 



### One device SPI mode

**MCU			              Adafruit ESP32 Feather**

3.3V/5V  <------------- VCC

SDA        <------------- A5 / pin 4

SCK        <------------- SCK / pin 5

MOSI     <------------- MOSI / pin 18

MISO    <------------- MISO / pin 19



### Lib requirement

AS5048A:  [AS5048A.zip](Adafruit feather HUZZAH32\LIB\AS5048A.zip) 

**NOTE !!!** 

in this library it sets 50 ms delay for ESP32 board. it is should be reset to 0

In AS5048A.cpp

```
/**
 * Set the delay acording to the microcontroller architecture
 */
void AS5048A::setDelay()
{
#if defined(ESP32) || defined(ARDUINO_ARCH_ESP32)
	this->esp32_delay = 50;  //this->esp32_delay = 0;
	if (this->debug)
	{
		Serial.println("AS5048A working with ESP32");
	}
#elif __AVR__
	this->esp32_delay = 0;
	if (this->debug)
	{
		Serial.println("AS5048A working with AVR");
	}
#else
	this->esp32_delay = 0;
	if (this->debug)
	{
		Serial.println("Device not detected");
	}
#endif
}
```



