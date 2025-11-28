/*
================================================================================
hra Přežij
================================================================================

POPIS PROJEKTU:
Tento projekt vytváří interaktivní kvízovou hru "Přežij!" kde hráč odpovídá na otázky
a snaží se zničit nepřítele střelami vystřelenými díky správně zodpovězeným otázkám.
Nepřítel současně střílí zpět a hráč musí odpovídat dost rychle, aby ho vystřelil dříve,
než ho nepřítel zasáhne.

HRÁ: 
- Hráč odpovídá na otázky s 4 možnostmi odpovědí (otázky jsou nahrány skrze web)
- Nepřítel střílí automaticky každé 4 sekundy (z LED 119 → LED 0)
- Správná odpověď = vystřelí protistřelu směrem k nepřítele (z LED 0 → LED 119)
- Špatná odpověď = žádná střela
- Kolize střel = exploze, střely mimo explozi zůstávají aktivní
- Výhra hráče = jeho střela dosáhne nepřítele (LED 119)
- Prohra = střela nepřítele dosáhne hráče (LED 0)

VYŽADOVANÝ HARDWARE:
- ESPWLED (https://www.laskakit.cz/laskakit-espwled/?variantId=16925)
- WS2812B / NeoPixel LED pásek (https://www.laskakit.cz/vyhledavani/?string=ws2812b) (60 LED/m = 2m = 120 LED, řízení GPIO 5)
- Napájení: 5V/2A+ pro LED pásku
- Propojení: LED_DATA → DAT(GPIO5), LED_GND → GND, LED_5V → PWR(5V)

POUŽITÉ KNÍHOVNY:
• Adafruit_NeoPixel (v1.15.2+) - ovládání WS2812B LED pásky
• ESPAsyncWebServer (3.9.1+) - asynchronní webový server
• AsyncTCP (3.4.7+) - TCP základ pro async web server

VLASTNOSTI:
• Webové UI přes WiFi AP (SSID: hraPrezijAP, heslo: prezij1234)
• Nahrát otázky přímo v prohlížeči (formát: "Číslo otázky;Otázka?;Odpivěď1;Odpivěď2;Odpivěď3;Odpivěď4;Index správné odpovědi 1..4")
• Uložení WiFi přihlašovacích údajů (automatické připojení)
• 2x "Ještě jednou" (skip otázky)
• Real-time animace střel a exploze (50ms krok)
• Otázky uloženy v RAM (max 200 otázek)

POKYNY:
1. Nahrát kód do ESPWLED přes Arduino IDE / prezij1234
2. Připojit se na WiFi "hraPrezijAP" (heslo: utec1234)
3. Otevřít IP adresu (obvykle 192.168.4.1)
4. Nahrát otázky a nastavit WiFi (volitelně)
5. Hrát! Odpovídat na otázky dříve než nepřítel vystřelí

AUTOR: laskakit.cz (2025)
VERZE: 1.0 
================================================================================
*/

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <Adafruit_NeoPixel.h>
#include <Preferences.h>
#include <vector>

#define LED_PIN        5
#define LED_COUNT      120

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_BRG + NEO_KHZ800);
AsyncWebServer server(80);
Preferences prefs;

// ---- Otazky ----
struct Question {
  String id;
  String text;
  String answers[4];
  uint8_t correctIndex; // 1..4
};

#define MAX_QUESTIONS 200
std::vector<Question> questions; // RAM-only

int currentQuestionIndex = 0;
int turnCounter = 0;
int skipsUsed = 0;
bool gameOver = false;
String gameResult = ""; // "hrac zvitezil" / "prothrac zvitezil"

// WiFi AP 
const char* apSsid = "hraPrezijAP";
const char* apPass = "prezij1234";
// ulozeno do trvale pameti
String savedSSID = "";
String savedPass = "";

// citace
int lastValidCount = 0;
int lastInvalidCount = 0;

// ---- Shot system ----
// Protivnika (ESPWLED) strela: jedna střela z LED_COUNT-1 -> 0
int enemyShotPos = -1; // -1 = no active enemy shot
unsigned long lastEnemyShotTime = 0;
const unsigned long ENEMY_SHOT_INTERVAL = 4000UL; // 4s

// Hrac: strely
#define MAX_PLAYER_SHOTS 10
int playerShots[MAX_PLAYER_SHOTS]; // positions
int playerShotCount = 0;

// Casovani pohybu strely
unsigned long lastShotStep = 0;
const unsigned long SHOT_STEP = 50UL; // 50 ms

// Animace exploze
bool explosionActive = false;
int explosionCenter = -1;
int explosionFrame = 0;
const int EXPLOSION_FRAMES = 6;

// UI log (RAM)
String gameLog = "";

// Deklarace
void drawScene();
bool parseQuestionsFromString(const String &data);
String generateIndexHtml();
void nextQuestionRandom();
void resetGameState();
void enemyTryShoot();
void updateShots();
void drawShotsToStrip();
void triggerExplosion(int pos);
void playExplosionFrame();
String jsonEscape(const String &s);

// Utilities
void clearStrip() {
  for (int i=0;i<LED_COUNT;i++) strip.setPixelColor(i, 0);
}
uint32_t colorRGB(uint8_t r, uint8_t g, uint8_t b) { return strip.Color(r,g,b); }

// ---------------- Implementace ----------------

void resetGameState() {
  turnCounter = 0;
  skipsUsed = 0;
  gameOver = false;
  gameResult = "";
  enemyShotPos = -1;
  playerShotCount = 0;
  for (int i=0;i<MAX_PLAYER_SHOTS;i++) playerShots[i] = -1;
  explosionActive = false;
  explosionCenter = -1;
  explosionFrame = 0;
  lastEnemyShotTime = millis();
  addGameLogOnce("Hra restartována.");
  if (questions.size() > 0) currentQuestionIndex = 0;
  drawScene();
}

void nextQuestionRandom() {
  if (questions.size() == 0) return;
  int idx = random(0, questions.size());
  currentQuestionIndex = idx;
}

bool parseQuestionsFromString(const String &data) {
  questions.clear();
  lastValidCount = 0;
  lastInvalidCount = 0;

  int start = 0;
  while (start < (int)data.length()) {
    int eol = data.indexOf('\n', start);
    if (eol == -1) eol = data.length();
    String line = data.substring(start, eol);
    start = eol + 1;
    line.trim();
    if (line.length() == 0) continue;

    // rozdeleni ';'
    std::vector<String> parts;
    int pstart = 0;
    while (pstart <= (int)line.length()) {
      int p = line.indexOf(';', pstart);
      if (p == -1) p = line.length();
      String part = line.substring(pstart, p);
      part.trim();
      parts.push_back(part);
      pstart = p + 1;
      if (p >= (int)line.length()) break;
    }

    if (parts.size() != 7) { lastInvalidCount++; continue; }
    if (parts[0].length() == 0) { lastInvalidCount++; continue; }
    int id = parts[0].toInt();
    if (id <= 0) { lastInvalidCount++; continue; }
    if (parts[1].length() == 0) { lastInvalidCount++; continue; }
    bool emptyAnswer = false;
    for (int i = 2; i <= 5; ++i) if (parts[i].length() == 0) { emptyAnswer = true; break; }
    if (emptyAnswer) { lastInvalidCount++; continue; }
    int correct = parts[6].toInt();
    if (correct < 1 || correct > 4) { lastInvalidCount++; continue; }

    Question q;
    q.id = String(id);
    q.text = parts[1];
    q.answers[0] = parts[2];
    q.answers[1] = parts[3];
    q.answers[2] = parts[4];
    q.answers[3] = parts[5];
    q.correctIndex = (uint8_t)correct;
    questions.push_back(q);
    lastValidCount++;
    if ((int)questions.size() >= MAX_QUESTIONS) break;
  }

  Serial.printf("Otazky nahrany: valid=%d invalid=%d", lastValidCount, lastInvalidCount);
  return lastValidCount > 0;
}

void drawScene() {
  // základní pozadí
  clearStrip();
  for (int i = 0; i < LED_COUNT; i++) strip.setPixelColor(i, colorRGB(6, 8, 18));
  // hráčova základna (LED 0)
  strip.setPixelColor(0, colorRGB(0, 120, 0));
  // nepřátelská základna (poslední LED)
  strip.setPixelColor(LED_COUNT - 1, colorRGB(150, 0, 0));

  // střely vždy kreslíme tak, aby byly viditelné i během exploze
  drawShotsToStrip();

  if (explosionActive) {
    // vykreslí explozní animaci navrch střel
    playExplosionFrame();
    // NERETURN - volání playExplosionFrame() samo volá strip.show()
  } else {
    // normální zobrazení
    strip.show();
  }
}


String lastLogMessage = "";

void addGameLogOnce(const String &msg) {
  // Přidej zprávu jen pokud není shodná s poslední přidanou zprávou
  if (msg != lastLogMessage) {
    gameLog = msg + "\n" + gameLog;
    lastLogMessage = msg;
  }
}

void drawShotsToStrip() {
  // strela protivnika
  if (enemyShotPos >= 0 && enemyShotPos < LED_COUNT) {
    strip.setPixelColor(enemyShotPos, colorRGB(255, 0, 0));
    int behind = enemyShotPos + 1;
    if (behind >= 0 && behind < LED_COUNT) strip.setPixelColor(behind, colorRGB(80, 10, 10));
  }
  // strela hrace
  for (int i=0;i<playerShotCount;i++) {
    int p = playerShots[i];
    if (p >= 0 && p < LED_COUNT) {
      strip.setPixelColor(p, colorRGB(0, 255, 0));
      int behind = p - 1; if (behind >= 0 && behind < LED_COUNT) strip.setPixelColor(behind, colorRGB(20,80,30));
    }
  }
}

void triggerExplosion(int pos) {
  explosionActive = true;
  explosionCenter = pos;
  explosionFrame = 0;
  enemyShotPos = -2;

  int newCount = 0;
  for (int i=0; i<playerShotCount; i++) {
    int p = playerShots[i];
    // Odstraň pouze střely přesně na pozici výbuchu
    if (p == pos) continue;
    playerShots[newCount++] = p;
  }
  playerShotCount = newCount;

  addGameLogOnce("Výbuch na pozici " + String(pos) + ".");
}

void playExplosionFrame() {
  // Nepoužívat clearStrip() ani podbarvení celé scény, aby střely zůstaly viditelné
  // clearStrip();
  // for (int i = 0; i < LED_COUNT; i++) strip.setPixelColor(i, colorRGB(6,8,18));

  int fixedRadius = 3; // polovina délky exploze ~ 6 LED celkem
  int radius = (explosionFrame <= fixedRadius) ? explosionFrame : fixedRadius;

  for (int d = -radius; d <= radius; d++) {
    int p = explosionCenter + d;
    if (p < 0 || p >= LED_COUNT) continue;

    int intensity = 255 - (explosionFrame * 30);
    if (intensity < 40) intensity = 40;
    uint8_t r = min(255, intensity + 60);
    uint8_t g = max(0, intensity / 3);

    // Sloučení barvy výbuchu se současnou barvou LED pro efekt překrytí
    uint32_t currentColor = strip.getPixelColor(p);
    uint8_t cr = (currentColor >> 16) & 0xFF;
    uint8_t cg = (currentColor >> 8) & 0xFF;
    uint8_t cb = currentColor & 0xFF;

    uint8_t nr = min(255, cr + r);
    uint8_t ng = min(255, cg + g);
    uint8_t nb = cb; // výbuch žádnou modrou nepřidává

    strip.setPixelColor(p, colorRGB(nr, ng, nb));
  }

  strip.show();

  explosionFrame++;
  if (explosionFrame > EXPLOSION_FRAMES) {
    explosionActive = false;
    explosionCenter = -1;
    explosionFrame = 0;
    drawScene(); // Vykreslí scénu znovu bez exploze
  }
}



void enemyTryShoot() {
  if (gameOver) return;
  unsigned long now = millis();
  if (enemyShotPos == -1 && (now - lastEnemyShotTime >= ENEMY_SHOT_INTERVAL)) {
    enemyShotPos = LED_COUNT - 1;
    lastEnemyShotTime = now;
    addGameLogOnce("Nepřítel vystřelil (poz " + String(enemyShotPos) + ").");
  }
}

void updateShots() {
  if (gameOver) return;
  unsigned long now = millis();
  if (now - lastShotStep < SHOT_STEP) return;
  lastShotStep = now;

  // --- posun hráčových střel ---
  for (int i = 0; i < playerShotCount; i++) {
    playerShots[i]++;
  }

  // --- kontrola, zda hráčova střela nedošla do cíle ---
  int newCount = 0;
  for (int i = 0; i < playerShotCount; i++) {
    if (playerShots[i] >= LED_COUNT - 1) {
      gameOver = true;
      gameResult = "player_win";
      addGameLogOnce("Hráčova střela dosáhla cíle. Výhra hráče.");
      drawScene();
      return;
    }
  }

  // --- posun střely nepřítele ---
  if (enemyShotPos != -1) {
    enemyShotPos--;
    if (enemyShotPos <= 0) {
      gameOver = true;
      gameResult = "enemy_win";
      addGameLogOnce("Střela protivníka zasáhla hráče. Prohra hráče.");
      drawScene();
      return;
    }
  }

  // --- KOLIZE: hráčovy střely x střela nepřítele ---
  if (enemyShotPos != -1) {
    // Nové pole pro hráčovy střely, které nepřišly do kontaktu
    int updatedCount = 0;
    int explosionPos = -1;
    bool collisionDetected = false;

    for (int i = 0; i < playerShotCount; i++) {
      if (abs(playerShots[i] - enemyShotPos) <= 2) {
        // Kolize se střelou nepřítele
        collisionDetected = true;
        // Určíme pozici exploze jako střed první kolizní střely a nepřátelské
        if (explosionPos == -1) {
          explosionPos = (playerShots[i] + enemyShotPos) / 2;
        }
        // tuto střelu neukládáme do nového pole (tj. mažeme ji)
      } else {
        // střela není v kolizi, ponecháme ji
        playerShots[updatedCount++] = playerShots[i];
      }
    }

    // detekce kolize
    if (collisionDetected) {
      triggerExplosion(explosionPos);
      enemyShotPos = -1;
      playerShotCount = updatedCount;
    }
  }

  drawScene(); // vykresleni sceny
}




// Otazky z webu
void handleAnswer(uint8_t chosenIndex, String &responseJson) {
  if (gameOver) {
    responseJson = String("{\"status\":\"error\",\"message\":\"Hra skončila\",\"gameOver\":true}");
    return;
  }
  if (questions.size() == 0) {
    responseJson = String("{\"status\":\"error\",\"message\":\"Žádné otázky\"}");
    return;
  }

  turnCounter++;
  Question &q = questions[currentQuestionIndex];
  bool correct = (chosenIndex == q.correctIndex);
  String status = correct ? "correct" : "wrong";
  if (correct) {
    // vystřelí hráč, pokud je v poli místo
    if (playerShotCount < MAX_PLAYER_SHOTS) {
      playerShots[playerShotCount++] = 0; // new shot at LED 0
      addGameLogOnce("Hráč vystřelil protistřelu.");
    } else {
      addGameLogOnce("Hráč chtěl vystřelit, ale překročil maximální počet střel.");
    }
  } else {
    addGameLogOnce("Hráč odpověděl špatně.");

  }

  nextQuestionRandom();

  String json = "{";
  json += "\"status\":\"" + status + "\"";
  json += ",\"playerShotCount\":" + String(playerShotCount);
  json += ",\"enemyShotPos\":" + String(enemyShotPos);
  json += ",\"turnCounter\":" + String(turnCounter);
  json += ",\"gameOver\":";
  json += (gameOver ? "true" : "false");
  if (gameOver) json += ",\"result\":\"" + gameResult + "\"";
  json += "}";
  responseJson = json;
}

void handleSkip(String &responseJson) {
  if (gameOver) {
    responseJson = String("{\"status\":\"error\",\"message\":\"Hra skončila\"}");
    return;
  }
  if (skipsUsed >= 2) {
    responseJson = String("{\"status\":\"error\",\"message\":\"Maximální počet 'Ještě jednou' použit\"}");
    return;
  }
  skipsUsed++;
  nextQuestionRandom();
  addGameLogOnce("Ještě jednou použito.");
  drawScene();
  responseJson = String("{\"status\":\"ok\",\"skipsUsed\":") + String(skipsUsed) + String("}");
}

// Web server HTML generator
String generateIndexHtml() {
  String html = R"rawliteral(
<!doctype html>
<html lang="cs">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>hra Přežij!</title>
<style>
  :root {
    --bg: #0f1724;
    --card: #0b1220;
    --accent: #06b6d4;
    --muted: #94a3b8;
  }
  body {
    margin: 0;
    font-family: Inter, system-ui, Arial;
    background: linear-gradient(180deg, #041026 0%, #071733 100%);
    color: #e6eef6;
    padding: 20px;
  }
  .container {
    max-width: 1000px;
    margin: 0 auto;
  }
  .card {
    background: rgba(255, 255, 255, 0.02);
    border-radius: 10px;
    padding: 16px;
    margin-bottom: 14px;
  }
  h1 {
    margin: 0 0 8px 0;
  }
  .q {
    font-size: 18px;
    margin: 10px 0;
    color: #dbeafe;
  }
  .answers {
    display: flex;
    flex-direction: column;
    gap: 8px;
  }
  button.answer {
    width: 100%;
    padding: 12px;
    border-radius: 8px;
    border: 0;
    background: linear-gradient(180deg, var(--accent), #0b86a0);
    cursor: pointer;
    font-weight: 700;
  }
  .controls {
    display: flex;
    gap: 8px;
    align-items: center;
    margin-top: 10px;
  }
  .muted {
    color: var(--muted);
    font-size: 13px;
  }
  .led-preview {
    height: 28px;
    border-radius: 8px;
    background: linear-gradient(90deg, #00121e, #002233);
    display: flex;
    align-items: center;
    padding: 6px;
    overflow: hidden;
    width: 100%;
    box-sizing: border-box;
    margin-top: 8px;
  }
  .pill {
    height: 12px;
    border-radius: 6px;
    margin-right: 4px;
    flex: 0 0 auto;
  }
  .settings-container {
    display: flex;
    flex-direction: column;
    gap: 12px;
  }
  #gameResult {
    margin-bottom: 8px;
    font-weight: 700;
    font-size: 16px;
    color: #80ff80;
  }
</style>
</head>
<body>
<div class="container">
  <div class="card">
    <h1>hra Přežij!</h1>
    <div class="q" id="question">Načítám otázku…</div>
    <div id="answerMessage" style="color:#ff6666; font-weight:bold; margin-bottom:10px;"></div>
    <div class="answers" id="answers"></div>
    <div class="controls">
      <button id="skipBtn" class="answer">Ještě jednou (max 2x)</button>
      <button id="restartBtn" class="answer">Restart hry</button>
      <div style="margin-left:auto" class="muted">Kola: <span id="turnCounter">0</span></div>
    </div>
  </div>

  <div class="settings-container">
    <div class="card">
      <div class="small">Nahrát otázky (RAM only)</div>
      <textarea id="questionsText" rows="8" style="width:100%;background:transparent;color:inherit;border-radius:6px;padding:8px" placeholder="1; Kolik je 2+2?;3;4;5;6;2"></textarea>
      <div style="display:flex;gap:8px;margin-top:8px">
        <button id="uploadBtn" class="answer">Nahrát</button>
        <div id="uploadResult" class="muted"></div>
      </div>
    </div>

    <div class="card">
      <div class="small">Wi-Fi</div>
      <input id="ssid" type="text" placeholder="SSID" style="width:100%;padding:8px;border-radius:6px;background:transparent;color:inherit;margin-bottom:6px">
      <input id="pass" type="password" placeholder="Heslo" style="width:100%;padding:8px;border-radius:6px;background:transparent;color:inherit">
      <div style="display:flex;gap:8px;margin-top:8px">
        <button id="setWifi" class="answer">Uložit</button>
        <div id="wifiResult" class="muted"></div>
      </div>
    </div>
  </div>
</div>


<script>
let skipsLeft = 2;
let gameOverShown = false;

async function fetchState(){
  const res = await fetch('/state');
  if (!res.ok) return;
  const js = await res.json();
  document.getElementById('question').innerText = js.question || "Žádná otázka";
  const answers = js.answers || [];
  const answersDiv = document.getElementById('answers');
  answersDiv.innerHTML = '';
  for (let i=0; i<4; i++){
    const btn = document.createElement('button');
    btn.className = 'answer';
    btn.innerText = answers[i] || ('Odpověď '+(i+1));
    btn.onclick = () => submitAnswer(i+1);
    answersDiv.appendChild(btn);
  }

  document.getElementById('turnCounter').innerText = js.turnCounter || 0;
  skipsLeft = js.skipsLeft;
  document.getElementById('skipBtn').innerText = 'Ještě jednou (zbyvá ' + skipsLeft + ')';

  if (js.gameOver) {
    if (!gameOverShown) {
      const msg = (js.result === 'player_win') ? 'Vyhráli jste!' : 'Vyhrál protivník!';
      alert(msg);
      gameOverShown = true;
    }
  } else {
    gameOverShown = false;
  }

  // Render s předáním pole hráčových střel
  renderPreview(js.playerShotCount, js.enemyShotPos, js.explosionActive, js.fields, js.playerShotsPositions);
}


function renderPreview(playerShotCount, enemyShotPos, explosionActive, fields, playerShotsPos){
  const boxes = 40;
  const box = document.getElementById('ledPreview');
  box.innerHTML = '';
  for (let i=0;i<boxes;i++){
    const pill = document.createElement('div');
    pill.className = 'pill';
    pill.style.width = (100/boxes) + '%';
    pill.style.background = 'rgba(255,255,255,0.02)';
    pill.style.height = '12px';
    pill.style.marginRight = '3px';

    // Vykreslit nepřátelskou střelu
    if (enemyShotPos !== null && enemyShotPos !== undefined && enemyShotPos >= 0) {
      const mappedEnemy = Math.floor(enemyShotPos / (120/boxes));
      if (mappedEnemy === i) pill.style.background = 'linear-gradient(90deg,#ff8b8b,#ff3b3b)';
    }
    // Vykreslit hráčovy střely
    if (playerShotsPos && playerShotsPos.length > 0) {
      for (const pos of playerShotsPos) {
        if (pos !== null && pos !== undefined && pos >= 0) {
          const mappedPlayer = Math.floor(pos / (120/boxes));
          if (mappedPlayer === i) {
            pill.style.background = 'linear-gradient(90deg,#50ff70,#20a030)';
            break;
          }
        }
      }
    }
    // Exploze
    if (explosionActive) pill.style.background = 'linear-gradient(90deg,#ffb86b,#ff3b3b)';

    box.appendChild(pill);
  }
}

async function submitAnswer(idx){
  const res = await fetch('/answer', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:'index='+idx});
  if (!res.ok) {
    const txt = await res.text();
    alert('Chyba: ' + txt);
    return;
  }
  const js = await res.json();
  
  const answerMsg = document.getElementById('answerMessage');
  
  if (js.status === 'correct') {
    answerMsg.style.color = '#70ff70';
    answerMsg.textContent = 'Odpověď ' + idx + ' - správně! (Vystřelena protistřela)';
  } else if (js.status === 'wrong') {
    answerMsg.style.color = '#ff6666';
    answerMsg.textContent = 'Odpověď ' + idx + ' - špatně! (Žádná protistřela)';
  } else {
    answerMsg.textContent = '';
  }

  await fetchState();
}

document.getElementById('skipBtn').addEventListener('click', async ()=>{
  if (skipsLeft <= 0) { alert('Už nemůžeš použít Ještě jednou.'); return; }
  const res = await fetch('/skip', {method:'POST'});
  const js = await res.json();
  if (js.status === 'ok') {
    document.getElementById('log').innerHTML = '<div>Ještě jednou použito</div>' + document.getElementById('log').innerHTML;
  }
  await fetchState();
});

document.getElementById('uploadBtn').addEventListener('click', async ()=>{
  const qs = document.getElementById('questionsText').value;
  if (!qs || qs.trim().length===0) { alert('Vlož text s otázkami.'); return; }

  const body = new URLSearchParams();
  body.append('questions', qs);

  const res = await fetch('/uploadQuestions', {
    method: 'POST',
    headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    body: body.toString()
  });

  const js = await res.json();
  if (res.ok) {
    document.getElementById('uploadResult').innerText =
      'Nahráno: ' + js.count + ' otázek (' + (js.invalid||0) + ' chybných)';
    document.getElementById('log').innerHTML =
      '<div>Nahrány otázky: '+js.count+'</div>' + document.getElementById('log').innerHTML;

    await fetchState();
  } else {
    alert('Chyba nahrání: ' + (js.message || JSON.stringify(js)));
  }
});

document.getElementById('setWifi').addEventListener('click', async ()=>{
  const ssid = document.getElementById('ssid').value;
  const pass = document.getElementById('pass').value;
  const body = new URLSearchParams();
  body.append('ssid', ssid);
  body.append('pass', pass);
  const res = await fetch('/setWiFi', {method:'POST', body: body});
  if (res.ok) {
    document.getElementById('wifiResult').innerText = 'Uloženo. ESP se pokusí připojit.';
  } else document.getElementById('wifiResult').innerText = 'Chyba.';
});

document.getElementById('restartBtn').addEventListener('click', async ()=>{
  if (!confirm('Opravdu restartovat hru?')) return;
  const res = await fetch('/restartGame', {method:'POST'});
  if (res.ok) {
    document.getElementById('log').innerHTML = '<div>Hra restartována</div>' + document.getElementById('log').innerHTML;
    await fetchState();
  }
});

setInterval(fetchState, 300);
fetchState();
</script>
</body>
</html>
)rawliteral";
  return html;
}


String jsonEscape(const String &s) {
  String out;
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s.charAt(i);
    if (c == '"') out += "\\\"";     // escapovaný uvozovky pro JSON
    else if (c == '\n') out += "\\n"; // escapovaný nový řádek
    else if (c == '\'') { /* ignorovat apostrof */ }
    else out += c;
  }
  return out;
}

void setupWebServer() {
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = generateIndexHtml();
    request->send(200, "text/html", html);
  });

  server.on("/state", HTTP_GET, [](AsyncWebServerRequest *request){
    String qtext = "\"\"";
    String ansJson = "[]";
    if (questions.size() > 0) {
      Question &q = questions[currentQuestionIndex];
      qtext = "\"" + jsonEscape(q.text) + "\"";
      ansJson = "[";
      for (int i=0;i<4;i++){
        ansJson += "\"" + jsonEscape(q.answers[i]) + "\"";
        if (i<3) ansJson += ",";
      }
      ansJson += "]";
    }
    String json = "{";
    json += "\"question\":" + qtext + ",";
    json += "\"answers\":" + ansJson + ",";
    json += "\"playerShotCount\":" + String(playerShotCount) + ",";
    json += "\"enemyShotPos\":" + String(enemyShotPos) + ",";
    json += "\"explosionActive\":" + (explosionActive?String("true"):String("false")) + ",";
    json += "\"turnCounter\":" + String(turnCounter) + ",";
    json += "\"skipsLeft\":" + String(max(0,2-skipsUsed)) + ",";
    json += "\"gameOver\":" + (gameOver?String("true"):String("false")) + ",";
    if (gameOver) json += "\"result\":\"" + gameResult + "\",";
    json += "\"log\":\"" + jsonEscape(gameLog) + "\"";
    json += "}";
    request->send(200, "application/json", json);
  });

  server.on("/answer", HTTP_POST, [](AsyncWebServerRequest *request){
    uint8_t idx = 0;
    if (request->hasParam("index", true)) idx = request->getParam("index", true)->value().toInt();
    else if (request->hasParam("index", false)) idx = request->getParam("index", false)->value().toInt();
    String responseJson;
    if (idx < 1 || idx > 4) {
      responseJson = "{\"status\":\"error\",\"message\":\"Neplatná odpověď\"}";
      request->send(400, "application/json", responseJson);
      return;
    }
    handleAnswer(idx, responseJson);
    request->send(200, "application/json", responseJson);
  });

  server.on("/skip", HTTP_POST, [](AsyncWebServerRequest *request){
    String resp; handleSkip(resp); request->send(200, "application/json", resp);
  });

  server.on("/uploadQuestions", HTTP_POST, [](AsyncWebServerRequest *request){
    String data = "";
    if (request->hasParam("questions", true)) {
      data = request->getParam("questions", true)->value();
    }
    else if (request->hasParam("questions", false)) data = request->getParam("questions", false)->value();
    if (data.length() == 0) { request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Žádný obsah\"}"); return; }
    bool ok = parseQuestionsFromString(data);
    if (ok) {
      currentQuestionIndex = 0; resetGameState();
      String resp = "{\"status\":\"ok\",\"count\":" + String(lastValidCount) + ",\"invalid\":" + String(lastInvalidCount) + "}";
      request->send(200, "application/json", resp);
    } else {
      String resp = "{\"status\":\"error\",\"message\":\"Chybný formát nebo žádné platné otázky\",\"valid\":" + String(lastValidCount) + ",\"invalid\":" + String(lastInvalidCount) + "}";
      request->send(400, "application/json", resp);
    }
  });

  server.on("/setWiFi", HTTP_POST, [](AsyncWebServerRequest *request){
    String ssid = ""; String pass = "";
    if (request->hasParam("ssid", true)) ssid = request->getParam("ssid", true)->value();
    if (request->hasParam("pass", true)) pass = request->getParam("pass", true)->value();
    savedSSID = ssid; savedPass = pass;
    prefs.begin("wifi", false); prefs.putString("ssid", savedSSID); prefs.putString("pass", savedPass); prefs.end();
    if (savedSSID.length() > 0) WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    request->send(200, "application/json", "{\"status\":\"ok\"}");
  });

  server.on("/restartGame", HTTP_POST, [](AsyncWebServerRequest *request){ resetGameState(); request->send(200, "application/json", "{\"status\":\"ok\"}"); });

  server.begin();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  strip.begin(); strip.show();

  prefs.begin("wifi", true);
  savedSSID = prefs.getString("ssid", "");
  savedPass = prefs.getString("pass", "");
  prefs.end();

  WiFi.softAP(apSsid, apPass);
  IPAddress ip = WiFi.softAPIP();
  Serial.printf("AP started. IP: %s", ip.toString().c_str());

  if (savedSSID.length() > 0) {
    Serial.printf("Attempting to connect to saved SSID: %s", savedSSID.c_str());
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    unsigned long start = millis();
    const unsigned long timeout = 8000;
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout) delay(250);
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected to WiFi as station. IP: "); Serial.println(WiFi.localIP());
    } else Serial.println("Failed to connect to saved WiFi - staying as AP (fallback)");
  }

  setupWebServer();

  if (questions.size() == 0) {
    Question q; q.id = "1"; q.text = "Kolik je 2+2?"; q.answers[0] = "3"; q.answers[1] = "4"; q.answers[2] = "5"; q.answers[3] = "6"; q.correctIndex = 2; questions.push_back(q);
  }

  resetGameState();
  Serial.println("Setup done");
}

void loop() {
  // wifi reconnect
  static unsigned long lastConnAttempt = 0;
  if (savedSSID.length() > 0 && WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if (now - lastConnAttempt > 15000) { WiFi.begin(savedSSID.c_str(), savedPass.c_str()); lastConnAttempt = now; }
  }

  enemyTryShoot();
  updateShots();

  // kratká prodleva
  delay(2);
}
