# LaskaKit ESPwled

Are you looking for a board that is compatible with the WLED project, which makes it easy to control a variety of LED strips without programming? Then look no further.
Simply upload the WLED program into Laskakit ESPwled from [web browser](https://kno.wled.ge/).

First you connect the board via USB-C cable and then the ESP32-C3, on which the board is based, into recording mode. You do this by pressing and holding the FLASH button, pressing and releasing the RESET button, and then releasing the FLASH button as well. 
You then select the connected Laskakit ESPwled board in the browser and load the program. You will connect to the AP (Wi-Fi access point) that ESPwled created, via, for example, your phone, and fill in your Wi-Fi name and password. Then ESPwled already connects to your wifi and you control the connected LED strips via your browser on your phone, computer or tablet.

The GPIO data pin that is used to control the LED strips is 5, not pin 2 as it is in the default WLED settings -> so it is in the WLED settings to change this pin via the web browser. 
If the LED strip also has a CLK signal, this is connected to GPIO 4.

MicroESPink is equipped with our[universal I2C uŠup port connector](https://blog.laskakit.cz/predstavujeme-univerzalni-konektor-pro-propojeni-modulu-a-cidel-%CE%BCsup/) to which you can connect various sensors - [SHT40 (temperature, humidity)](https://www.laskakit.cz/laskakit-sht40-senzor-teploty-a-vlhkosti-vzduchu/), [BME280 (pressure, temperature, humidity)](https://www.laskakit.cz/arduino-senzor-tlaku--teploty-a-vlhkosti-bme280/), [SCD41 (CO2, temperature, humidity)](https://www.laskakit.cz/laskakit-scd41-senzor-co2--teploty-a-vlhkosti-vzduchu/), [BH1750 (lighting)](https://www.laskakit.cz/laskakit-bh1750-snimac-intenzity-osvetleni/), [APDS-9960 (gesture sensor)](https://www.laskakit.cz/laskakit-apds-9960-senzor-priblizeni-a-gest/) and many other sensors. </br>
Our I2C ushup connector is compatible with Adafruit Stemma and Qwiik from Sparkfun.</br>

The I2C bus is connected as follows, SDA GPIO10, SCL GPIO8. Before initializing the I2C module, it is necessary to add a definition of which pins are on which: wire.begin(10, 8); // (SDA,SCL)

In addition to the I2C uŠup connector, the board has an SPI uŠup connector to which you can connect other SPI peripherals. 

ESPwled has an addressable LED connected to GPIO 9. To control this LED we have a predefined [our example code on ESPwled github](https://github.com/LaskaKit/ESPwled/tree/main/SW).

You may need additional ones: The board comes without LED connector, the pitch is 2.54mm. You can buy [1pc 4pin JST-XH rigth angle](https://www.laskakit.cz/jst-xh-2-54mm-konektor-do-dps--pravouhly/?variantId=5393) or [2pc 2 pin screw terminal block for KF128-2.54 PCB](https://www.laskakit.cz/sroubovaci-svorkovnice-do-dps-kf128-2-54/?variantId=8871) or pin rail. Suitable pigtail with IPEX3 connector for IPEX versions - [Pigtail MHF3/IPEX3 - SMA Female](https://www.laskakit.cz/pigtail-mhf3-ipex3-sma-female--kabel-1-15mm--15cm/), 1.15mm cable, 15cm

Default WLED firmware does not support microphone input for ESP32-C3 (ESPwled), if the microphone input is needed it is necessary to compile WLED firmware with different setting. 

ESPwled connection you may check on the picture below
![ESPwled zapojení LED pásku](https://github.com/LaskaKit/ESPwled/blob/main/img/espwled_zapojeni.JPG)

The 5V LED strip can thus be powered from the USB-C that is on the board. The 5V, 12 and up to 24V strips then need an external power supply from which the ESPwled can also be powered. 

## ESPwled board is available on https://www.laskakit.cz/laskakit-espwled/?variantId=16928

