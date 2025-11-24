# Uteč! – ESPWLED hra s adresovatelným LED páskem

**Uteč!** je interaktivní hra pro [ESPWLED (ESP32-C3)](https://www.laskakit.cz/laskakit-espwled/?variantId=16925), kde hráč závodí s duchem po LED polích. Cílem je dosáhnout koncového pole dříve než duch. Hráč postupuje odpovídáním na otázky, duch postupuje při chybné odpovědi.

---

## Hardware

- **Mikrokontrolér:** ESP32-C3, využijte desku [ESPWLED](https://www.laskakit.cz/laskakit-espwled/?variantId=16925)
- **Redukce pro LED pásek:** [LaskaKit ESPwled adaptér pro LED pásek JST-SM-3](https://www.laskakit.cz/laskakit-espwled-adapter-pro-led-pasek-jst-sm-3--10-cm/), [4pinový konektor pro LED pásek](https://www.laskakit.cz/jst-xh-2-54mm-konektor-do-dps--pravouhly/?variantId=5393)
- **LED pás:** Adresovatelný RGB LED pás (např. WS2812 / NeoPixel), například [Led pásek Neopixel WS2812B 60led/m](https://www.laskakit.cz/led-pasek-neopixel-ws2812b-60led-m-ip30-5m-cerny/)
  - Pin: GPIO5
  - Počet LED: 120 (8 LED na pole), pásek ve verzi 60LED/m zkraťte na 2m
  - LED pásek i ESPWLED se bude napájet z nabíječky s USB-C konektorem. Nabíjecí adaptér musí být minimálně 5V/3A
     
---

## Software & Knihovny

- **Arduino framework pro ESP32-C3**
- **Knihovny:**
  - [Adafruit_NeoPixel](https://github.com/adafruit/Adafruit_NeoPixel) – ovládání LED pásu
  - [AsyncTCP](https://github.com/me-no-dev/AsyncTCP) – asynchronní TCP
  - [ESPAsyncWebServer](https://github.com/me-no-dev/ESPAsyncWebServer) – web server
  - Preferences – ukládání Wi-Fi přihlašovacích údajů

---

## Princip hry

1. Hráč začíná na poli 3, duch na poli 1.
2. Správná odpověď → hráč postupuje o 1 pole.
3. Špatná odpověď → duch postupuje o 1–3 pole podle kola.
4. Možnost **"Ještě jednou"** max 2x, pro přeskočení otázky bez pohybu ducha.
5. LED pás zobrazuje:
   - Zelené LED = hráč
   - Červené LED = duch
   - Ostatní pole = dimované LED
6. Konec hry:
   - Hráč dosáhne posledního pole → výhra hráče
   - Duch dohoní hráče → výhra ducha

---

## Formát otázek

Otázky se nahrávají přes webové rozhraní v tomto formátu (7 polí, oddělených středníkem):
	`ID;Otázka;Odpověď1;Odpověď2;Odpověď3;Odpověď4;IndexSprávnéOdpovědi(1..4)`

### Prompt pro generování otázek a odpovědí
Jsi AI generátor kvízových otázek. Vytvoř prosím otázky ve formátu vhodném pro hru **"Uteč!"** na ESP32-C3.</br>
Otázky musí být zaměřeny na (**zde zadejte téma otázek**). </br>
*Např. Matematiku Základní školy pro první stupeň, Fyziku pro střední školu, Všeobecné znalosti o Evropské Unii a jiné*</br>
Každá otázka musí mít přesně 7 částí oddělených středníkem (`;`):</br>

ID otázky;Otázka;Odpověď1;Odpověď2;Odpověď3;Odpověď4;Index Správné odpovědi (od 1 do 4)

Pravidla

1. **ID** musí být číslo větší než 0, unikátní pro každou otázku.  
2. **Otázka** musí být jasná, krátká a srozumitelná.  
3. Každá otázka musí mít **4 možné odpovědi**.  
4. **Správná odpověď** je číslo od 1 do 4 podle pořadí odpovědí.  
5. **Nepoužívej čárky** mezi odpověďmi, protože pole se oddělují středníkem.  
6. Vytvoř **30 otázek** v tomto formátu.  
7. Výstup má být **čistě textový**, každá otázka na novém řádku, bez úvodních komentářů.

Příklad</br>
1;Kolik je 2+2?;3;4;5;6;2</br>
2;Hlavní město Francie?;Paříž;Londýn;Berlín;Řím;1

---

## Instalace

1. Nainstalujte Arduino IDE s podporou ESP32-C3.
2. Nainstalujte knihovny:
   - Adafruit_NeoPixel
   - AsyncTCP
   - ESPAsyncWebServer
3. Povolte USB CDC On Boot (menu - Tools - nastavit na Enabled)
4. Nahrajte `hra_Utec.ino` do ESPWLED.
   - Pokud nelze program do ESPWLED nahrát, přepněte jej manuálně do bootloader módu.
   - Připojte nabíjecí adaptér do USB-C konektoru.
   - Stiskněte a držte tlačítko FLASH
   - Stiskněte a uvolněte tlačítko RESET
   - Uvolněte tlačítko RESET
   - Nahrajte program do ESPWLED
5. Po spuštění:
   - ESPWLED (ESP32-C3) vytvoří Wi-Fi Access Point `hraUtecAP` (heslo `utec1234`).
   - Připojte se přes prohlížeč na `http://192.168.4.1`.
   - Nahrajte otázky a spusťte hru.

> Pokud uložíte Wi-Fi přihlašovací údaje, ESP32-C3 (ESPWLED) se automaticky pokusí připojit k domácí síti a AP zůstane aktivní jako fallback.

---

## Webové rozhraní

- Zobrazení aktuální otázky a odpovědí
- Odeslání odpovědi
- Přeskočení otázky (`Ještě jednou`)
- Restart hry
- Nahrání nových otázek
- Nastavení Wi-Fi

LED pás zobrazuje dynamicky pozice hráče a ducha.

<img src="https://github.com/LaskaKit/ESPwled/blob/main/SW/hra_Utec/Utec_1.jpeg" width="50%" alt="Webová stránka hry">
<img src="https://github.com/LaskaKit/ESPwled/blob/main/SW/hra_Utec/Utec_2.jpeg" width="50%" alt="Webová stránka hry">

---

## Ukázka LED zobrazení

- Zelené = hráč  
- Červené = duch  
- Modré (dimované) = pole

---

## Licence

Projekt je open-source a můžete jej používat a upravovat podle potřeby. 

laskakit.cz 2025

---

*Vytvořeno pro zábavu a vzdělávací účely – hra kombinuje ESPWLED, NeoPixel a webové rozhraní.*
