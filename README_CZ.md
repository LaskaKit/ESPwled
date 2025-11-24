![ESPwled](https://github.com/LaskaKit/ESPwled/blob/main/img/13115-1.jpg)
# LaskaKit ESPwled

Hledáš desku, která je kompatibilní s projektem WLED, který přináší naprosto jednoduché ovládání nejrůznějších LED pásků bez nutnosti programování? Pak už dál hledat nemusíš.
Program WLED do Laskakit ESPwled nahraješ jednoduše z [webového prohlížeče projektu](https://kno.wled.ge/).

Nejprve připojíš desku skrze USB-C kabel a pak ESP32-C3, na kterém je deska založena, do nahrávacího režimu. To provedeš tak, že zmáčkneš a držíš tlačítko FLASH, zmáčkneš a uvolníš tlačítko RESET a poté uvolníš i tlačítko FLASH. 
Poté v prohlížeči vybereš připojenou Laskakit ESPwled desku a nahraješ program. Připojíš se na AP (Wi-Fi access point), kterou ESPwled vytvořil, přes třeba tvůj telefon a vyplníš jméno a heslo tvé Wi-Fi. Poté se ESPwled už připojí do tvé wifi a přes tvůj prohlížeč v telefonu, počítači nebo tabletu ovládáš připojené LED pásky.

DAtový GPIO pin, který se používá na ovládání LED pásků je 5, ne pin 2 jak je ve výchozím nastavení WLED -> je tedy v nastavení WLED tento pin změnit skrze webový prohlížeč. 
Pokud má LED pásek i CLK signál, ten je připojen na GPIO 4.

Deska samotná obsahuje navíc [náš univerzální I2C uŠup konektor](https://blog.laskakit.cz/predstavujeme-univerzalni-konektor-pro-propojeni-modulu-a-cidel-%CE%BCsup/) ke kterým můžeš připojit nejrůznější čidla - [SHT40 (teplota, vlhkost)](https://www.laskakit.cz/laskakit-sht40-senzor-teploty-a-vlhkosti-vzduchu/), [BME280 (tlak, teplota, vlhkost)](https://www.laskakit.cz/arduino-senzor-tlaku--teploty-a-vlhkosti-bme280/), [SCD41 (CO2, teplota, vlhkost)](https://www.laskakit.cz/laskakit-scd41-senzor-co2--teploty-a-vlhkosti-vzduchu/), [BH1750 (osvětlení)](https://www.laskakit.cz/laskakit-bh1750-snimac-intenzity-osvetleni/), [APDS-9960 (senzor gest)](https://www.laskakit.cz/laskakit-apds-9960-senzor-priblizeni-a-gest/) a spoustu další senzorů. </br>
Náš I2C uŠup konektor je kompatibilní s Adafruit Stemma a Qwiik od Sparkfun.</br>
I2C sběrnice je připojeno takto, SDA GPIO10, SCL GPIO8. Je potřeba před iniciací i2c modulu přidat definici na kterých PINech jsou: Wire.begin(10, 8); // (SDA,SCL)

Kromě I2C uŠup konektoru má deska i SPI uŠup konektor, ke kterému můžeš připojit další SPI periférie. 

ESPwled má osazenou adresovatelnou LED připojenou na GPIO 9. Pro ovládání této LED máme předpřiravený [vlastní vzorový kód na našem github](https://github.com/LaskaKit/ESPwled/tree/main/SW).

Dodatečné možna budeš potřebovat:
Deska je dodávána bez LED konektoru, rozteč je 2.54mm. Můžeš dokoupit 1ks [4pin JST-XH pravoúhlý](https://www.laskakit.cz/jst-xh-2-54mm-konektor-do-dps--pravouhly/?variantId=5393) nebo [2ks 2 pin šroubovací svorkovnice do DPS KF128-2.54](https://www.laskakit.cz/sroubovaci-svorkovnice-do-dps-kf128-2-54/?variantId=8871) anebo kolíkovou lištu.
Vhodný pigtail s konektorem IPEX3 pro verze IPEX - [Pigtail MHF3/IPEX3 - SMA Female](https://www.laskakit.cz/pigtail-mhf3-ipex3-sma-female--kabel-1-15mm--15cm/), kabel 1,15mm, 15cm

Výchozí WLED firmware nepodporuje vstup z mikrofonu pro ESP32-C3 (ESPwled), pokud je mikrofonní vstup potřeba, je nutné si zkompilovat WLED firmware s jiným nastavením.

Zapojení LED pásku vidíš na obrázku níže
![ESPwled zapojení LED pásku](https://github.com/LaskaKit/ESPwled/blob/main/img/espwled_zapojeni.JPG)

5V LED pásek tak může být napájen z USB-C, který je na desce. Pásky 5V, 12 a až 24V pak potřebují externí zdroj ze kterého se také může ESPwled napájet. 

## ESPwled je k dostání na https://www.laskakit.cz/laskakit-espwled/?variantId=16928
