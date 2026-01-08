/*
 * ESPWLED Color Game
 *
 * Webová arkádová hra pro ESP32-C3 a WS2812B LED pásek.
 * LED pásek slouží jako herní plocha, po které se pohybují
 * barevné střely hráče a nepřítele.
 *
 * Ovládání a nastavení probíhá přes webové rozhraní:
 * - nastavení Wi-Fi (AP + volitelný STA režim)
 * - volba obtížnosti (počet barev)
 * - interval nepřátelské střelby
 * - rychlost pohybu střel (10–200 ms, uloženo v NVM)
 *
 * Wi-Fi Access Point (AP):
 *   SSID: ESPWLED-Color game
 *   Heslo: espwledgame
 *
 * Hra běží plně offline, bez cloudu.
 * Použité knihovny: WiFi, Preferences, ESPAsyncWebServer,
 * AsyncTCP, Adafruit NeoPixel.
 *
 * laskakit.cz (2026)
 */

#include <WiFi.h>
#include <Preferences.h>
#include <Adafruit_NeoPixel.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

// ====================== Konfigurace ======================

#define LED_PIN        5
#define LED_COUNT      120

// Základny
#define PLAYER_BASE_START   0
#define PLAYER_BASE_END     3
#define ENEMY_BASE_START    115
#define ENEMY_BASE_END      119

// AP nastavení (pevné)
const char* AP_SSID     = "ESPWLED-Color game";
const char* AP_PASSWORD = "espwledgame";

// Preferences namespace a klíče
Preferences preferences;
const char* PREF_NAMESPACE = "wifi";
const char* PREF_SSID_KEY  = "sta_ssid";
const char* PREF_PSK_KEY   = "sta_psk";
const char* PREF_SPEED_KEY = "shot_speed";  // NOVÉ: rychlost střel

// Web server
AsyncWebServer server(80);

// NeoPixel
Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

// ====================== Herní stav ======================

enum GameState {
  GAME_IDLE,
  GAME_RUNNING,
  GAME_OVER
};

GameState gameState = GAME_IDLE;

// Obtížnost = počet barev z palety
uint8_t difficultyColors = 3;  // 3,4,6,8

// Interval nepřátelské střelby (ms)
uint32_t enemyShotMinMs = 1000;
uint32_t enemyShotMaxMs = 5000;

// Rychlost pohybu střel (ms) - NOVÉ: nastavitelné
uint16_t shotSpeedMs = 50;  // Výchozí 50ms, rozsah 10-200

// Čas pro další nepřátelskou střelu
uint32_t nextEnemyShotMillis = 0;

// Paleta barev (RGB)
struct Color {
  uint8_t r, g, b;
};

const Color COLOR_PALETTE[8] = {
  {255,   0,   0},   // červená
  {0,   255,   0},   // zelená
  {0,     0, 255},   // modrá
  {255, 255,   0},   // žlutá
  {0,   255, 255},   // azurová
  {255,   0, 255},   // fialová
  {255, 128,   0},   // oranžová
  {255, 255, 255}    // bílá
};

// Střely - rychlost je nyní globální shotSpeedMs
struct Projectile {
  bool    active;
  int16_t position;      // index LED (0..119)
  int8_t  direction;     // +1 hráč -> nepřítel, -1 nepřítel -> hráč
  uint8_t colorIndex;    // index v COLOR_PALETTE
  uint32_t lastMoveMs;   // poslední posun
};

const uint8_t MAX_PROJECTILES = 16;
Projectile projectiles[MAX_PROJECTILES];

// Exploze
struct Explosion {
  bool active;
  int16_t center;
  uint8_t colorIndex;
  uint32_t startMs;
  uint32_t durationMs;
};

const uint8_t MAX_EXPLOSIONS = 8;
Explosion explosions[MAX_EXPLOSIONS];

// Skóre
uint32_t score = 0;

// Pulzování základen
uint32_t lastBaseUpdateMs = 0;
uint16_t basePulsePhase = 0;

// ====================== Deklarace funkcí ======================

void setupWiFi();
void startAP();
void connectSTAIfConfigured();
void loadWiFiCredentials(String &ssid, String &psk);
void loadShotSpeed();
void saveShotSpeed(uint16_t speed);

void setupWebServer();
String htmlPage();

void resetGame();
void startGame();
void updateGame();
void updateProjectiles();
void spawnEnemyProjectile();
void spawnPlayerProjectile(uint8_t colorIndex);
void handleCollisions();
void addExplosion(int16_t pos, uint8_t colorIndex);
void updateExplosions();
void renderStrip();
void renderBases();
uint32_t colorFromPalette(uint8_t idx, uint8_t brightness);
uint32_t scaleColor(uint32_t c, uint8_t scale);

// ====================== Setup & Loop ======================

void setup() {
  Serial.begin(115200);
  delay(200);

  // NeoPixel init
  strip.begin();
  strip.show();
  strip.setBrightness(255);

  // Preferences init
  preferences.begin(PREF_NAMESPACE, false);

  // Načti rychlost střel z NVM
  loadShotSpeed();

  // WiFi
  setupWiFi();

  // Web server
  setupWebServer();

  // Herní inicializace
  resetGame();

  Serial.print("Nastavení dokončeno. Rychlost střely: ");
  Serial.print(shotSpeedMs);
  Serial.println(" ms");
}

void loop() {
  uint32_t now = millis();

  // Herní logika
  if (gameState == GAME_RUNNING) {
    updateGame();
  }

  // Základny a exploze (i když hra neběží)
  if (now - lastBaseUpdateMs >= 20) {
    lastBaseUpdateMs = now;
    basePulsePhase += 5;
  }

  updateExplosions();
  renderStrip();
}

// ====================== WiFi ======================

void setupWiFi() {
  WiFi.mode(WIFI_AP_STA);

  startAP();
  connectSTAIfConfigured();
}

void startAP() {
  bool apResult = WiFi.softAP(AP_SSID, AP_PASSWORD);
  if (apResult) {
    Serial.println("AP start OK");
  } else {
    Serial.println("AP start FAILED");
  }

  IPAddress apIP = WiFi.softAPIP();
  Serial.print("AP SSID: ");
  Serial.println(AP_SSID);
  Serial.print("AP Password: ");
  Serial.println(AP_PASSWORD);
  Serial.print("AP IP: ");
  Serial.println(apIP);
}

void connectSTAIfConfigured() {
  String ssid, psk;
  loadWiFiCredentials(ssid, psk);

  if (ssid.length() == 0 || psk.length() == 0) {
    Serial.println("No STA credentials stored, skipping STA connect.");
    return;
  }

  Serial.print("Connecting STA to SSID: ");
  Serial.println(ssid);

  WiFi.begin(ssid.c_str(), psk.c_str());

  unsigned long startAttempt = millis();
  const unsigned long timeoutMs = 15000;

  while (WiFi.status() != WL_CONNECTED && (millis() - startAttempt) < timeoutMs) {
    delay(250);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("STA připojeno.");
    Serial.print("STA SSID: ");
    Serial.println(ssid);
    Serial.print("STA IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("STA připojení selhalo (zkontrolujte údaje k Wi-Fi).");
  }
}

void loadWiFiCredentials(String &ssid, String &psk) {
  ssid = preferences.getString(PREF_SSID_KEY, "");
  psk  = preferences.getString(PREF_PSK_KEY, "");
}

void saveWiFiCredentials(const String &ssid, const String &psk) {
  if (ssid.length() == 0 && psk.length() == 0) {
    Serial.println("Empty WiFi fields, not saving.");
    return;
  }

  if (ssid.length() > 0) {
    preferences.putString(PREF_SSID_KEY, ssid);
    Serial.print("Saved STA SSID: ");
    Serial.println(ssid);
  }

  if (psk.length() > 0) {
    preferences.putString(PREF_PSK_KEY, psk);
    Serial.println("Saved STA password.");
  }

  connectSTAIfConfigured();
}

// NOVÉ: Načtení/Uložení rychlosti střel
void loadShotSpeed() {
  shotSpeedMs = preferences.getUShort(PREF_SPEED_KEY, 50);
  if (shotSpeedMs < 10) shotSpeedMs = 50;
  if (shotSpeedMs > 200) shotSpeedMs = 50;
  Serial.print("Načtená rychlost střely: ");
  Serial.print(shotSpeedMs);
  Serial.println(" ms");
}

void saveShotSpeed(uint16_t speed) {
  if (speed < 10) speed = 10;
  if (speed > 200) speed = 200;
  shotSpeedMs = speed;
  preferences.putUShort(PREF_SPEED_KEY, shotSpeedMs);
  Serial.print("Uložena nová rychlost střely: ");
  Serial.print(shotSpeedMs);
  Serial.println(" ms");
}

// ====================== Web server ======================

void setupWebServer() {
  // Úvodní stránka
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html; charset=utf-8", htmlPage());
  });

  // Uložení WiFi
  server.on("/saveWifi", HTTP_POST, [](AsyncWebServerRequest *request) {
    String ssid = "";
    String psk  = "";

    if (request->hasParam("ssid", true)) {
      ssid = request->getParam("ssid", true)->value();
    }
    if (request->hasParam("pass", true)) {
      psk = request->getParam("pass", true)->value();
    }

    saveWiFiCredentials(ssid, psk);

    request->send(200, "application/json; charset=utf-8", "{\"status\":\"ok\"}");
  });

  // NOVÉ: Uložení rychlosti střel
  server.on("/saveShotSpeed", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("speed", true)) {
      uint16_t speed = request->getParam("speed", true)->value().toInt();
      saveShotSpeed(speed);
      request->send(200, "application/json; charset=utf-8", "{\"status\":\"ok\",\"speed\":" + String(shotSpeedMs) + "}");
    } else {
      request->send(400, "application/json; charset=utf-8", "{\"status\":\"error\"}");
    }
  });

  // Spuštění hry
  server.on("/startGame", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("difficulty", true)) {
      int diff = request->getParam("difficulty", true)->value().toInt();
      if (diff == 3 || diff == 4 || diff == 6 || diff == 8) {
        difficultyColors = diff;
      }
    }

    if (request->hasParam("enemyMin", true) && request->hasParam("enemyMax", true)) {
      uint32_t minMs = request->getParam("enemyMin", true)->value().toInt() * 1000UL;
      uint32_t maxMs = request->getParam("enemyMax", true)->value().toInt() * 1000UL;
      if (maxMs <= minMs) {
        maxMs = minMs + 1000;
      }
      enemyShotMinMs = minMs;
      enemyShotMaxMs = maxMs;
    }

    resetGame();
    startGame();

    request->send(200, "application/json; charset=utf-8", "{\"status\":\"game_started\"}");
  });

  // Střela hráče
  server.on("/shoot", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("color", true)) {
      int idx = request->getParam("color", true)->value().toInt();
      if (idx >= 0 && idx < difficultyColors) {
        spawnPlayerProjectile((uint8_t)idx);
      }
    }
    request->send(200, "application/json; charset=utf-8", "{\"status\":\"shot\"}");
  });

  // Restart hry
  server.on("/restartGame", HTTP_POST, [](AsyncWebServerRequest *request) {
    resetGame();
    request->send(200, "application/json; charset=utf-8", "{\"status\":\"restarted\"}");
  });

  server.begin();
}

// HTML UI - přidáno pole pro rychlost střel
String htmlPage() {
  String html = 
"<!DOCTYPE html>\n"
"<html lang=\"cs\">\n"
"<head>\n"
"<meta charset=\"UTF-8\">\n"
"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">\n"
"<title>ESPWLED Color game</title>\n"
"<style>\n"
"body{font-family:system-ui,Arial,sans-serif;margin:0;padding:0;background:#111;color:#eee;}\n"
".container{max-width:480px;margin:0 auto;padding:16px;}\n"
"h2{margin-top:24px;margin-bottom:8px;font-size:1.2rem;}\n"
"label{display:block;margin-bottom:4px;font-size:0.9rem;}\n"
"input,select,button{width:100%;padding:8px 10px;margin-bottom:10px;border-radius:6px;border:1px solid #444;background:#222;color:#eee;font-size:0.95rem;box-sizing:border-box;}\n"
"button{border:none;background:#007bff;color:#fff;font-weight:600;cursor:pointer;transition:background 0.2s;}\n"
"button:active{background:#0056b3;}\n"
".row{display:flex;gap:8px;}\n"
".row>div{flex:1;}\n"
".btn-color{flex:1;padding:12px 0;margin:4px;border-radius:8px;border:none;font-weight:600;color:#000;font-size:0.9rem;}\n"
"#status{font-size:0.85rem;margin-top:4px;min-height:1.2em;}\n"
".section{border:1px solid #333;border-radius:10px;padding:12px;margin-top:12px;background:#181818;}\n"
"</style>\n"
"</head>\n"
"<body>\n"
"<div class=\"container\">\n"

"<h1 style=\"font-size:1.4rem;margin:10px 0;\">ESPWLED Color game</h1>\n"

"<div class=\"section\" id=\"wifi-section\">\n"
"<h2>WiFi</h2>\n"
"<label for=\"ssid\">SSID</label>\n"
"<input id=\"ssid\" type=\"text\" placeholder=\"Název WiFi\">\n"
"<label for=\"pass\">Heslo</label>\n"
"<input id=\"pass\" type=\"password\" placeholder=\"Heslo WiFi\">\n"
"<button id=\"saveWifiBtn\">Uložit WiFi</button>\n"
"<div id=\"wifiStatus\"></div>\n"
"</div>\n"

"<div class=\"section\" id=\"game-settings\">\n"
"<h2>Nastavení hry</h2>\n"
"<label for=\"difficulty\">Obtížnost (počet barev)</label>\n"
"<select id=\"difficulty\">\n"
"<option value=\"3\">3 barvy</option>\n"
"<option value=\"4\">4 barvy</option>\n"
"<option value=\"6\">6 barev</option>\n"
"<option value=\"8\">8 barev</option>\n"
"</select>\n"
"<div class=\"row\">\n"
"<div>\n"
"<label for=\"enemyMin\">Min. interval střel (s)</label>\n"
"<input id=\"enemyMin\" type=\"number\" min=\"1\" max=\"5\" value=\"1\">\n"
"</div>\n"
"<div>\n"
"<label for=\"enemyMax\">Max. interval střel (s)</label>\n"
"<input id=\"enemyMax\" type=\"number\" min=\"1\" max=\"5\" value=\"5\">\n"
"</div>\n"
"</div>\n"
"<!-- NOVÉ: Rychlost pohybu střel -->\n"
"<label for=\"shotSpeed\">Rychlost střel (ms)</label>\n"
"<input id=\"shotSpeed\" type=\"range\" min=\"10\" max=\"200\" value=\"50\">\n"
"<div style=\"display:flex;justify-content:space-between;font-size:0.85rem;\">\n"
"<span>10ms (rychlá)</span><span id=\"speedValue\">50</span><span>200ms (pomalá)</span>\n"
"</div>\n"
"<button id=\"saveSpeedBtn\">Uložit rychlost</button>\n"
"<button id=\"startGameBtn\">Spustit hru</button>\n"
"<div id=\"gameStatus\"></div>\n"
"</div>\n"

"<div class=\"section\" id=\"game-section\">\n"
"<h2>Hra</h2>\n"
"<div id=\"colorButtons\" style=\"display:flex;flex-wrap:wrap;gap:4px;\"></div>\n"
"<button id=\"restartBtn\" style=\"margin-top:8px;background:#dc3545;\">Restart hry</button>\n"
"<div id=\"status\"></div>\n"
"</div>\n"

"</div>\n"

"<script>\n"
"const colorPalette = [\n"
" {name:'Červená', rgb:'rgb(255,0,0)'},\n"
" {name:'Zelená',  rgb:'rgb(0,255,0)'},\n"
" {name:'Modrá',   rgb:'rgb(0,0,255)'},\n"
" {name:'Žlutá',   rgb:'rgb(255,255,0)'},\n"
" {name:'Azurová', rgb:'rgb(0,255,255)'},\n"
" {name:'Fialová', rgb:'rgb(255,0,255)'},\n"
" {name:'Oranžová',rgb:'rgb(255,128,0)'},\n"
" {name:'Bílá',    rgb:'rgb(255,255,255)'}\n"
"];\n"
"\n"
"function buildColorButtons(count){\n"
"  const wrap = document.getElementById('colorButtons');\n"
"  wrap.innerHTML='';\n"
"  for(let i=0;i<count;i++){\n"
"    const btn=document.createElement('button');\n"
"    btn.className='btn-color';\n"
"    btn.textContent=colorPalette[i].name;\n"
"    btn.style.background=colorPalette[i].rgb;\n"
"    btn.dataset.idx=i;\n"
"    btn.addEventListener('click',()=>{\n"
"      fetch('/shoot',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'color='+encodeURIComponent(i)})\n"
"        .then(()=>{document.getElementById('status').textContent='Střela: '+colorPalette[i].name;})\n"
"        .catch(()=>{document.getElementById('status').textContent='Chyba při střelbě';});\n"
"    });\n"
"    wrap.appendChild(btn);\n"
"  }\n"
"}\n"
"\n"
"document.getElementById('difficulty').addEventListener('change',e=>{\n"
"  const diff=parseInt(e.target.value,10);\n"
"  buildColorButtons(diff);\n"
"});\n"
"\n"
"buildColorButtons(parseInt(document.getElementById('difficulty').value,10));\n"
"\n"
"// NOVÉ: Slider rychlosti\n"
"const speedSlider = document.getElementById('shotSpeed');\n"
"const speedValue = document.getElementById('speedValue');\n"
"speedSlider.addEventListener('input',e=>{\n"
"  speedValue.textContent = e.target.value;\n"
"});\n"
"\n"
"document.getElementById('saveSpeedBtn').addEventListener('click',()=>{\n"
"  const speed = speedSlider.value;\n"
"  fetch('/saveShotSpeed',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'speed='+speed})\n"
"    .then(r=>r.json()).then(data=>{\n"
"      document.getElementById('gameStatus').textContent='Rychlost uložena: '+data.speed+' ms';\n"
"    }).catch(()=>{\n"
"      document.getElementById('gameStatus').textContent='Chyba při ukládání rychlosti.';\n"
"    });\n"
"});\n"
"\n"
"document.getElementById('saveWifiBtn').addEventListener('click',()=>{\n"
"  const ssid=document.getElementById('ssid').value.trim();\n"
"  const pass=document.getElementById('pass').value.trim();\n"
"  const body='ssid='+encodeURIComponent(ssid)+'&pass='+encodeURIComponent(pass);\n"
"  fetch('/saveWifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body})\n"
"    .then(r=>r.json()).then(()=>{\n"
"      document.getElementById('wifiStatus').textContent='WiFi uložena (pokud nebyla pole prázdná).';\n"
"    }).catch(()=>{\n"
"      document.getElementById('wifiStatus').textContent='Chyba při ukládání WiFi.';\n"
"    });\n"
"});\n"
"\n"
"document.getElementById('startGameBtn').addEventListener('click',()=>{\n"
"  const diff=document.getElementById('difficulty').value;\n"
"  let min=parseInt(document.getElementById('enemyMin').value,10)||1;\n"
"  let max=parseInt(document.getElementById('enemyMax').value,10)||5;\n"
"  if(max<=min){max=min+1;}\n"
"  if(min<1)min=1;if(max>5)max=5;\n"
"  const body='difficulty='+encodeURIComponent(diff)+'&enemyMin='+encodeURIComponent(min)+'&enemyMax='+encodeURIComponent(max);\n"
"  fetch('/startGame',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body})\n"
"    .then(r=>r.json()).then(()=>{\n"
"      document.getElementById('gameStatus').textContent='Hra spuštěna.';\n"
"      document.getElementById('status').textContent='';\n"
"    }).catch(()=>{\n"
"      document.getElementById('gameStatus').textContent='Chyba při spuštění hry.';\n"
"    });\n"
"});\n"
"\n"
"document.getElementById('restartBtn').addEventListener('click',()=>{\n"
"  fetch('/restartGame',{method:'POST'}).then(()=>{\n"
"    document.getElementById('status').textContent='Hra restartována.';\n"
"  }).catch(()=>{\n"
"    document.getElementById('status').textContent='Chyba při restartu.';\n"
"  });\n"
"});\n"
"</script>\n"

"</body>\n"
"</html>\n";

  return html;
}

// ====================== Herní logika ======================

void resetGame() {
  gameState = GAME_IDLE;
  score = 0;

  for (uint8_t i = 0; i < MAX_PROJECTILES; i++) {
    projectiles[i].active = false;
  }
  for (uint8_t i = 0; i < MAX_EXPLOSIONS; i++) {
    explosions[i].active = false;
  }

  nextEnemyShotMillis = millis() + random(enemyShotMinMs, enemyShotMaxMs + 1);

  Serial.println("Reset hry.");
}

void startGame() {
  if (gameState == GAME_RUNNING) return;

  gameState = GAME_RUNNING;
  nextEnemyShotMillis = millis() + random(enemyShotMinMs, enemyShotMaxMs + 1);
  Serial.println("Hra odstartována.");
}

void updateGame() {
  uint32_t now = millis();

  // Nepřátelské střelby
  if (now >= nextEnemyShotMillis) {
    spawnEnemyProjectile();
    uint32_t interval = random(enemyShotMinMs, enemyShotMaxMs + 1);
    nextEnemyShotMillis = now + interval;
  }

  updateProjectiles();
  handleCollisions();
}

void updateProjectiles() {
  uint32_t now = millis();

  for (uint8_t i = 0; i < MAX_PROJECTILES; i++) {
    if (!projectiles[i].active) continue;

    if (now - projectiles[i].lastMoveMs >= (uint32_t)shotSpeedMs) {  // Používá globální rychlost
      projectiles[i].lastMoveMs = now;
      projectiles[i].position += projectiles[i].direction;

      // Kontrola konců
      if (projectiles[i].direction > 0) {
        // Hráč -> nepřítel
        if (projectiles[i].position > ENEMY_BASE_END) {
          projectiles[i].active = false;
        }
      } else {
        // Nepřítel -> hráč
        if (projectiles[i].position < PLAYER_BASE_START) {
          projectiles[i].active = false;
        }
      }
    }
  }

  // Kontrola dosažení hráčovy základny nepřátelskou střelou
  if (gameState == GAME_RUNNING) {
    for (uint8_t i = 0; i < MAX_PROJECTILES; i++) {
      if (!projectiles[i].active) continue;
      if (projectiles[i].direction < 0 && projectiles[i].position <= PLAYER_BASE_END) {
        // Game over
        gameState = GAME_OVER;
        Serial.println("Konec hry (protihráč zničil základnu).");
        break;
      }
    }
  }
}

void spawnEnemyProjectile() {
  for (uint8_t i = 0; i < MAX_PROJECTILES; i++) {
    if (!projectiles[i].active) {
      projectiles[i].active = true;
      projectiles[i].direction = -1;
      projectiles[i].position = ENEMY_BASE_END;
      projectiles[i].colorIndex = random(0, difficultyColors);
      projectiles[i].lastMoveMs = millis();
      return;
    }
  }
}

void spawnPlayerProjectile(uint8_t colorIndex) {
  if (gameState != GAME_RUNNING) {
    return;
  }

  for (uint8_t i = 0; i < MAX_PROJECTILES; i++) {
    if (!projectiles[i].active) {
      projectiles[i].active = true;
      projectiles[i].direction = +1;
      projectiles[i].position = PLAYER_BASE_START;
      projectiles[i].colorIndex = colorIndex;
      projectiles[i].lastMoveMs = millis();
      return;
    }
  }
}

void handleCollisions() {
  for (uint8_t i = 0; i < MAX_PROJECTILES; i++) {
    if (!projectiles[i].active || projectiles[i].direction <= 0) continue; // hráč
    for (uint8_t j = 0; j < MAX_PROJECTILES; j++) {
      if (!projectiles[j].active || projectiles[j].direction >= 0) continue; // nepřítel
      if (projectiles[i].colorIndex != projectiles[j].colorIndex) continue;

      int16_t posPlayer = projectiles[i].position;
      int16_t posEnemy  = projectiles[j].position;

      if (abs(posPlayer - posEnemy) <= 1) {
        int16_t explosionPos = (posPlayer + posEnemy) / 2;
        addExplosion(explosionPos, projectiles[i].colorIndex);
        projectiles[i].active = false;
        projectiles[j].active = false;
        score++;
        Serial.print("Zásah! Skóre = ");
        Serial.println(score);
      }
    }
  }
}

void addExplosion(int16_t pos, uint8_t colorIndex) {
  for (uint8_t i = 0; i < MAX_EXPLOSIONS; i++) {
    if (!explosions[i].active) {
      explosions[i].active = true;
      explosions[i].center = pos;
      explosions[i].colorIndex = colorIndex;
      explosions[i].startMs = millis();
      explosions[i].durationMs = 400;
      return;
    }
  }
}

void updateExplosions() {
  uint32_t now = millis();
  for (uint8_t i = 0; i < MAX_EXPLOSIONS; i++) {
    if (!explosions[i].active) continue;
    if (now - explosions[i].startMs >= explosions[i].durationMs) {
      explosions[i].active = false;
    }
  }
}

// ====================== Render LED pásku ======================

void renderStrip() {
  for (int i = 0; i < LED_COUNT; i++) {
    strip.setPixelColor(i, 0);
  }

  renderBases();

  for (uint8_t i = 0; i < MAX_PROJECTILES; i++) {
    if (!projectiles[i].active) continue;
    if (projectiles[i].position < 0 || projectiles[i].position >= LED_COUNT) continue;
    uint8_t idx = projectiles[i].colorIndex;
    uint32_t c = colorFromPalette(idx, 255);
    strip.setPixelColor(projectiles[i].position, c);
  }

  uint32_t now = millis();
  for (uint8_t i = 0; i < MAX_EXPLOSIONS; i++) {
    if (!explosions[i].active) continue;
    float t = (float)(now - explosions[i].startMs) / (float)explosions[i].durationMs;
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    uint8_t brightness = (uint8_t)((1.0f - t) * 255.0f);

    for (int d = -1; d <= 1; d++) {
      int16_t pos = explosions[i].center + d;
      if (pos < 0 || pos >= LED_COUNT) continue;
      uint32_t baseColor = colorFromPalette(explosions[i].colorIndex, brightness);
      strip.setPixelColor(pos, baseColor);
    }
  }

  strip.show();
}

void renderBases() {
  uint8_t phase = (basePulsePhase >> 2) & 0xFF;
  uint8_t brightness = (phase < 128) ? (phase * 2) : ((255 - phase) * 2);
  if (brightness < 60) brightness = 60;

  uint32_t playerColor = ((uint32_t)brightness << 16) | ((uint32_t)brightness << 8) | (uint32_t)brightness;

  for (int i = PLAYER_BASE_START; i <= PLAYER_BASE_END; i++) {
    strip.setPixelColor(i, playerColor);
  }

  uint32_t cycle = (millis() / 800) % difficultyColors;
  uint32_t enemyColor = colorFromPalette((uint8_t)cycle, brightness);

  for (int i = ENEMY_BASE_START; i <= ENEMY_BASE_END; i++) {
    strip.setPixelColor(i, enemyColor);
  }
}

uint32_t colorFromPalette(uint8_t idx, uint8_t brightness) {
  if (idx >= 8) idx = 7;
  Color c = COLOR_PALETTE[idx];
  uint32_t col = strip.Color(
    (uint8_t)((c.r * brightness) / 255),
    (uint8_t)((c.g * brightness) / 255),
    (uint8_t)((c.b * brightness) / 255)
  );
  return col;
}

uint32_t scaleColor(uint32_t c, uint8_t scale) {
  uint8_t r = (uint8_t)((uint8_t)(c >> 16) * scale / 255);
  uint8_t g = (uint8_t)((uint8_t)(c >> 8)  * scale / 255);
  uint8_t b = (uint8_t)((uint8_t)c         * scale / 255);
  return strip.Color(r, g, b);
}
