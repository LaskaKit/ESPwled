# ESPWLED Color Game

Webová arkádová hra pro **ESPWLED s ESP32-C3** a **WS2812B LED pásek**, ovládaná přes webový prohlížeč.
Herní logika běží přímo na ESP32-C3, bez cloudu a bez externích služeb.

LED pásek slouží jako herní pole, po kterém se pohybují barevné střely hráče a nepřítele.
Hráč reaguje výběrem správné barvy pomocí webového rozhraní.

---

## Hlavní vlastnosti

- Arkádová hra běžící přímo na ESPWLED
- Ovládání přes webové rozhraní (PC / mobil)
- LED pásek WS2812B jako herní plocha
- Wi-Fi Access Point + volitelný STA režim
- Ukládání nastavení do NVM (Preferences)
- Nastavitelná obtížnost (počet barev)
- Nastavitelný interval nepřátelské střelby
- Nastavitelná rychlost pohybu střel (10–200 ms)
- Barevné kolize a vizuální exploze
- Offline provoz, žádný cloud

---

## Použitý hardware

- ESPWLED s ESP32-C3 (https://www.laskakit.cz/laskakit-espwled/?variantId=16928)
- WS2812B LED pásek (například https://www.laskakit.cz/led-pasek-neopixel-ws2812b-60led-m-ip30-5m-bily/
- Počet LED: 120 (pro pásek 60 LED/m je potřebná délka 2m)
- Datový pin LED: GPIO 5 (označený na desce jako DAT)

---

## Zapojení LED pásku

| WS2812B | ESPWLED |
|-------|----------|
| DIN   | GPIO 5 (DAT)   |
| +5V   | 5V (PWR) |
| GND   | GND      |

Doporučení:
- použijte samostatný 5V zdroj pro LED pásek nebo jej napájejte ESPWLED z USB-C
- nezapomeňte na společnou zem (GND)

---

## Wi-Fi režimy

### Access Point (AP)

ESP32 vždy spustí vlastní Wi-Fi síť:

- SSID: `ESPWLED-Color game`
- Heslo: `espwledgame`
- IP adresa: `192.168.4.1` (typicky)

### Station (STA)

- Připojení k existující Wi-Fi
- Nastavuje se přes webové rozhraní
- Údaje se ukládají do NVM
- Po restartu se ESP automaticky připojí

- SSID a IP adresou jsou zobrazeny v Serial Monitor

---

## Webové rozhraní

Po připojení k ESP otevřete IP adresu v prohlížeči.

### Funkce webu

#### Wi-Fi
- Zadání SSID a hesla
- Uložení do NVM

#### Nastavení hry
- Obtížnost (3 / 4 / 6 / 8 barev)
- Minimální interval střel nepřítele (s)
- Maximální interval střel nepřítele (s)
- Rychlost pohybu střel (10–200 ms)

#### Ovládání hry
- Spuštění hry
- Barevná tlačítka pro střelbu
- Restart hry

---

## Herní princip

- Hráčova základna je na začátku LED pásku
- Nepřítel je na konci LED pásku
- Obě strany vystřelují barevné střely
- Střely se pohybují po LED pásku
- Pokud se střely stejné barvy setkají → exploze
- Pokud nepřátelská střela dosáhne hráčovy základny → konec hry
- Skóre se zvyšuje za každý úspěšný zásah

---

## Ukládaná nastavení (Preferences)

Do NVM se ukládá:

- STA Wi-Fi SSID
- STA Wi-Fi heslo
- Rychlost pohybu střel (shotSpeedMs)

Po restartu zařízení se tato nastavení automaticky načtou.

---

## Použité knihovny

- WiFi
- Preferences
- Adafruit NeoPixel
- ESPAsyncWebServer
- AsyncTCP

---

## Poznámky k výkonu

- Pohyb střel je řízen pomocí `millis()`
- Rychlost střel je globální pro hráče i nepřítele
- LED rendering probíhá v každé iteraci `loop()`
- Exploze jsou časově řízené s plynulým zhasínáním

---

## Licence

Creative Commons BY-NC 4.0

---

Laskakit (2026)

ESPWLED - https://www.laskakit.cz/laskakit-espwled/?variantId=16928

LED pásek - https://www.laskakit.cz/led-pasek-neopixel-ws2812b-60led-m-ip30-5m-bily
