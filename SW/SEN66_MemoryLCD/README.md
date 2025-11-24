# SEN66 + ESPwled (ESP32-C3) + MemoryLCD + RGB LED

**Autor:** [laskakit.cz](https://www.laskakit.cz) (2025)

## Popis
Projekt pro čtení hodnot ze senzoru **Sensirion SEN66** (CO₂, teplota, vlhkost, VOC, NOx, PM2.5)  
a jejich zobrazení na **Sharp Memory LCD** (400x240).  

Hodnoty se vykreslují dynamicky – před každým překreslením se smaže oblast podle **bounding boxu** staré i nové hodnoty,  
aby nezůstávaly zbytky při změně délky čísla (např. `99 → 100`).  

**RGB LED** signalizuje kvalitu vzduchu podle koncentrace CO₂.

---

## Nutné knihovny
- [Adafruit_GFX](https://github.com/adafruit/Adafruit-GFX-Library)  
- [Adafruit_SharpMem](https://github.com/adafruit/Adafruit_SHARP_Memory_Display)  
- [Adafruit_NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel)  
- [SensirionI2cSen66](https://github.com/Sensirion/arduino-i2c-sen66)  

---

## Hardware
- [ESPwled (ESP32-C3)](https://www.laskakit.cz/laskakit-espwled/)  
- [SEN66 (I2C)](https://www.laskakit.cz/senserion-sen66-sin-t-senzor-kvality-ovzdusi/)  
- [Propojovací kabel I2C uŠup](https://www.laskakit.cz/laskakit-airboard-propojovaci-kabel-pro-senserion-sen6x-senzor-kvality-ovzdusi/)  
- [Sharp Memory LCD 400x240 (SPI)](https://www.laskakit.cz/laskakit-2-7--400x240-lcd-memory-displej/)  
- [Propojovací kabel SPI uŠup](https://www.laskakit.cz/--sup-spi-jst-sh-6-pin-kabel-10cm/)  

---

## Ukázka výstupu
- Na LCD se zobrazuje:  
  - CO₂ (ppm)  
  - Teplota (°C)  
  - Relativní vlhkost (%)  
  - VOC index  
  - NOx index  
  - PM2.5 (µg/m³)  

- RGB LED svítí:  
  - 🟢 zeleně při CO₂ < 800 ppm  
  - 🟡 žlutě při 800–1400 ppm  
  - 🔴 červeně při CO₂ > 1400 ppm  
  - 🔵 modře při chybě senzoru  
