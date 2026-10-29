#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <ArduinoOTA.h>
#include <Update.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "esp_wifi.h"
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

const byte DNS_PORT = 53;
DNSServer dnsServer;

// ==========================================
// HARDWARE SETTINGS (ESP32-C3 SuperMini)
// ==========================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

// Pins as per SuperMini Pinout (D4 = GPIO6, D5 = GPIO7)
#define I2C_SDA 6
#define I2C_SCL 7
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ==========================================
// WI-FI ACCESS POINT CREDENTIALS
// ==========================================
const char *AP_SSID = "my bot";
const char *AP_PASS = "88888888";

WebServer server(80);

// Current active face mode
String currentFace = "idle";
unsigned long lastAnimUpdate = 0;
int animFrame = 0;

// Wi-Fi Battery Saver: Auto-off after 1 minute IF NO CLIENT IS CONNECTED
bool wifiEnabled = true;
unsigned long wifiOffTime = 60000; // 60 seconds from boot

// ==========================================
// HTML & WEB INTERFACE (Mobile-First Cyberpunk UI)
// ==========================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>MY BOT - OLED Emotion Controller</title>
  <style>
    :root {
      --bg: #090d16;
      --card-bg: rgba(255, 255, 255, 0.05);
      --card-border: rgba(255, 255, 255, 0.1);
      --accent: #6366f1;
      --accent-glow: rgba(99, 102, 241, 0.4);
      --cyan: #06b6d4;
      --pink: #ec4899;
      --text: #f8fafc;
      --text-dim: #94a3b8;
    }
    * {
      box-sizing: border-box;
      margin: 0;
      padding: 0;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif;
      -webkit-tap-highlight-color: transparent;
    }
    body {
      background: var(--bg);
      background-image: 
        radial-gradient(at 0% 0%, rgba(99, 102, 241, 0.18) 0px, transparent 50%),
        radial-gradient(at 100% 100%, rgba(236, 72, 153, 0.15) 0px, transparent 50%);
      color: var(--text);
      min-height: 100vh;
      padding: 18px 14px 40px;
    }
    header {
      text-align: center;
      margin-bottom: 22px;
    }
    .badge {
      display: inline-flex;
      align-items: center;
      gap: 6px;
      background: rgba(99, 102, 241, 0.15);
      border: 1px solid rgba(99, 102, 241, 0.35);
      color: #a5b4fc;
      padding: 4px 12px;
      border-radius: 999px;
      font-size: 11px;
      font-weight: 600;
      letter-spacing: 0.5px;
      margin-bottom: 8px;
    }
    .pulse-dot {
      width: 7px;
      height: 7px;
      background: #10b981;
      border-radius: 50%;
      box-shadow: 0 0 8px #10b981;
      animation: pulse 1.5s infinite;
    }
    @keyframes pulse {
      0%, 100% { opacity: 1; transform: scale(1); }
      50% { opacity: 0.4; transform: scale(0.85); }
    }
    h1 {
      font-size: 26px;
      font-weight: 800;
      background: linear-gradient(135deg, #fff 30%, #a5b4fc 100%);
      -webkit-background-clip: text;
      -webkit-text-fill-color: transparent;
      margin-bottom: 4px;
    }
    .subtitle {
      color: var(--text-dim);
      font-size: 13px;
    }
    .active-bar {
      background: rgba(15, 23, 42, 0.7);
      border: 1px solid var(--card-border);
      backdrop-filter: blur(10px);
      border-radius: 14px;
      padding: 12px 16px;
      margin-bottom: 20px;
      display: flex;
      justify-content: space-between;
      align-items: center;
    }
    .active-bar span {
      font-size: 12px;
      color: var(--text-dim);
    }
    .active-pill {
      font-size: 13px;
      font-weight: 700;
      color: #38bdf8;
      text-transform: uppercase;
      letter-spacing: 0.5px;
    }
    .grid {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 12px;
      max-width: 500px;
      margin: 0 auto;
    }
    @media (min-width: 480px) {
      .grid {
        grid-template-columns: repeat(3, 1fr);
      }
    }
    .card {
      background: var(--card-bg);
      border: 1px solid var(--card-border);
      backdrop-filter: blur(12px);
      border-radius: 16px;
      padding: 16px 12px;
      text-align: center;
      cursor: pointer;
      transition: all 0.2s cubic-bezier(0.4, 0, 0.2, 1);
      position: relative;
      overflow: hidden;
      user-select: none;
    }
    .card:active {
      transform: scale(0.95);
    }
    .card.selected {
      background: linear-gradient(135deg, rgba(99, 102, 241, 0.25), rgba(236, 72, 153, 0.2));
      border-color: #818cf8;
      box-shadow: 0 0 20px var(--accent-glow);
    }
    .card.selected::before {
      content: "";
      position: absolute;
      top: 6px;
      right: 6px;
      width: 8px;
      height: 8px;
      background: #38bdf8;
      border-radius: 50%;
      box-shadow: 0 0 6px #38bdf8;
    }
    .emoji {
      font-size: 34px;
      display: block;
      margin-bottom: 8px;
      transition: transform 0.25s ease;
    }
    .card:hover .emoji {
      transform: scale(1.15);
    }
    .title {
      font-size: 14px;
      font-weight: 700;
      color: var(--text);
      margin-bottom: 2px;
    }
    .desc {
      font-size: 11px;
      color: var(--text-dim);
    }
    .test-card {
      grid-column: 1 / -1;
      background: linear-gradient(135deg, rgba(16, 185, 129, 0.15), rgba(6, 182, 212, 0.15));
      border-color: rgba(16, 185, 129, 0.35);
      margin-top: 6px;
      display: flex;
      align-items: center;
      justify-content: center;
      gap: 14px;
      padding: 14px 18px;
      text-align: left;
    }
    .test-card.selected {
      border-color: #34d399;
      box-shadow: 0 0 20px rgba(52, 211, 153, 0.4);
    }
    .test-card .emoji {
      margin: 0;
      font-size: 28px;
    }
    footer {
      text-align: center;
      margin-top: 24px;
      font-size: 11px;
      color: var(--text-dim);
    }
  </style>
</head>
<body>

  <header>
    <div class="badge">
      <div class="pulse-dot"></div>
      WIFI AP: "my bot" (Auto-Off: 60s)
    </div>
    <h1>MY BOT OLED</h1>
    <p class="subtitle">Tap any emotion to project it live on OLED</p>
    <div style="margin-top: 8px;">
      <button onclick="if(confirm('Turn off Wi-Fi now to save battery?')){fetch('/sleep');alert('Wi-Fi is turning OFF! Device will now run in low-power battery mode.');}" style="background: rgba(16, 185, 129, 0.15); border: 1px solid rgba(16, 185, 129, 0.4); color: #34d399; padding: 6px 14px; border-radius: 20px; font-size: 11px; font-weight: 600; cursor: pointer;">
        🔋 Turn Off Wi-Fi Now (Save ~85% Battery)
      </button>
    </div>
  </header>

  <div class="active-bar">
    <span>CURRENT MOOD:</span>
    <span class="active-pill" id="currentLabel">IDLE (BLOCKED)</span>
  </div>

  <div class="grid">
    <!-- 1. Normal / Idle -->
    <div class="card selected" onclick="setFace('idle', 'Normal / Blink', this)">
      <span class="emoji">👀</span>
      <div class="title">Normal</div>
      <div class="desc">Cute Blinking</div>
    </div>

    <!-- 2. Happy -->
    <div class="card" onclick="setFace('happy', 'Happy / Smile', this)">
      <span class="emoji">😊</span>
      <div class="title">Happy</div>
      <div class="desc">Big Joyful Smile</div>
    </div>

    <!-- 3. Love / Heart Eyes -->
    <div class="card" onclick="setFace('love', 'Love / Heart Eyes', this)">
      <span class="emoji">😍</span>
      <div class="title">Love</div>
      <div class="desc">Pulsing Hearts</div>
    </div>

    <!-- 4. Wink -->
    <div class="card" onclick="setFace('wink', 'Wink / Naughty', this)">
      <span class="emoji">😉</span>
      <div class="title">Wink</div>
      <div class="desc">Playful Wink</div>
    </div>

    <!-- 5. Cool / Sunglasses -->
    <div class="card" onclick="setFace('cool', 'Cool / Sunglasses', this)">
      <span class="emoji">😎</span>
      <div class="title">Cool</div>
      <div class="desc">Dark Shades</div>
    </div>

    <!-- 6. Sleepy -->
    <div class="card" onclick="setFace('sleepy', 'Sleepy / Zzz', this)">
      <span class="emoji">😴</span>
      <div class="title">Sleepy</div>
      <div class="desc">Sleeping Zzz</div>
    </div>

    <!-- 7. Angry -->
    <div class="card" onclick="setFace('angry', 'Angry / Gussa', this)">
      <span class="emoji">😡</span>
      <div class="title">Angry</div>
      <div class="desc">Fierce Eyes</div>
    </div>

    <!-- 8. Sad -->
    <div class="card" onclick="setFace('sad', 'Sad / Udas', this)">
      <span class="emoji">😢</span>
      <div class="title">Sad</div>
      <div class="desc">Droopy Tear</div>
    </div>

    <!-- 9. Surprised -->
    <div class="card" onclick="setFace('surprised', 'Shocked / Wow', this)">
      <span class="emoji">😲</span>
      <div class="title">Shocked</div>
      <div class="desc">Huge Eyes</div>
    </div>

    <!-- 10. Laughing -->
    <div class="card" onclick="setFace('laughing', 'Laughing / XD', this)">
      <span class="emoji">😆</span>
      <div class="title">Laughing</div>
      <div class="desc">XD Giggle</div>
    </div>

    <!-- 11. Confused -->
    <div class="card" onclick="setFace('confused', 'Confused / Thinking', this)">
      <span class="emoji">🤨</span>
      <div class="title">Confused</div>
      <div class="desc">Raised Eyebrow</div>
    </div>

    <!-- 12. Dizzy -->
    <div class="card" onclick="setFace('dizzy', 'Dizzy / Knockout', this)">
      <span class="emoji">😵</span>
      <div class="title">Dizzy</div>
      <div class="desc">XX Knockout</div>
    </div>

    <!-- 13. Happy Birthday Abhishek -->
    <div class="card" onclick="setFace('birthday', 'HBD Abhishek 🎂', this)" style="background: linear-gradient(135deg, rgba(255,215,0,0.15), rgba(236,72,153,0.2)); border-color: rgba(255,215,0,0.4);">
      <span class="emoji">🎂</span>
      <div class="title" style="color:#fbbf24;">Birthday</div>
      <div class="desc">HBD Abhishek!</div>
    </div>

    <!-- 14. Dead Pixel Diagnostic Test -->
    <div class="card test-card" onclick="setFace('pixel_test', 'Dead Pixel Test', this)">
      <span class="emoji">🧪</span>
      <div>
        <div class="title">Pixel Diagnostics Mode</div>
        <div class="desc">All Pixels White & Black Test to spot dead/stuck pixels</div>
      </div>
    </div>
    <!-- 14. Wireless Code Upload (OTA) -->
    <div class="card test-card" style="margin-top: 10px; background: linear-gradient(135deg, rgba(236, 72, 153, 0.15), rgba(99, 102, 241, 0.2)); border-color: rgba(236, 72, 153, 0.4); flex-direction: column; align-items: stretch; text-align: left;">
      <div style="display:flex; align-items:center; gap: 10px; margin-bottom: 8px;">
        <span class="emoji" style="margin:0; font-size:24px;">🚀</span>
        <div>
          <div class="title" style="color:#f472b6;">Wireless Code Upload (OTA)</div>
          <div class="desc">Bina cable ke direct naya .bin firmware upload karein</div>
        </div>
      </div>
      <form id="otaForm" style="display:flex; flex-direction:column; gap:8px;">
        <input type="file" id="otaFile" accept=".bin" style="font-size:12px; color:var(--text-dim);" required>
        <button type="submit" id="otaBtn" style="background:linear-gradient(135deg, #6366f1, #ec4899); color:#fff; border:none; padding:10px; border-radius:10px; font-weight:700; font-size:13px; cursor:pointer;">⚡ Flash New Firmware (.bin)</button>
        <div id="progCont" style="display:none; width:100%; height:8px; background:rgba(255,255,255,0.1); border-radius:4px; overflow:hidden;">
          <div id="progBar" style="width:0%; height:100%; background:#10b981; transition:width 0.2s;"></div>
        </div>
        <div id="otaStatus" style="font-size:11px; color:#38bdf8; text-align:center;"></div>
      </form>
    </div>
  </div>

  <footer>
    ESP32-C3 SuperMini • 0.96" SSD1306 (128x64) • OTA Enabled
  </footer>

  <script>
    function setFace(face, label, el) {
      if (navigator.vibrate) navigator.vibrate(30);

      document.querySelectorAll('.card').forEach(c => c.classList.remove('selected'));
      el.classList.add('selected');
      document.getElementById('currentLabel').innerText = label;

      fetch('/set?face=' + face).catch(err => console.log(err));
    }

    document.getElementById('otaForm').onsubmit = function(e) {
      e.preventDefault();
      var fin = document.getElementById('otaFile');
      if (!fin.files.length) return;
      var f = fin.files[0];
      var fd = new FormData();
      fd.append('update', f);
      var xhr = new XMLHttpRequest();
      document.getElementById('progCont').style.display = 'block';
      document.getElementById('otaBtn').disabled = true;
      document.getElementById('otaBtn').innerText = 'Uploading...';
      xhr.upload.addEventListener('progress', function(e) {
        if (e.lengthComputable) {
          var p = Math.round((e.loaded / e.total) * 100);
          document.getElementById('progBar').style.width = p + '%';
          document.getElementById('otaStatus').innerText = p + '% Uploaded - Please wait...';
        }
      });
      xhr.onload = function() {
        if (xhr.status === 200) {
          document.getElementById('otaStatus').innerText = 'Upload Successful! ESP32 Rebooting in 5s...';
          setTimeout(function() { location.reload(); }, 6000);
        } else {
          document.getElementById('otaStatus').innerText = 'Upload Error: ' + xhr.responseText;
          document.getElementById('otaBtn').disabled = false;
          document.getElementById('otaBtn').innerText = '⚡ Retry Upload';
        }
      };
      xhr.open('POST', '/update');
      xhr.send(fd);
    };
  </script>
</body>
</html>
)rawliteral";

// ==========================================
// OLED FACE DRAWING FUNCTIONS
// ==========================================

// Helper: Draw Smooth Heart
void drawHeart(int cx, int cy, int size) {
  int r = size / 2;
  display.fillCircle(cx - r / 2, cy - r / 3, r / 2, SSD1306_WHITE);
  display.fillCircle(cx + r / 2, cy - r / 3, r / 2, SSD1306_WHITE);
  display.fillTriangle(cx - r, cy - r / 6, cx + r, cy - r / 6, cx,
                       cy + size / 2, SSD1306_WHITE);
}

// 1. Normal / Idle with automatic natural blink
void drawIdleFace(int frame) {
  display.clearDisplay();
  bool isBlinking = (frame % 30 >= 27); // Blink every ~3 seconds for 3 frames

  if (isBlinking) {
    // Eyelid slit
    display.fillRoundRect(24, 28, 28, 6, 3, SSD1306_WHITE);
    display.fillRoundRect(76, 28, 28, 6, 3, SSD1306_WHITE);
  } else {
    // Cute Big Eyes
    display.fillRoundRect(24, 14, 28, 34, 8, SSD1306_WHITE);
    display.fillRoundRect(76, 14, 28, 34, 8, SSD1306_WHITE);

    // Eye glint (white pupils/highlights)
    display.fillCircle(30, 20, 3, SSD1306_BLACK);
    display.fillCircle(82, 20, 3, SSD1306_BLACK);
  }

  // Small cute mouth
  display.drawFastHLine(60, 54, 8, SSD1306_WHITE);
}

// 2. Happy Face (Anime style ^ ^ smile)
void drawHappyFace(int frame) {
  display.clearDisplay();
  int bounce = (frame % 4 < 2) ? 0 : 2;

  // Squinting smiling arch eyes
  for (int t = 0; t < 5; t++) {
    // Left eye arch
    display.drawCircle(38, 34 + bounce + t, 16, SSD1306_WHITE);
    // Right eye arch
    display.drawCircle(90, 34 + bounce + t, 16, SSD1306_WHITE);
  }
  // Clear lower half of the circles so they remain curved arches
  display.fillRect(15, 34 + bounce, 100, 20, SSD1306_BLACK);

  // Big joyful open smile
  display.fillCircle(64, 46 + bounce, 10, SSD1306_WHITE);
  display.fillRect(52, 42 + bounce, 24, 6, SSD1306_BLACK);

  // Cute blush marks
  display.drawFastHLine(18, 40, 6, SSD1306_WHITE);
  display.drawFastHLine(104, 40, 6, SSD1306_WHITE);
}

// 3. Love / Heart Eyes (Pulsing animation)
void drawLoveFace(int frame) {
  display.clearDisplay();
  int pulse = (frame % 6 < 3) ? 22 : 26; // Heart beats

  drawHeart(38, 28, pulse);
  drawHeart(90, 28, pulse);

  // Cute open smile
  display.drawCircle(64, 48, 8, SSD1306_WHITE);
  display.fillRect(54, 40, 20, 8, SSD1306_BLACK);
}

// 4. Playful Wink
void drawWinkFace(int frame) {
  display.clearDisplay();
  // Left Eye: Wide open cute rounded eye
  display.fillRoundRect(24, 14, 28, 34, 8, SSD1306_WHITE);
  display.fillCircle(30, 20, 3, SSD1306_BLACK);

  // Right Eye: Winking closed slit with lash
  display.fillRoundRect(76, 28, 28, 6, 3, SSD1306_WHITE);
  display.drawLine(104, 28, 108, 24, SSD1306_WHITE);
  display.drawLine(104, 32, 108, 36, SSD1306_WHITE);

  // Smirk Smile
  display.drawCircle(68, 48, 10, SSD1306_WHITE);
  display.fillRect(56, 40, 24, 9, SSD1306_BLACK);
}

// 5. Cool Sunglasses
void drawCoolFace(int frame) {
  display.clearDisplay();
  int glintOffset = (frame % 16) * 4;

  // Dark sunglasses frame
  display.fillRoundRect(20, 16, 38, 24, 4, SSD1306_WHITE);
  display.fillRoundRect(70, 16, 38, 24, 4, SSD1306_WHITE);
  // Bridge
  display.fillRect(58, 22, 12, 5, SSD1306_WHITE);

  // Cool white reflection glint moving
  int gx1 = 22 + (glintOffset % 32);
  display.drawLine(gx1, 38, gx1 + 6, 18, SSD1306_BLACK);
  int gx2 = 72 + (glintOffset % 32);
  display.drawLine(gx2, 38, gx2 + 6, 18, SSD1306_BLACK);

  // Smug smirk mouth
  display.drawLine(60, 52, 76, 50, SSD1306_WHITE);
  display.drawLine(76, 50, 78, 46, SSD1306_WHITE);
}

// 6. Sleepy / Zzz
void drawSleepyFace(int frame) {
  display.clearDisplay();

  // Sleeping closed droopy eye slits
  display.fillRoundRect(22, 30, 28, 5, 2, SSD1306_WHITE);
  display.fillRoundRect(74, 30, 28, 5, 2, SSD1306_WHITE);

  // Drifting "Zzz"
  int zStep = frame % 20;
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(95 + (zStep / 3), 18 - (zStep / 2));
  display.print("z");
  display.setTextSize(2);
  display.setCursor(105 + (zStep / 4), 10 - zStep);
  display.print("Z");

  // Tiny relaxed mouth (o)
  display.drawCircle(62, 48, 3, SSD1306_WHITE);
}

// 7. Angry / Gussa
void drawAngryFace(int frame) {
  display.clearDisplay();
  int shake = (frame % 2 == 0) ? 1 : -1;

  // Left Eye: Slanted fiercely downwards to center
  display.fillRoundRect(24 + shake, 18, 28, 28, 4, SSD1306_WHITE);
  display.fillTriangle(24 + shake, 18, 52 + shake, 18, 52 + shake, 30,
                       SSD1306_BLACK);

  // Right Eye: Slanted fiercely downwards to center
  display.fillRoundRect(76 + shake, 18, 28, 28, 4, SSD1306_WHITE);
  display.fillTriangle(76 + shake, 18, 104 + shake, 18, 76 + shake, 30,
                       SSD1306_BLACK);

  // Angry jagged mouth
  display.drawLine(52, 54, 60, 48, SSD1306_WHITE);
  display.drawLine(60, 48, 68, 54, SSD1306_WHITE);
  display.drawLine(68, 54, 76, 48, SSD1306_WHITE);
}

// 8. Sad / Crying with falling tear
void drawSadFace(int frame) {
  display.clearDisplay();

  // Droopy sad eyes (slanting outwards)
  display.fillRoundRect(24, 20, 28, 26, 4, SSD1306_WHITE);
  display.fillTriangle(24, 20, 52, 20, 24, 30, SSD1306_BLACK);

  display.fillRoundRect(76, 20, 28, 26, 4, SSD1306_WHITE);
  display.fillTriangle(76, 20, 104, 20, 104, 30, SSD1306_BLACK);

  // Tear falling from right eye
  int tearY = 36 + (frame % 14) * 2;
  display.fillCircle(92, tearY, 2, SSD1306_WHITE);
  display.drawPixel(92, tearY - 3, SSD1306_WHITE);

  // Sad curved down mouth
  display.drawCircle(64, 58, 8, SSD1306_WHITE);
  display.fillRect(52, 58, 24, 9, SSD1306_BLACK);
}

// 9. Shocked / Surprised
void drawSurprisedFace(int frame) {
  display.clearDisplay();
  int widen = (frame % 4 < 2) ? 0 : 2;

  // Huge wide vertical round eyes
  display.fillRoundRect(24 - widen, 10, 28 + (widen * 2), 38, 10,
                        SSD1306_WHITE);
  display.fillRoundRect(76 - widen, 10, 28 + (widen * 2), 38, 10,
                        SSD1306_WHITE);

  // Inner black pupils looking wide
  display.fillCircle(38, 28, 4, SSD1306_BLACK);
  display.fillCircle(90, 28, 4, SSD1306_BLACK);

  // Huge 'O' mouth
  display.fillCircle(64, 52, 6, SSD1306_WHITE);
  display.fillCircle(64, 52, 4, SSD1306_BLACK);
}

// 10. Laughing / XD
void drawLaughingFace(int frame) {
  display.clearDisplay();
  int shake = (frame % 2 == 0) ? 0 : 2;

  // Left Eye: '>'
  for (int i = 0; i < 4; i++) {
    display.drawLine(24, 20 + i, 44, 30 + i, SSD1306_WHITE);
    display.drawLine(44, 30 + i, 24, 40 + i, SSD1306_WHITE);
  }

  // Right Eye: '<'
  for (int i = 0; i < 4; i++) {
    display.drawLine(104, 20 + i, 84, 30 + i, SSD1306_WHITE);
    display.drawLine(84, 30 + i, 104, 40 + i, SSD1306_WHITE);
  }

  // Wide grinning laughing mouth
  display.fillCircle(64, 46 + shake, 12, SSD1306_WHITE);
  display.fillRect(50, 40 + shake, 28, 8, SSD1306_BLACK);
}

// 11. Confused (One eye up, one eye small)
void drawConfusedFace(int frame) {
  display.clearDisplay();

  // Left Eye: Normal round eye
  display.fillRoundRect(24, 18, 28, 30, 6, SSD1306_WHITE);
  display.fillCircle(38, 32, 4, SSD1306_BLACK);

  // Right Eye: Raised high, smaller & squinted
  display.fillRoundRect(78, 10, 24, 20, 4, SSD1306_WHITE);
  display.fillCircle(90, 18, 3, SSD1306_BLACK);

  // Raised eyebrow on right
  display.drawFastHLine(76, 6, 28, SSD1306_WHITE);

  // Wavy squiggly mouth
  display.drawLine(52, 54, 60, 50, SSD1306_WHITE);
  display.drawLine(60, 50, 68, 56, SSD1306_WHITE);
  display.drawLine(68, 56, 76, 52, SSD1306_WHITE);
}

// 12. Dizzy (XX Knockout)
void drawDizzyFace(int frame) {
  display.clearDisplay();

  // Left 'X'
  for (int i = -2; i <= 2; i++) {
    display.drawLine(24 + i, 18, 50 + i, 42, SSD1306_WHITE);
    display.drawLine(50 + i, 18, 24 + i, 42, SSD1306_WHITE);
  }

  // Right 'X'
  for (int i = -2; i <= 2; i++) {
    display.drawLine(76 + i, 18, 102 + i, 42, SSD1306_WHITE);
    display.drawLine(102 + i, 18, 76 + i, 42, SSD1306_WHITE);
  }

  // Mouth with tongue out
  display.drawFastHLine(54, 50, 20, SSD1306_WHITE);
  display.fillRoundRect(62, 50, 8, 8, 3, SSD1306_WHITE);
}

// 13. Pixel Diagnostics (Full White & Checkerboard test)
void drawPixelTestMode(int frame) {
  int phase = (frame / 15) % 3; // Har phase ~1.5 sec chalega

  display.clearDisplay();
  if (phase == 0) {
    // Phase 1: All Pixels ON (White) -> Spot Dead Pixels
    display.fillRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, SSD1306_WHITE);
  } else if (phase == 1) {
    // Phase 2: All Pixels OFF (Black) -> Spot Stuck Pixels
    // Kept fully black
  } else {
    // Phase 3: Checkerboard Micro-Grid
    for (int y = 0; y < SCREEN_HEIGHT; y++) {
      for (int x = 0; x < SCREEN_WIDTH; x++) {
        if ((x + y) % 2 == 0)
          display.drawPixel(x, y, SSD1306_WHITE);
      }
    }
  }
}

// 14. Happy Birthday Abhishek - Full Celebration Animation
void drawBirthdayFace(int frame) {
  int phase = (frame / 25) % 5; // 5 phases, ~4.5 sec each
  display.clearDisplay();

  if (phase == 0) {
    // Phase 1: Birthday Cake with flickering candle
    // Cake base (3-tier)
    display.fillRoundRect(34, 48, 60, 12, 3, SSD1306_WHITE); // Bottom tier
    display.fillRoundRect(40, 38, 48, 12, 3, SSD1306_WHITE); // Middle tier
    display.fillRoundRect(48, 28, 32, 12, 3, SSD1306_WHITE); // Top tier

    // Frosting drips on each tier
    for (int i = 0; i < 5; i++) {
      display.drawPixel(38 + i * 12, 50, SSD1306_BLACK);
      display.drawPixel(44 + i * 8, 40, SSD1306_BLACK);
    }

    // Candle
    display.fillRect(62, 18, 4, 10, SSD1306_WHITE);

    // Flickering flame (animated)
    int flicker = (frame % 4);
    if (flicker == 0) {
      display.fillCircle(64, 14, 3, SSD1306_WHITE);
      display.drawPixel(64, 10, SSD1306_WHITE);
    } else if (flicker == 1) {
      display.fillCircle(63, 14, 3, SSD1306_WHITE);
      display.drawPixel(62, 11, SSD1306_WHITE);
      display.drawPixel(63, 10, SSD1306_WHITE);
    } else if (flicker == 2) {
      display.fillCircle(65, 14, 3, SSD1306_WHITE);
      display.drawPixel(66, 11, SSD1306_WHITE);
    } else {
      display.fillCircle(64, 13, 4, SSD1306_WHITE);
      display.drawPixel(64, 9, SSD1306_WHITE);
    }

    // Plate
    display.drawFastHLine(28, 60, 72, SSD1306_WHITE);

    // Sparkle stars around cake
    int sp = frame % 6;
    display.drawPixel(20 + sp * 2, 20 + sp, SSD1306_WHITE);
    display.drawPixel(100 - sp * 3, 15 + sp * 2, SSD1306_WHITE);
    display.drawPixel(25, 35 - sp, SSD1306_WHITE);
    display.drawPixel(103, 30 + sp, SSD1306_WHITE);

  } else if (phase == 1) {
    // Phase 2: Scrolling "HAPPY BIRTHDAY" text
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(2);
    int scrollX = 128 - (frame % 50) * 6;
    display.setCursor(scrollX, 8);
    display.print("HAPPY");
    display.setTextSize(1);
    display.setCursor(scrollX + 10, 30);
    display.print("BIRTHDAY!");

    // Small cake icon at bottom right
    display.fillRoundRect(90, 46, 28, 8, 2, SSD1306_WHITE);
    display.fillRoundRect(94, 40, 20, 8, 2, SSD1306_WHITE);
    display.fillRect(103, 34, 3, 6, SSD1306_WHITE);
    display.fillCircle(104, 31, 2, SSD1306_WHITE);

    // Confetti falling
    for (int i = 0; i < 8; i++) {
      int cx = (i * 17 + frame * 3) % 128;
      int cy = (i * 11 + frame * 2) % 64;
      display.drawPixel(cx, cy, SSD1306_WHITE);
      display.drawPixel(cx + 1, cy, SSD1306_WHITE);
    }

  } else if (phase == 2) {
    // Phase 3: Confetti burst + stars rain
    for (int i = 0; i < 20; i++) {
      int sx = (i * 7 + frame * 5) % 128;
      int sy = (i * 13 + frame * 3) % 64;
      // Draw tiny star
      display.drawPixel(sx, sy, SSD1306_WHITE);
      display.drawPixel(sx - 1, sy, SSD1306_WHITE);
      display.drawPixel(sx + 1, sy, SSD1306_WHITE);
      display.drawPixel(sx, sy - 1, SSD1306_WHITE);
      display.drawPixel(sx, sy + 1, SSD1306_WHITE);
    }

    // Center text pulsing
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    int pulseY = 24 + ((frame % 6 < 3) ? 0 : 2);
    display.setCursor(10, pulseY);
    display.print("** CELEBRATION **");

  } else if (phase == 3) {
    // Phase 4: "ABHISHEK" name with sparkle border
    display.setTextColor(SSD1306_WHITE);

    // Sparkle animated border
    for (int i = 0; i < 128; i += 4) {
      int offset = (frame + i) % 8;
      if (offset < 4) {
        display.drawPixel(i, 0, SSD1306_WHITE);
        display.drawPixel(i, 63, SSD1306_WHITE);
      }
    }
    for (int i = 0; i < 64; i += 4) {
      int offset = (frame + i) % 8;
      if (offset < 4) {
        display.drawPixel(0, i, SSD1306_WHITE);
        display.drawPixel(127, i, SSD1306_WHITE);
      }
    }

    // Name in big letters
    display.setTextSize(2);
    int nameX = 4;
    int bobY = 18 + ((frame % 4 < 2) ? 0 : 2);
    display.setCursor(nameX, bobY);
    display.print("ABHISHEK");

    // Heart below name
    drawHeart(64, 48, 16 + ((frame % 6 < 3) ? 0 : 4));

  } else {
    // Phase 5: Full celebration scene
    // Cake at center-bottom
    display.fillRoundRect(44, 42, 40, 10, 2, SSD1306_WHITE);
    display.fillRoundRect(50, 34, 28, 10, 2, SSD1306_WHITE);
    display.fillRect(62, 26, 4, 8, SSD1306_WHITE);
    // Flame
    int fl = frame % 3;
    display.fillCircle(64, 23 - fl, 2 + (fl == 1 ? 1 : 0), SSD1306_WHITE);

    // "HBD" text on top
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(8, 2);
    display.print("Happy Birthday");
    display.setCursor(28, 14);
    display.print("Abhishek!");

    // Confetti + sparkles everywhere
    for (int i = 0; i < 12; i++) {
      int px = (i * 11 + frame * 4) % 128;
      int py = (i * 7 + frame * 2) % 54;
      display.drawPixel(px, py, SSD1306_WHITE);
    }

    // Plate
    display.drawFastHLine(38, 53, 52, SSD1306_WHITE);

    // Bottom sparkle row
    for (int i = 0; i < 128; i += 6) {
      if ((frame + i) % 4 < 2)
        display.drawPixel(i, 58 + (i % 4), SSD1306_WHITE);
    }
  }
}

// ==========================================
// RENDER CURRENT FACE
// ==========================================
void renderCurrentFace() {
  if (currentFace == "idle") {
    drawIdleFace(animFrame);
  } else if (currentFace == "happy") {
    drawHappyFace(animFrame);
  } else if (currentFace == "love") {
    drawLoveFace(animFrame);
  } else if (currentFace == "wink") {
    drawWinkFace(animFrame);
  } else if (currentFace == "cool") {
    drawCoolFace(animFrame);
  } else if (currentFace == "sleepy") {
    drawSleepyFace(animFrame);
  } else if (currentFace == "angry") {
    drawAngryFace(animFrame);
  } else if (currentFace == "sad") {
    drawSadFace(animFrame);
  } else if (currentFace == "surprised") {
    drawSurprisedFace(animFrame);
  } else if (currentFace == "laughing") {
    drawLaughingFace(animFrame);
  } else if (currentFace == "confused") {
    drawConfusedFace(animFrame);
  } else if (currentFace == "dizzy") {
    drawDizzyFace(animFrame);
  } else if (currentFace == "pixel_test") {
    drawPixelTestMode(animFrame);
  } else if (currentFace == "birthday") {
    drawBirthdayFace(animFrame);
  }

  display.display();
}

// ==========================================
// WEB SERVER HANDLERS
// ==========================================
void handleRoot() { server.send(200, "text/html", INDEX_HTML); }

void handleSetFace() {
  if (server.hasArg("face")) {
    currentFace = server.arg("face");
    animFrame = 0;
    Serial.printf("[WEB] Emotion changed to: %s\n", currentFace.c_str());
    server.send(200, "text/plain", "OK: " + currentFace);
  } else {
    server.send(400, "text/plain", "Missing face parameter");
  }
}

// ==========================================
// SETUP & LOOP
// ==========================================
#define STATUS_LED 8 // ESP32-C3 SuperMini Onboard Blue LED
bool oledConnected = false;

// Function to shut down Wi-Fi completely after 1 minute to save battery
void disableWiFi() {
  if (!wifiEnabled) return;
  wifiEnabled = false;

  Serial.println("\n==========================================");
  Serial.println("[BATTERY SAVER] EXACT 1 MINUTE REACHED!");
  Serial.println("[BATTERY SAVER] Turning OFF Wi-Fi completely...");
  Serial.println("==========================================");

  // 1. Stop all web & network services
  dnsServer.stop();
  server.stop();
  ArduinoOTA.end();

  // 2. Disconnect SoftAP with true (sends deauth frame to all connected phones)
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);

  // 3. Disable AP mode and turn off RF radio synthesizer
  WiFi.enableAP(false);
  WiFi.mode(WIFI_OFF);
  esp_wifi_stop();

  // 4. Turn off onboard Blue LED (HIGH = OFF on SuperMini)
  digitalWrite(STATUS_LED, HIGH);

  // 5. Visual confirmation banner on OLED screen for 2 seconds
  if (oledConnected) {
    display.clearDisplay();
    display.drawRoundRect(4, 6, 120, 52, 6, SSD1306_WHITE);
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(38, 14);
    display.println("WIFI OFF");
    display.setCursor(8, 28);
    display.println("No Device Connected");
    display.setCursor(18, 42);
    display.println("Battery Mode ON");
    display.display();
    delay(2000);
  }

  Serial.println("[BATTERY SAVER] Wi-Fi is now completely OFF.");
  Serial.println("[BATTERY SAVER] To reconnect, power OFF and ON (restart) device.\n");
}

void setup() {
  // Disable hardware brownout detector to prevent voltage dips from crashing RF
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);

  Serial.begin(115200);
  delay(1000);
  Serial.println("\n==========================================");
  Serial.println("  MY BOT - ESP32-C3 SuperMini Stable AP   ");
  Serial.println("==========================================");

  // Status LED on
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW); // LOW = ON on SuperMini

  // 1. Full clean reset of Wi-Fi state
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  delay(100);

  // 2. Start SoftAP
  WiFi.mode(WIFI_AP);
  delay(50);
  bool apStarted = WiFi.softAP(AP_SSID, AP_PASS);
  delay(50);

  // CRITICAL HARDWARE FIX FOR ESP32-C3 SUPERMINI:
  // Reduce TX power to 8.5 dBm (raw value 34).
  // SuperMini ka tiny voltage regulator peak power (19.5dBm) par crash ho jata
  // tha, jisse Wi-Fi antenna radio switch off ho jata tha!
  esp_wifi_set_max_tx_power(34);
  Serial.println("[RF POWER] TX Power set to 8.5 dBm (SuperMini Stable Mode)");

  if (apStarted) {
    IPAddress IP = WiFi.softAPIP();
    Serial.println("[WIFI AP BROADCASTING]");
    Serial.printf("SSID: '%s'\n", AP_SSID);
    Serial.printf("Password: '%s'\n", AP_PASS);
    Serial.print("IP Address: ");
    Serial.println(IP);
  } else {
    Serial.println("[ERROR] Failed to start SoftAP!");
  }

  // 3. Captive Portal, Web Server & Web OTA
  IPAddress apIP = WiFi.softAPIP();
  dnsServer.start(DNS_PORT, "*", apIP);

  server.on("/", HTTP_GET, handleRoot);
  server.on("/set", HTTP_GET, handleSetFace);
  server.on("/sleep", HTTP_GET, []() {
    server.send(200, "text/plain", "OK: Turning off WiFi to save battery");
    delay(200);
    disableWiFi();
  });

  // Web Browser OTA Route (Bina kisi app ke direct browser se .bin upload)
  server.on("/update", HTTP_POST, []() {
    server.sendHeader("Connection", "close");
    server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
    delay(500);
    ESP.restart();
  }, []() {
    HTTPUpload& upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
      Serial.printf("[WEB OTA] Starting update: %s\n", upload.filename.c_str());
      if (oledConnected) {
        display.clearDisplay();
        display.setTextColor(SSD1306_WHITE);
        display.setTextSize(1);
        display.setCursor(14, 20);
        display.println("WEB OTA FLASHING");
        display.setCursor(20, 36);
        display.println("Please Wait...");
        display.display();
      }
      if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
      if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
        Update.printError(Serial);
      }
    } else if (upload.status == UPLOAD_FILE_END) {
      if (Update.end(true)) {
        Serial.printf("[WEB OTA] Flash Success: %u bytes\n", upload.totalSize);
        if (oledConnected) {
          display.clearDisplay();
          display.setCursor(18, 24);
          display.println("UPDATE SUCCESS!");
          display.setCursor(26, 40);
          display.println("REBOOTING...");
          display.display();
        }
      } else {
        Update.printError(Serial);
      }
    }
  });

  server.onNotFound([]() { server.send(200, "text/html", INDEX_HTML); });
  server.begin();
  Serial.println("[SERVER] Web Server started on port 80 with Web OTA!");

  // 4. ArduinoOTA Setup (PlatformIO direct wireless upload)
  ArduinoOTA.setHostname("mybot");
  ArduinoOTA.setPassword("88888888");
  ArduinoOTA.onStart([]() {
    Serial.println("[ARDUINO OTA] Start wireless flash...");
    if (oledConnected) {
      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);
      display.setTextSize(1);
      display.setCursor(18, 14);
      display.println("OTA FLASHING");
      display.display();
    }
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    int pct = progress / (total / 100);
    if (oledConnected) {
      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);
      display.setTextSize(1);
      display.setCursor(16, 12);
      display.println("OTA WIRELESS");
      display.drawRect(14, 28, 100, 12, SSD1306_WHITE);
      display.fillRect(16, 30, (pct * 96) / 100, 8, SSD1306_WHITE);
      display.setCursor(52, 46);
      display.printf("%d%%", pct);
      display.display();
    }
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("[ARDUINO OTA] Done!");
    if (oledConnected) {
      display.clearDisplay();
      display.setCursor(18, 24);
      display.println("UPDATE SUCCESS!");
      display.setCursor(26, 40);
      display.println("REBOOTING...");
      display.display();
    }
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("[ARDUINO OTA] Error: %u\n", error);
  });
  ArduinoOTA.begin();
  Serial.println("[OTA] ArduinoOTA service active on port 3232!");

  // 5. Initialize I2C with Fast 400kHz Clock (Saves CPU time for Wi-Fi!)
  Wire.begin(I2C_SDA, I2C_SCL);
  Wire.setClock(400000); // 400 kHz Fast I2C mode
  Wire.setTimeOut(30);

  Wire.beginTransmission(SCREEN_ADDRESS);
  if (Wire.endTransmission() == 0) {
    if (display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
      oledConnected = true;
      Serial.println("[OLED] Display connected at 0x3C!");
    }
  } else {
    Wire.beginTransmission(0x3D);
    if (Wire.endTransmission() == 0) {
      if (display.begin(SSD1306_SWITCHCAPVCC, 0x3D)) {
        oledConnected = true;
        Serial.println("[OLED] Display connected at 0x3D!");
      }
    }
  }

  if (oledConnected) {
    display.clearDisplay();
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(18, 12);
    display.println("MY BOT READY!");
    display.setCursor(10, 26);
    display.println("WiFi: my bot");
    display.setCursor(10, 38);
    display.println("IP: 192.168.4.1");
    display.setCursor(10, 50);
    display.println("Auto-Off: 1m idle");
    display.display();
  }

  // Hold boot splash screen for 1.5 seconds before starting animation
  lastAnimUpdate = millis() + 1500;

  Serial.println(
      "[READY] Broadcast active. Search for 'my bot' on your phone!");
  Serial.println(
      "[BATTERY SAVER] Wi-Fi turns OFF after 1 min ONLY IF NO DEVICE CONNECTS.");
  Serial.println(
      "[BATTERY SAVER] If anyone is connected or uploading code, Wi-Fi STAYS ON!");
}

void loop() {
  unsigned long now = millis();

  // ==========================================
  // 1. Wi-Fi & Services (Auto-Off ONLY IF NO CLIENT CONNECTED)
  // ==========================================
  if (wifiEnabled) {
    // OTA & Web Handlers
    ArduinoOTA.handle();
    dnsServer.processNextRequest();
    server.handleClient();

    int stations = WiFi.softAPgetStationNum();

    if (stations > 0 || Update.isRunning()) {
      // Client is connected or OTA upload in progress:
      // DO NOT TURN OFF WI-FI! Keep it awake continuously!
      // Once client disconnects, give 1 minute (60s) buffer before sleeping.
      wifiOffTime = now + 60000;
    } else if (now >= wifiOffTime) {
      // No client connected for 1 full minute -> Turn OFF Wi-Fi to save battery!
      disableWiFi();
    } else {
      // Status heartbeat every 3 seconds while waiting for clients
      static unsigned long lastLog = 0;
      if (now - lastLog >= 3000) {
        lastLog = now;
        unsigned long secLeft = (now < wifiOffTime) ? (wifiOffTime - now) / 1000 : 0;
        Serial.printf("[WIFI ACTIVE] Stations: %d | Auto-off in: %luds (if no connection)\n",
                      stations, secLeft);
      }
    }
  }

  // ==========================================
  // 2. OLED Animation Update (180ms - Smooth & CPU friendly)
  // ==========================================
  if (now - lastAnimUpdate >= 180) {
    lastAnimUpdate = now;
    animFrame++;
    if (oledConnected) {
      renderCurrentFace();

      // Top-right corner indicators:
      if (wifiEnabled) {
        if (WiFi.softAPgetStationNum() > 0) {
          // Connected: Solid dot indicator
          display.fillCircle(124, 4, 2, SSD1306_WHITE);
        } else if ((animFrame % 4) < 2) {
          // Waiting for connection: Blinking dot
          display.fillCircle(124, 4, 2, SSD1306_WHITE);
        }
        display.display();
      }
    }
  }

  // Small delay yields CPU to FreeRTOS idle task
  delay(5);
}
