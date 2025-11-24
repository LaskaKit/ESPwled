/*
 * Projekt: SEN66 + ESPwled (ESP32-C3) + MemoryLCD + RGB LED
 * Autor: laskakit.cz (2025)
 *
 * Popis:
 * Čtení hodnot z čidla SEN66 (CO2, teplota, vlhkost, VOC, NOx, PM2.5) 
 * a jejich zobrazení na Sharp Memory LCD. 
 * Hodnoty se vykreslují dynamicky – před každým překreslením se smaže oblast 
 * podle bounding boxu staré i nové hodnoty, aby nezůstávaly zbytky při změně 
 * délky čísla (např. 99 → 100). 
 * RGB LED signalizuje kvalitu vzduchu dle koncentrace CO2.
 *
 * Nutné knihovny:
 *  - Adafruit_GFX
 *  - Adafruit_SharpMem
 *  - Adafruit_NeoPixel
 *  - SensirionI2cSen66
 *
 * Hardware:
 *  - ESPwled https://www.laskakit.cz/laskakit-espwled/
 *  - SEN66 (I2C) https://www.laskakit.cz/senserion-sen66-sin-t-senzor-kvality-ovzdusi/
 *  - Propojovací kabel I2C uŠup https://www.laskakit.cz/laskakit-airboard-propojovaci-kabel-pro-senserion-sen6x-senzor-kvality-ovzdusi/ 
 *  - Sharp Memory LCD 400x240 (SPI) https://www.laskakit.cz/laskakit-2-7--400x240-lcd-memory-displej/
 *  - Propojovací kabel SPI uŠup https://www.laskakit.cz/--sup-spi-jst-sh-6-pin-kabel-10cm/
 */


#include <Wire.h>
#include <SPI.h>
#include <Adafruit_NeoPixel.h>
#include <Adafruit_GFX.h>
#include <SensirionI2cSen66.h>
#include <Adafruit_SharpMem.h>
#include <Fonts/FreeMono12pt7b.h>    // malý font cca 16px
#include <Fonts/FreeMonoBold24pt7b.h>   // velký font cca 30px (18pt * 1.5)

#define PIN_SDA        10
#define PIN_SCL        8

#define PIN_SPI_CLK    6
#define PIN_SPI_MOSI   7
#define PIN_SPI_MISO   2
#define PIN_SPI_CS     3

#define PIN_RGB        9

#define BLACK 0
#define WHITE 1
#define NO_ERROR 0

Adafruit_NeoPixel rgbLed(1, PIN_RGB, NEO_GRB + NEO_KHZ800);
SensirionI2cSen66 sen66Sensor;

#define LCD_WIDTH   400  // Výška v režimu rotace 3
#define LCD_HEIGHT  240  // Šířka v režimu rotace 3
Adafruit_SharpMem lcd(PIN_SPI_CLK, PIN_SPI_MOSI, PIN_SPI_CS, LCD_WIDTH, LCD_HEIGHT);

// Uložené poslední hodnoty pro porovnání (prevence zbytečného přepisu)
float lastValues[6] = { -1, -1, -1, -1, -1, -1 };

void printParam(int index, int &y, const char* label, float value, const char* unit, int valuePrecision=0) {
  // Malý font na horním řádku (název + jednotka)
  lcd.setFont(&FreeMono12pt7b);
  lcd.setCursor(5, y);
  if (label != nullptr && strlen(label) > 0) {
    lcd.print(label);
  }

  // Jednotka vpravo na stejném řádku
  int16_t x1, y1;
  uint16_t w, h;
  lcd.getTextBounds(unit, 0, 0, &x1, &y1, &w, &h);
  int unitX = LCD_HEIGHT - w - 5;   // používáme LCD_HEIGHT jako šířku
  lcd.setCursor(unitX, y);
  lcd.print(unit);

  y += 40;  // posun dolů na hodnotu

  // Pokud se hodnota změnila, smažeme oblast a vykreslíme novou hodnotu
  if (lastValues[index] != value) {
    lcd.setFont(&FreeMonoBold24pt7b);

    // Připravíme stringy pro starou a novou hodnotu
    char oldValStr[20], newValStr[20];
    if (lastValues[index] != -1) {
      dtostrf(lastValues[index], 0, valuePrecision, oldValStr);
    } else {
      strcpy(oldValStr, ""); // pokud ještě nebyla hodnota, starý string je prázdný
    }
    dtostrf(value, 0, valuePrecision, newValStr);

    // Zjistíme bounding box staré hodnoty
    int16_t x1_old, y1_old;
    uint16_t w_old, h_old;
    lcd.getTextBounds(oldValStr, 0, 0, &x1_old, &y1_old, &w_old, &h_old);

    // Zjistíme bounding box nové hodnoty
    int16_t x1_new, y1_new;
    uint16_t w_new, h_new;
    lcd.getTextBounds(newValStr, 0, 0, &x1_new, &y1_new, &w_new, &h_new);

    // Vybereme větší šířku a výšku
    uint16_t w_clear = max(w_old, w_new);
    uint16_t h_clear = max(h_old, h_new);

    // Vypočítáme X podle větší šířky, aby bylo mazání zarovnané na střed
    int valX_clear = (LCD_HEIGHT - w_clear) / 2;

    // Smažeme oblast pro hodnotu
    int margin = 6;  // rezerva
    lcd.fillRect(valX_clear - margin, y - h_clear - margin, w_clear + 2*margin, h_clear + 2*margin, WHITE);

    // --- vykreslení nové hodnoty ---
    int16_t x1_draw, y1_draw;
    uint16_t w_draw, h_draw;
    lcd.getTextBounds(newValStr, 0, 0, &x1_draw, &y1_draw, &w_draw, &h_draw);

    int valX = (LCD_HEIGHT - w_draw) / 2;  // vystředění nové hodnoty
    lcd.setCursor(valX, y);
    lcd.print(newValStr);

    lastValues[index] = value;
  }

  y += 45;  // posun pro další parametr
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println(F("[BOOT] Startuji setup..."));

  Wire.begin(PIN_SDA, PIN_SCL);
  sen66Sensor.begin(Wire, SEN66_I2C_ADDR_6B);
  int error = sen66Sensor.deviceReset();
  if (error != NO_ERROR)
  {
    Serial.print(F("[ERROR] Nemohu číst SEN66. Kód chyby: "));
    Serial.println(error);
  }
  error = sen66Sensor.startContinuousMeasurement();
  if (error != NO_ERROR)
  {
    Serial.print(F("[ERROR] Nemohu číst SEN66. Kód chyby: "));
    Serial.println(error);
  }

  lcd.begin();
  lcd.setRotation(3);
  lcd.clearDisplay();
  lcd.setTextColor(BLACK);

  rgbLed.begin();
  rgbLed.setBrightness(16);
  rgbLed.show();

  lcd.clearDisplay();
  lcd.setFont(&FreeMono12pt7b);
  lcd.setCursor(5, 30);
  lcd.println("LaskaKit ESPwled");
  lcd.setCursor(5, 50);
  lcd.println("SEN66 + RGB LED + MemoryLCD");
  lcd.refresh();
  delay(3000);
  lcd.clearDisplay();

  Serial.println(F("[BOOT] Setup dokončen.\n"));
}

void loop() {
  Serial.println(F("\n[LOOP] Nový cyklus - čtení SEN66..."));

  float massConcentrationPm1p0, massConcentrationPm2p5, massConcentrationPm4p0,
        massConcentrationPm10p0, humidity, temperature, vocIndex, noxIndex;
  uint16_t co2;
  int error = sen66Sensor.readMeasuredValues(
        massConcentrationPm1p0, massConcentrationPm2p5, massConcentrationPm4p0,
        massConcentrationPm10p0, humidity, temperature, vocIndex, noxIndex, co2);

  if (error == 0) {
    int y = 15;
    printParam(0, y, "CO2", (float)co2, "ppm", 0);
    printParam(1, y, "Teplota", temperature, "degC", 1);
    printParam(2, y, "Vlhkost", humidity, "%Rh", 1);
    printParam(3, y, "VOC", vocIndex, "", 0);
    printParam(4, y, "NOx", noxIndex, "", 0);
    printParam(5, y, "PM2.5", massConcentrationPm2p5, "µg/m3", 0);

    Serial.print(F("[DATA] CO2: "));      Serial.print(co2);       Serial.print(F(" ppm, "));
    Serial.print(F("Teplota: "));         Serial.print(temperature, 1); Serial.print(F(" degC, "));
    Serial.print(F("RH: "));              Serial.print(humidity, 1);    Serial.print(F(" %, "));
    Serial.print(F("VOC: "));             Serial.print((int)vocIndex);    Serial.print(F(" , "));
    Serial.print(F("NOx: "));             Serial.print((int)noxIndex);    Serial.print(F(" , "));
    Serial.print(F("PM2.5: "));           Serial.print((int)massConcentrationPm2p5); Serial.println(F(" ug/m3"));

    if (co2 < 800) {
      rgbLed.setPixelColor(0, rgbLed.Color(0,255,0));
      Serial.println(F("[RGB] CO2 v normě (zelená)"));
    } else if (co2 < 1400) {
      rgbLed.setPixelColor(0, rgbLed.Color(255,255,0));
      Serial.println(F("[RGB] Zvýšené CO2 (žlutá)"));
    } else {
      rgbLed.setPixelColor(0, rgbLed.Color(255,0,0));
      Serial.println(F("[RGB] Vysoké CO2 (červená)"));
    }
  } else {
    lcd.setCursor(5, 40);
    lcd.print("Chyba komunikace SEN66!");
    rgbLed.setPixelColor(0, rgbLed.Color(0,0,255));
    Serial.print(F("[ERROR] Nemohu číst SEN66. Kód chyby: "));
    Serial.println(error);
    Serial.println(F("[RGB] Chyba senzoru (modrá)"));
  }

  rgbLed.show();
  lcd.refresh();

  delay(1000);
}
