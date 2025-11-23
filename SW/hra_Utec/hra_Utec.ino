/*
================================================================================
Uteč! - Hra pro ESPWLED s adresovatelným RGB LED páskem
================================================================================

Popis hry:
-----------
"Uteč!" je jednoduchá interaktivní hra, kde hráč závodí s duchem přes sérii polí
zobrazených adresovatelným LED páskem (např. WS2812B 60LED/m). Cílem hráče je dosáhnout
koncového pole dříve, než ho duch dohoní. Hráč postupuje správnými odpověďmi
na otázky, duch se pohybuje při chybných odpovědích, přičemž rychlost ducha se
zvyšuje s počtem kol.

Pravidla:
----------
1. Hráč začíná na předem definovaném poli (např. pole 3), duch na poli 1.
2. Hráč odpovídá na otázky zadané přes webové rozhraní.
3. Správná odpověď posouvá hráče o 1 pole vpřed.
4. Špatná odpověď posouvá ducha o 1–3 pole vpřed v závislosti na pořadí kola:
   - Kola 1–3: duch +1 pole
   - Kola 4–6: duch +2 pole
   - Kola 7+: duch +3 pole
5. Hráč může použít maximálně 2x možnost "Ještě jednou" pro přeskočení otázky
   bez postupu ducha.
6. Hra končí, když:
   - hráč dosáhne posledního pole → hráč vyhrává
   - duch dohoní nebo předběhne hráče → duch vyhrává
7. LED pás zobrazuje průběh hry:
   - Zelené LED: hráč
   - Červené LED: duch
   - Neaktivní pole: bílé LED na 10 % jasu
   - Při konci hry bliká LED pás podle vítěze

Použité knihovny:
------------------
1. Adafruit_NeoPixel  - pro ovládání adresovatelných LED.
2. AsyncTCP           - TCP server pro asynchronní komunikaci (základ pro ESPAsyncWebServer).
3. ESPAsyncWebServer  - asynchronní webový server pro webové rozhraní hry.
4. Preferences        - ukládání Wi-Fi přihlašovacích údajů a nastavení LED do NVS.
5. Vector (STL)       - uchování otázek v RAM.

Formát otázek:
--------------
Každá otázka se nahrává ve formátu CSV se 7 poli oddělenými středníkem:
ID;Otázka;Odpověď1;Odpověď2;Odpověď3;Odpověď4;IndexSprávnéOdpovědi(1..4)

- ID: unikátní číslo otázky (>0)
- Otázka: text otázky
- Odpovědi: 4 textové možnosti (musí být vyplněné)
- Index správné odpovědi: číslo 1–4, určující která odpověď je správná

Poznámky k implementaci:
------------------------
- Hra běží na ESPWLED (ESP32-C3), LED pásku (60LED/m, celkem 2m) je inicializován podle definovaného PINu a počtu LED.
- Webové rozhraní umožňuje:
  - Zobrazit aktuální otázku a odpovědi
  - Odeslat odpověď
  - Použít "Ještě jednou" (max 2x)
  - Restartovat hru
  - Nahrát nové otázky (RAM-only)
  - Nastavit Wi-Fi připojení, které se ukládá do NVS
- Otázky jsou uchovávány pouze v RAM, nejsou ukládány na SPIFFS.
- LED pás zobrazuje dynamicky hráče a ducha a indikaci konce hry.
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
#define LEDS_PER_FIELD 8
#define FIELDS         (LED_COUNT / LEDS_PER_FIELD) // 15

Adafruit_NeoPixel strip(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

AsyncWebServer server(80);
Preferences prefs;

// Game state
struct Question {
  String id;
  String text;
  String answers[4];
  uint8_t correctIndex; // 1..4
};

#define MAX_QUESTIONS 200
std::vector<Question> questions; // QUESTIONS IN RAM ONLY

int currentQuestionIndex = 0;
int playerField = 3; // starts on field 3
int ghostField = 1;  // starts on field 1
int turnCounter = 0; // increments after each answered question (correct or wrong)
int skipsUsed = 0;   // "Ještě jednou" used, max 2
bool gameOver = false;
String gameResult = ""; // "player_win" / "ghost_win" / ""

const char* apSsid = "hraUtecAP";
const char* apPass = "utec1234"; // nebo prázdné

// WiFi station creds stored in NVS (Preferences)
String savedSSID = "";
String savedPass = "";

// For reporting validation results
int lastValidCount = 0;
int lastInvalidCount = 0;

// Forward
void drawFields();
bool parseQuestionsFromString(const String &data);
String generateIndexHtml();
void nextQuestionRandom();
void resetGameState();

// ------- Utilities -------
void toLowerTrim(String &s) {
  // small helper
  s.trim();
}

// LED helpers
void setFieldColor(int field, uint32_t color) {
  if (field < 1) return;
  if (field > FIELDS) return;
  int start = (field - 1) * LEDS_PER_FIELD;
  for (int i = 0; i < LEDS_PER_FIELD; ++i) {
    strip.setPixelColor(start + i, color);
  }
}

void clearStripDim() {
  for (int i = 0; i < LED_COUNT; ++i) strip.setPixelColor(i, strip.Color(0,0,0));
}

void showStrip() {
  strip.show();
}

// map color helper
uint32_t colorRGB(uint8_t r, uint8_t g, uint8_t b) {
  return strip.Color(r,g,b);
}

// ----- Game logic -----
void resetGameState() {
  playerField = 3;
  ghostField = 1;
  turnCounter = 0;
  skipsUsed = 0;
  gameOver = false;
  gameResult = "";
  if (questions.size() > 0) currentQuestionIndex = 0;
  drawFields();
}

void checkEndConditions() {
  if (playerField >= FIELDS) {
    gameOver = true;
    gameResult = "player_win";
  } else if (ghostField >= playerField) {
    gameOver = true;
    gameResult = "ghost_win";
  }
}

void handleAnswer(uint8_t chosenIndex, String &responseJson) {
  if (gameOver) {
    responseJson = String("{\"status\":\"error\",\"message\":\"Hra skončila\",\"gameOver\":true}");
    return;
  }
  if (questions.size() == 0) {
    responseJson = String("{\"status\":\"error\",\"message\":\"Žádné otázky\"}");
    return;
  }
  turnCounter++; // one turn passed (answer submitted)
  Question &q = questions[currentQuestionIndex];
  bool correct = (chosenIndex == q.correctIndex);
  if (correct) {
    playerField += 1;
  } else {
    // ghost moves depending on turn count
    int ghostMove = 1;
    if (turnCounter <= 3) ghostMove = 1;
    else if (turnCounter <= 6) ghostMove = 2;
    else ghostMove = 3;
    ghostField += ghostMove;
  }
  checkEndConditions();
  // choose next question automatically (random)
  nextQuestionRandom();

  // update LEDs
  drawFields();

  // prepare response JSON
  String status = correct ? "correct" : "wrong";
  String json = "{";
  json += "\"status\":\"" + status + "\"";
  json += ",\"playerField\":" + String(playerField);
  json += ",\"ghostField\":" + String(ghostField);
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
  // do not increment turnCounter, no ghost movement
  drawFields();
  responseJson = String("{\"status\":\"ok\",\"skipsUsed\":") + String(skipsUsed) + String("}");
}

void nextQuestionRandom() {
  if (questions.size() == 0) return;
  int idx = random(0, questions.size());
  currentQuestionIndex = idx;
}

// ---- Validation and parsing of uploaded questions ----
// Expected line format (exactly 7 parts separated by ';'):
// ID; Question; Answer1; Answer2; Answer3; Answer4; CorrectIndex(1..4)
// Lines not matching requirements are ignored and counted as invalid.
// Returns true if at least one valid question was parsed.
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

    // split line by ';' into parts
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

    // require exactly 7 parts
    if (parts.size() != 7) {
      lastInvalidCount++;
      continue;
    }

    // validate ID (numeric and >0)
    if (parts[0].length() == 0) { lastInvalidCount++; continue; }
    int id = parts[0].toInt();
    if (id <= 0) { lastInvalidCount++; continue; }

    // validate question text
    if (parts[1].length() == 0) { lastInvalidCount++; continue; }

    // validate 4 answers (non-empty)
    bool emptyAnswer = false;
    for (int i = 2; i <= 5; ++i) {
      if (parts[i].length() == 0) { emptyAnswer = true; break; }
    }
    if (emptyAnswer) { lastInvalidCount++; continue; }

    // validate correct index
    int correct = parts[6].toInt();
    if (correct < 1 || correct > 4) { lastInvalidCount++; continue; }

    // All validations passed -> save question
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

  Serial.printf("Questions loaded: valid=%d invalid=%d\n", lastValidCount, lastInvalidCount);
  return lastValidCount > 0;
}

// ---- LEDs drawing ----
void drawFields() {
  clearStripDim();
  // draw path as dim blue on all fields
  for (int f=1; f<=FIELDS; ++f) {
    int start = (f-1)*LEDS_PER_FIELD;
    for (int i=0;i<LEDS_PER_FIELD;i++) {
      // dim background
      strip.setPixelColor(start+i, colorRGB(10,10,20));
    }
  }
  // ghost red
  if (ghostField >= 1 && ghostField <= FIELDS) {
    setFieldColor(ghostField, colorRGB(180,0,0));
  }
  // player green
  if (playerField >= 1 && playerField <= FIELDS) {
    setFieldColor(playerField, colorRGB(0,160,0));
  }
  // if game over, flash winner
  if (gameOver) {
    if (gameResult == "player_win") {
      for (int i=0;i<LED_COUNT;i++) strip.setPixelColor(i, colorRGB(0,100,0));
    } else {
      for (int i=0;i<LED_COUNT;i++) strip.setPixelColor(i, colorRGB(100,0,0));
    }
  }
  showStrip();
}

// ---- Web server endpoints and page ----
String jsonEscape(const String &s) {
  String out;
  for (size_t i = 0; i < s.length(); ++i) {
    char c = s.charAt(i);
    if (c == '"') out += "\\\"";
    else if (c == '\n') out += "\\n";
    else if (c == '\r') { /* skip */ }
    else out += c;
  }
  return out;
}

void setupWebServer() {
  // Serve index
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    String html = generateIndexHtml();
    request->send(200, "text/html", html);
  });

  // get current game state + question
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
    json += "\"playerField\":" + String(playerField) + ",";
    json += "\"ghostField\":" + String(ghostField) + ",";
    json += "\"turnCounter\":" + String(turnCounter) + ",";
    json += "\"skipsUsed\":" + String(skipsUsed) + ",";
    json += "\"skipsLeft\":" + String(max(0,2-skipsUsed)) + ",";
    json += "\"gameOver\":" + (gameOver?String("true"):String("false")) + ",";
    if (gameOver) json += "\"result\":\"" + gameResult + "\",";
    json += "\"fields\":" + String(FIELDS);
    json += "}";
    request->send(200, "application/json", json);
  });

  // submit answer (POST JSON or form field "index" 1..4)
  server.on("/answer", HTTP_POST, [](AsyncWebServerRequest *request){
    uint8_t idx = 0;
    if (request->hasParam("index", true)) {
      idx = request->getParam("index", true)->value().toInt();
    } else if (request->hasParam("index", false)) {
      idx = request->getParam("index", false)->value().toInt();
    }
    String responseJson;
    if (idx < 1 || idx > 4) {
      responseJson = "{\"status\":\"error\",\"message\":\"Neplatná odpověď\"}";
      request->send(400, "application/json", responseJson);
      return;
    }
    handleAnswer(idx, responseJson);
    request->send(200, "application/json", responseJson);
  });

  // skip ("Ještě jednou")
  server.on("/skip", HTTP_POST, [](AsyncWebServerRequest *request){
    String resp;
    handleSkip(resp);
    request->send(200, "application/json", resp);
  });

  // upload questions - textarea content param "questions" -> store only in RAM
  server.on("/uploadQuestions", HTTP_POST, [](AsyncWebServerRequest *request){
    String data = "";
    if (request->hasParam("questions", true)) {
      data = request->getParam("questions", true)->value();
    } else if (request->hasParam("questions", false)) {
      data = request->getParam("questions", false)->value();
    }
    if (data.length() == 0) {
      request->send(400, "application/json", "{\"status\":\"error\",\"message\":\"Žádný obsah\"}");
      return;
    }
    bool ok = parseQuestionsFromString(data);
    if (ok) {
      // DO NOT save to SPIFFS or persistent storage — questions live only in RAM
      currentQuestionIndex = 0;
      resetGameState();
      String resp = "{\"status\":\"ok\",\"count\":" + String(lastValidCount) + ",\"invalid\":" + String(lastInvalidCount) + "}";
      request->send(200, "application/json", resp);
    } else {
      String resp = "{\"status\":\"error\",\"message\":\"Chybný formát nebo žádné platné otázky\",\"valid\":" + String(lastValidCount) + ",\"invalid\":" + String(lastInvalidCount) + "}";
      request->send(400, "application/json", resp);
    }
  });

  // save wifi credentials (Preferences) and attempt to connect
  server.on("/setWiFi", HTTP_POST, [](AsyncWebServerRequest *request){
    String ssid = "";
    String pass = "";
    if (request->hasParam("ssid", true)) ssid = request->getParam("ssid", true)->value();
    if (request->hasParam("pass", true)) pass = request->getParam("pass", true)->value();
    savedSSID = ssid;
    savedPass = pass;
    // persist into NVS
    prefs.begin("wifi", false);
    prefs.putString("ssid", savedSSID);
    prefs.putString("pass", savedPass);
    prefs.end();

    if (savedSSID.length() > 0) {
      WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    }
    request->send(200, "application/json", "{\"status\":\"ok\"}");
  });

  // restart game endpoint (keeps questions in RAM)
  server.on("/restartGame", HTTP_POST, [](AsyncWebServerRequest *request){
    resetGameState();
    request->send(200, "application/json", "{\"status\":\"ok\"}");
  });

  server.begin();
}

// Simple responsive modern page (CSS inline for single-file)
String generateIndexHtml() {
  String html = R"rawliteral(
<!doctype html>
<html lang="cs">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Uteč!</title>
<style>
  :root{--bg:#0f1724;--card:#0b1220;--accent:#06b6d4;--muted:#94a3b8;--danger:#ef4444;--success:#10b981}
  body{margin:0;font-family:Inter,system-ui,-apple-system,Segoe UI,Roboto,"Helvetica Neue",Arial;background:linear-gradient(180deg,#041026 0%, #071733 100%);color:#e6eef6;min-height:100vh;display:flex;justify-content:center;align-items:flex-start;padding:28px;}
  .container{max-width:980px;width:100%}
  .card{background:rgba(255,255,255,0.02);border-radius:12px;padding:20px;margin-bottom:18px;box-shadow:0 6px 20px rgba(2,6,23,0.6)}
  h1{margin:0 0 8px 0;font-size:24px}
  .q{font-size:18px;margin:12px 0 18px 0;color:#dbeafe}
  .answers{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:10px;margin-bottom:10px}
  /* Zvýrazněná tlačítka */
  button.answer{padding:14px;border-radius:10px;border:0;background:linear-gradient(180deg,var(--accent),#0b86a0);color:#04202a;font-weight:700;font-size:16px;cursor:pointer;box-shadow:0 8px 20px rgba(6,182,212,0.16);transition:transform .12s ease, box-shadow .12s ease}
  button.answer:hover{transform:translateY(-3px);box-shadow:0 12px 30px rgba(6,182,212,0.22)}
  button.ghost{background:linear-gradient(180deg,#ff7f7f,#ff4b4b);color:white}
  button.green{background:linear-gradient(180deg,#7ce3b8,#1fb77a);color:#04202a}
  .controls{display:flex;gap:10px;flex-wrap:wrap;margin-bottom:10px}
  .muted{color:var(--muted);font-size:13px}
  .form-row{display:flex;gap:10px;flex-wrap:wrap}
  input[type=text], input[type=password], textarea{width:100%;padding:10px;border-radius:8px;border:1px solid rgba(255,255,255,0.06);background:transparent;color:inherit}
  .small{text-transform:uppercase;font-size:12px;color:var(--muted);letter-spacing:0.6px}
  .grid{display:grid;grid-template-columns:1fr 320px;gap:14px}
  @media(max-width:900px){.grid{grid-template-columns:1fr}}
  .status {font-size:14px;padding:8px;border-radius:8px;background:rgba(255,255,255,0.02)}
  .led-preview{height:28px;border-radius:8px;background:linear-gradient(90deg,#00121e,#002233);display:flex;align-items:center;padding:6px;overflow:hidden}
  .field-pill{height:16px;min-width:16px;border-radius:8px;margin-right:6px;flex:0 0 auto}
  .danger{background:linear-gradient(90deg,var(--danger),#b91c1c)}
  .success{background:linear-gradient(90deg,var(--success),#059669)}
  .ghost{background:linear-gradient(90deg,#7f1d1d,#4c0000)}
  .player{background:linear-gradient(90deg,#064e3b,#10b981)}
  .big-restart{display:block;margin-top:14px;padding:12px;border-radius:10px;border:0;background:linear-gradient(180deg,#ffb86b,#ff8a00);color:#2b1400;font-weight:800;box-shadow:0 10px 30px rgba(255,138,0,0.2)}
</style>
</head>
<body>
<div class="container">
  <div class="card">
    <h1>Uteč!</h1>
    <div class="q" id="question">Načítám otázku…</div>
    <div class="answers" id="answers">
      <!-- buttons inserted by JS -->
    </div>
    <div class="controls">
      <button id="skipBtn" class="answer">Ještě jednou (max 2x)</button>
      <button id="restartBtnTop" class="answer green">Restart hry</button>
      <div class="status" id="status">Hráč: pole <span id="playerField">3</span> — Duch: pole <span id="ghostField">1</span> — Kolo: <span id="turnCounter">0</span></div>
    </div>
  </div>

  <div class="grid">
    <div class="card">
      <div class="small">Nahrát otázky</div>
      <p class="muted">Formát: Číslo otázka; Otázka; Odpověď 1, Odpověď 2, Odpověď 3, Odpověď 4; Index správné odpovědi (1..4)</p>
      <textarea id="questionsText" rows="8" placeholder="1; Kolik je 2+2?; 3,4,5,6;2"></textarea>
      <div style="display:flex;gap:8px;margin-top:8px">
        <button id="uploadBtn" class="answer">Nahrát otázky</button>
        <div id="uploadResult" class="muted"></div>
      </div>

      <hr style="margin:12px 0;border:none;border-top:1px solid rgba(255,255,255,0.03)">

      <div class="small">Připojení k Wi-Fi</div>
      <input id="ssid" type="text" placeholder="SSID">
      <input id="pass" type="password" placeholder="Heslo">
      <div style="display:flex;gap:8px;margin-top:8px">
        <button id="setWifi" class="answer">Uložit Wi-Fi a připojit</button>
        <div id="wifiResult" class="muted"></div>
      </div>
    </div>

    <div class="card">
      <div class="small">Náhled polí (10 LED = 1 pole)</div>
      <div class="led-preview" id="ledPreview">
        <!-- pills -->
      </div>
      <div style="height:12px"></div>
      <div class="small">Průběh hry</div>
      <div id="log" style="max-height:240px;overflow:auto;margin-top:8px;font-size:13px;color:var(--muted)"></div>

      <!-- prominent restart at the end of page -->
      <button id="restartBtnBottom" class="big-restart">Restartovat hru</button>

    </div>
  </div>

</div>

<script>
let skipsLeft = 2;
async function fetchState(){
  const res = await fetch('/state');
  if (!res.ok) return;
  const js = await res.json();
  document.getElementById('question').innerText = js.question || "Žádná otázka";
  // answers
  const answers = js.answers || [];
  const answersDiv = document.getElementById('answers');
  answersDiv.innerHTML = '';
  for (let i=0;i<4;i++){
    const btn = document.createElement('button');
    btn.className = 'answer';
    btn.innerText = answers[i] || ('Odpověď '+(i+1));
    btn.onclick = () => submitAnswer(i+1);
    answersDiv.appendChild(btn);
  }
  document.getElementById('playerField').innerText = js.playerField;
  document.getElementById('ghostField').innerText = js.ghostField;
  document.getElementById('turnCounter').innerText = js.turnCounter;
  skipsLeft = js.skipsLeft;
  document.getElementById('skipBtn').innerText = 'Ještě jednou (zbyvá ' + skipsLeft + ')';
  // led preview
  renderPreview(js.playerField, js.ghostField, js.fields);
  if (js.gameOver) {
    const log = document.getElementById('log');
    let msg = '';
    if (js.result === 'player_win') msg = 'Vyhrál hráč!';
    else msg = 'Vyhrál duch!';
    log.innerHTML = '<div style="color:lightgreen">'+msg+'</div>' + log.innerHTML;
  }
}

function renderPreview(player, ghost, fields){
  const box = document.getElementById('ledPreview');
  box.innerHTML = '';
  for (let f=1; f<=fields; f++){
    const pill = document.createElement('div');
    pill.className = 'field-pill';
    pill.style.width = (100/fields) + '%';
    pill.style.flexBasis = (100/fields) + '%';
    pill.style.marginRight = '3px';
    pill.style.height = '12px';
    if (f === player) pill.style.background = 'linear-gradient(90deg,#064e3b,#10b981)';
    else if (f === ghost) pill.style.background = 'linear-gradient(90deg,#7f1d1d,#4c0000)';
    else pill.style.background = 'rgba(255,255,255,0.02)';
    box.appendChild(pill);
  }
}

async function submitAnswer(idx){
  const res = await fetch('/answer', {method:'POST', headers:{'Content-Type':'application/x-www-form-urlencoded'}, body:'index='+idx});
  const js = await res.json();
  const log = document.getElementById('log');
  if (js.status === 'correct') {
    log.innerHTML = '<div>Odpověď ' + idx + ' - správně! Hráč se posouvá.</div>' + log.innerHTML;
  } else if (js.status === 'wrong') {
    log.innerHTML = '<div>Odpověď ' + idx + ' - špatně! Duch se posunul.</div>' + log.innerHTML;
  } else {
    log.innerHTML = '<div style="color:orange">Chyba: ' + (js.message || JSON.stringify(js)) + '</div>' + log.innerHTML;
  }
  await fetchState();
}

document.getElementById('skipBtn').addEventListener('click', async ()=>{
  if (skipsLeft <= 0) {
    alert('Už nemůžeš použít Ještě jednou.');
    return;
  }
  const res = await fetch('/skip', {method:'POST'});
  const js = await res.json();
  if (js.status === 'ok' || js.skipsUsed !== undefined) {
    document.getElementById('log').innerHTML = '<div>Ještě jednou použito. Nová otázka.</div>' + document.getElementById('log').innerHTML;
  } else {
    document.getElementById('log').innerHTML = '<div style="color:orange">Nepodařilo se provést změnu otázky.</div>' + document.getElementById('log').innerHTML;
  }
  await fetchState();
});

document.getElementById('uploadBtn').addEventListener('click', async ()=>{
  const qs = document.getElementById('questionsText').value;
  if (!qs || qs.trim().length===0) { alert('Vlož text s otázkami.'); return; }
  const body = new URLSearchParams();
  body.append('questions', qs);
  const res = await fetch('/uploadQuestions', {method:'POST', body: body});
  const js = await res.json();
  if (res.ok) {
    document.getElementById('uploadResult').innerText = 'Nahráno: ' + js.count + ' otázek (' + (js.invalid||0) + ' chybných řádků)';
    document.getElementById('log').innerHTML = '<div>Nahrány otázky ('+js.count+'), chybných: '+(js.invalid||0)+'</div>' + document.getElementById('log').innerHTML;
    await fetchState();
  } else {
    document.getElementById('uploadResult').innerText = 'Chyba nahrání';
    alert('Chybný formát otázek nebo jiná chyba. Více: ' + (js.message||'unknown'));
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

// Restart handlers (top and bottom buttons)
document.getElementById('restartBtnTop').addEventListener('click', async ()=>{
  if (!confirm('Opravdu restartovat hru?')) return;
  const res = await fetch('/restartGame', {method:'POST'});
  if (res.ok) {
    document.getElementById('log').innerHTML = '<div>Hra restartována</div>' + document.getElementById('log').innerHTML;
    await fetchState();
  }
});

document.getElementById('restartBtnBottom').addEventListener('click', async ()=>{
  if (!confirm('Opravdu restartovat hru?')) return;
  const res = await fetch('/restartGame', {method:'POST'});
  if (res.ok) {
    document.getElementById('log').innerHTML = '<div>Hra restartována</div>' + document.getElementById('log').innerHTML;
    await fetchState();
  }
});

// regularly poll state
setInterval(fetchState, 1200);
fetchState();
</script>
</body>
</html>
)rawliteral";

  return html;
}

// ------------------ setup & loop ------------------

void setup() {
  Serial.begin(115200);
  delay(1000);
  strip.begin();
  strip.show();

  // Read saved WiFi credentials from Preferences (NVS)
  prefs.begin("wifi", true); // read-only
  savedSSID = prefs.getString("ssid", "");
  savedPass = prefs.getString("pass", "");
  prefs.end();

  // Start AP first (AP available always as fallback)
  WiFi.softAP(apSsid, apPass);
  IPAddress ip = WiFi.softAPIP();
  Serial.printf("AP started. IP: %s\n", ip.toString().c_str());

  // If we have saved credentials, try to connect (but keep AP running)
  if (savedSSID.length() > 0) {
    Serial.printf("Attempting to connect to saved SSID: %s\n", savedSSID.c_str());
    WiFi.begin(savedSSID.c_str(), savedPass.c_str());
    unsigned long start = millis();
    const unsigned long timeout = 8000; // 8 seconds to get connected
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < timeout) {
      delay(250);
    }
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("Connected to WiFi as station. IP: ");
      Serial.println(WiFi.localIP());
    } else {
      Serial.println("Failed to connect to saved WiFi - staying as AP (fallback)");
    }
  }

  // start web server
  setupWebServer();

  // initial question (in RAM). Do NOT load from SPIFFS - questions are RAM-only
  if (questions.size() > 0) {
    currentQuestionIndex = 0;
  } else {
    // add sample question
    Question q;
    q.id = "1";
    q.text = "Kolik je 2+2?";
    q.answers[0] = "3";
    q.answers[1] = "4";
    q.answers[2] = "5";
    q.answers[3] = "6";
    q.correctIndex = 2;
    questions.push_back(q);
  }

  resetGameState();
  Serial.println("Setup done");
}

void loop() {
  // small housekeeping: if savedSSID provided and not connected, try to reconnect periodically
  static unsigned long lastConnAttempt = 0;
  if (savedSSID.length() > 0 && WiFi.status() != WL_CONNECTED) {
    unsigned long now = millis();
    if (now - lastConnAttempt > 15000) {
      Serial.println("Trying to connect to station WiFi...");
      WiFi.begin(savedSSID.c_str(), savedPass.c_str());
      lastConnAttempt = now;
    }
  }

  // draw periodically (in case)
  static unsigned long lastDraw = 0;
  if (millis() - lastDraw > 500) {
    drawFields();
    lastDraw = millis();
  }

  delay(10);
}
