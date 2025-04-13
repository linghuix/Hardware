

[**Preci**sion **AD**Cs](https://www.ti.com/data-converters/adc-circuit/precision-adcs/overview.html?keyMatch=&tisearch=search-everything&usecase=partmatches)



### ESP32 - 80Hz solution



**LIb requirement**

HX711：   [HX711-master.zip](esp32\loadcell\lib\HX711-master.zip) 

<img src="README.assets/image-20250409180350905.png" style="zoom:50%;" />

**code** 

 [loadcell.ino](loadcell\loadcell.ino) 



**hardware**

*ADC and amplifier*

<img src="README.assets/load-cell-click-inside-image.jpg" style="zoom:40%;" />  

图 board information refers to its [official website](https://www.mikroe.com/load-cell-click). **RTE pin** is used to select speed of output,  80 samples per second (high) or 10 Hz





<img src="README.assets/IMG_20231211_165405.jpg" style="zoom:3%;" /> <img src="README.assets/IMG_20231211_165420.jpg" style="zoom: 3%;" />



*load cell 200kg capability* 

<img src="README.assets/IMG_20231211_165436.jpg" style="zoom:3%;" />  



*microcontroller  adafruit ESP32 Feather*  

<img src="README.assets/IMG_20231211_165333.jpg" style="zoom:3%;" /> 



*Connection*  

<img src="README.assets/ele.jpg" alt="ele" style="zoom:50%;" /> 



