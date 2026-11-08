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
#include <time.h>
#include <sys/time.h>
#include <Preferences.h>
#include <Fonts/FreeSans9pt7b.h>
#include <Fonts/FreeSansBold9pt7b.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold24pt7b.h>

Preferences prefs;

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
String bdayName = "BUDDY";
unsigned long lastAnimUpdate = 0;
int animFrame = 0;

// Wi-Fi Battery Saver: Auto-off after 1 minute IF NO CLIENT IS CONNECTED
bool wifiEnabled = true;
unsigned long wifiOffTime = 60000; // 60 seconds from boot
bool timeSynced = false; // Set to true once synced from user device

// ==========================================
// PLAYLIST & SLIDESHOW CAROUSEL SYSTEM
// ==========================================
struct SlideItem {
  String face;
  int durationSec;
};

#define MAX_SLIDES 16
SlideItem playlist[MAX_SLIDES] = {
  {"time", 4},
  {"date", 3},
  {"happy", 4},
  {"cool", 3}
};
int playlistCount = 4;
bool slideshowActive = false;
int currentSlideIndex = 0;
unsigned long slideStartTime = 0;

// Slide transition animation state
bool inTransition = false;
unsigned long transitionStartTime = 0;
const unsigned long TRANSITION_DURATION = 400; // 400ms smooth slide
int transitionFromIndex = 0;
int transitionToIndex = 0;

uint8_t slideBufA[1024];
uint8_t slideBufB[1024];

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
    .studio-card {
      background: rgba(15, 23, 42, 0.75);
      border: 1px solid rgba(99, 102, 241, 0.35);
      backdrop-filter: blur(14px);
      border-radius: 16px;
      padding: 16px 14px;
      margin-bottom: 20px;
      max-width: 500px;
      margin-left: auto;
      margin-right: auto;
      box-shadow: 0 4px 20px rgba(0, 0, 0, 0.25);
    }
    .studio-header {
      display: flex;
      justify-content: space-between;
      align-items: center;
      margin-bottom: 12px;
      gap: 8px;
    }
    .slide-item {
      background: rgba(255, 255, 255, 0.04);
      border: 1px solid rgba(255, 255, 255, 0.08);
      border-radius: 12px;
      padding: 8px 10px;
      margin-bottom: 8px;
      display: flex;
      align-items: center;
      gap: 6px;
      transition: background 0.2s;
    }
    .slide-item.dragging {
      opacity: 0.5;
      border: 1px dashed #818cf8;
    }
    .slide-select {
      background: #0b1120;
      color: #f8fafc;
      border: 1px solid rgba(255, 255, 255, 0.15);
      border-radius: 8px;
      padding: 6px 8px;
      font-size: 12px;
      flex: 1;
      min-width: 0;
      outline: none;
    }
    .dur-ctrl {
      display: flex;
      align-items: center;
      gap: 3px;
      background: rgba(0, 0, 0, 0.3);
      border: 1px solid rgba(255, 255, 255, 0.1);
      border-radius: 8px;
      padding: 2px 4px;
    }
    .step-btn {
      background: rgba(255, 255, 255, 0.12);
      color: #fff;
      border: none;
      border-radius: 6px;
      width: 24px;
      height: 24px;
      display: flex;
      align-items: center;
      justify-content: center;
      cursor: pointer;
      font-weight: 700;
      font-size: 13px;
    }
    .step-btn:active { transform: scale(0.9); }
    .dur-val {
      font-size: 12px;
      font-weight: 700;
      color: #38bdf8;
      min-width: 24px;
      text-align: center;
    }
    .del-btn {
      background: rgba(239, 68, 68, 0.15);
      color: #f87171;
      border: 1px solid rgba(239, 68, 68, 0.3);
      border-radius: 6px;
      width: 26px;
      height: 26px;
      display: flex;
      align-items: center;
      justify-content: center;
      cursor: pointer;
      font-size: 12px;
    }
    .reorder-col {
      display: flex;
      flex-direction: column;
      gap: 2px;
    }
    .reorder-btn {
      background: transparent;
      color: #64748b;
      border: none;
      cursor: pointer;
      font-size: 10px;
      line-height: 1;
      padding: 1px;
    }
    .reorder-btn:hover { color: #a5b4fc; }
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
    <div style="display:flex; gap:8px; justify-content:center; margin-top:8px; flex-wrap:wrap;">
      <button id="syncBtn" onclick="syncTimeWithDevice()" style="background: rgba(99, 102, 241, 0.2); border: 1px solid rgba(99, 102, 241, 0.4); color: #a5b4fc; padding: 6px 14px; border-radius: 20px; font-size: 11px; font-weight: 600; cursor: pointer; transition: all 0.2s;">
        🕒 Sync Phone Time
      </button>
      <button onclick="if(confirm('Turn off Wi-Fi now to save battery?')){fetch('/sleep');alert('Wi-Fi is turning OFF! Device will now run in low-power battery mode.');}" style="background: rgba(16, 185, 129, 0.15); border: 1px solid rgba(16, 185, 129, 0.4); color: #34d399; padding: 6px 14px; border-radius: 20px; font-size: 11px; font-weight: 600; cursor: pointer;">
        🔋 Turn Off Wi-Fi Now (Save ~85% Battery)
      </button>
    </div>
  </header>

  <div class="active-bar">
    <span>CURRENT MOOD:</span>
    <span class="active-pill" id="currentLabel">SINGLE (NORMAL)</span>
  </div>

  <!-- 🎞️ SLIDESHOW PLAYLIST STUDIO -->
  <div class="studio-card">
    <div class="studio-header">
      <div>
        <div style="font-weight:700; font-size:14px; color:#38bdf8; display:flex; align-items:center; gap:6px;">
          <span>🎞️</span> Slideshow Playlist
        </div>
        <div style="font-size:11px; color:var(--text-dim);">Line se cycle hone ke liye list banayein</div>
      </div>
      <button id="slideshowToggleBtn" onclick="toggleSlideshow()" style="background:linear-gradient(135deg, #10b981, #06b6d4); color:#fff; border:none; padding:7px 14px; border-radius:20px; font-size:12px; font-weight:700; cursor:pointer; box-shadow:0 0 12px rgba(16,185,129,0.3);">
        ▶ Start Slideshow
      </button>
    </div>

    <div id="playlistList"></div>

    <div style="display:flex; gap:8px; margin-top:8px;">
      <button onclick="addSlideItem()" style="flex:1; background:rgba(99, 102, 241, 0.15); border:1px dashed #818cf8; color:#a5b4fc; padding:8px; border-radius:10px; font-size:12px; font-weight:600; cursor:pointer;">
        ➕ Add Slide (+)
      </button>
    </div>
  </div>

  <div class="grid">
    <!-- 1. Standalone Time -->
    <div class="card" onclick="setFace('time', 'Digital Time (Live)', this)">
      <span class="emoji">🕒</span>
      <div class="title">Digital Time</div>
      <div class="desc">HH : MM : SS Live</div>
    </div>

    <!-- 2. Standalone Date -->
    <div class="card" onclick="setFace('date', 'Today Date', this)">
      <span class="emoji">📅</span>
      <div class="title">Today Date</div>
      <div class="desc">Day, Month, Year</div>
    </div>

    <!-- 3. Normal / Idle -->
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

    <!-- 13. Happy Birthday with Custom Name Box -->
    <div class="card" id="card_birthday" onclick="selectBirthday(this)" style="grid-column: 1 / -1; background: linear-gradient(135deg, rgba(255,215,0,0.18), rgba(236,72,153,0.22)); border-color: rgba(255,215,0,0.45); padding: 12px 14px;">
      <div style="display:flex; align-items:center; justify-content:space-between; flex-wrap:wrap; gap:8px;">
        <div style="display:flex; align-items:center; gap:8px;">
          <span class="emoji" style="margin:0; font-size:30px;">🎂</span>
          <div style="text-align:left;">
            <div class="title" style="color:#fbbf24; font-size:14px;">Happy Birthday</div>
            <div class="desc">Screen 1: HAPPY • 2: BIRTHDAY • 3: Name</div>
          </div>
        </div>
        <div style="display:flex; align-items:center; gap:6px;" onclick="event.stopPropagation()">
          <input type="text" id="bdayInput" maxlength="10" placeholder="NAME" value="BUDDY"
                 style="background:rgba(9, 13, 22, 0.85); border:1px solid #fbbf24; color:#f8fafc; border-radius:8px; padding:6px 8px; font-size:13px; font-weight:800; width:110px; text-align:center; text-transform:uppercase; outline:none;"
                 oninput="handleBdayInput(this.value)">
          <button onclick="saveAndPlayBday()" style="background:linear-gradient(135deg, #fbbf24, #f43f5e); border:none; border-radius:8px; color:#fff; font-weight:700; padding:7px 12px; font-size:12px; cursor:pointer; box-shadow:0 0 10px rgba(251,191,36,0.3);">
            Play 🎂
          </button>
        </div>
      </div>
    </div>

    <!-- Diwali Celebration -->
    <div class="card" onclick="setFace('diwali', 'Diwali 🪔', this)" style="background: linear-gradient(135deg, rgba(255,165,0,0.2), rgba(255,215,0,0.15)); border-color: rgba(255,165,0,0.4);">
      <span class="emoji">🪔</span>
      <div class="title" style="color:#fbbf24;">Diwali</div>
      <div class="desc">Diya & Rangoli</div>
    </div>

    <!-- Ram Ji -->
    <div class="card" onclick="setFace('ram', 'Jai Shri Ram 🏹', this)" style="background: linear-gradient(135deg, rgba(255,140,0,0.2), rgba(255,69,0,0.15)); border-color: rgba(255,140,0,0.4);">
      <span class="emoji">🏹</span>
      <div class="title" style="color:#fb923c;">Ram Ji</div>
      <div class="desc">Jai Shri Ram</div>
    </div>

    <!-- Fireworks/Patake -->
    <div class="card" onclick="setFace('fireworks', 'Fireworks 🎆', this)" style="background: linear-gradient(135deg, rgba(239,68,68,0.2), rgba(245,158,11,0.15)); border-color: rgba(239,68,68,0.4);">
      <span class="emoji">🎆</span>
      <div class="title" style="color:#f87171;">Patake</div>
      <div class="desc">Skyshot & Fireworks</div>
    </div>

    <!-- 14. Torch Mode (Flashlight - All Pixels Full Brightness) -->
    <div class="card test-card" onclick="setFace('torch', 'Torch (Full Light) 🔦', this)" style="background: linear-gradient(135deg, rgba(254, 240, 138, 0.2), rgba(250, 204, 21, 0.25)); border-color: rgba(250, 204, 21, 0.6);">
      <span class="emoji">🔦</span>
      <div>
        <div class="title" style="color:#fef08a;">Torch Mode (Full Light)</div>
        <div class="desc">All 100% OLED Pixels White at Maximum Brightness</div>
      </div>
    </div>
  </div>

  <footer>
    ESP32-C3 SuperMini • 0.96" SSD1306 (128x64) • OTA Enabled
  </footer>

  <script>
    var AVAILABLE_FACES = [
      { id: 'time', name: '🕒 Digital Time (Live)' },
      { id: 'date', name: '📅 Today Date' },
      { id: 'idle', name: '👀 Normal (Blink)' },
      { id: 'happy', name: '😊 Happy Smile' },
      { id: 'love', name: '😍 Love Hearts' },
      { id: 'wink', name: '😉 Playful Wink' },
      { id: 'cool', name: '😎 Cool Shades' },
      { id: 'sleepy', name: '😴 Sleepy Zzz' },
      { id: 'angry', name: '😡 Angry' },
      { id: 'sad', name: '😢 Sad Tear' },
      { id: 'surprised', name: '😲 Shocked' },
      { id: 'laughing', name: '😆 Laughing XD' },
      { id: 'confused', name: '🤨 Confused' },
      { id: 'dizzy', name: '😵 Dizzy' },
      { id: 'birthday', name: '🎂 Birthday' },
      { id: 'diwali', name: '🪔 Diwali Celebration' },
      { id: 'ram', name: '🏹 Jai Shri Ram' },
      { id: 'fireworks', name: '🎆 Patake/Fireworks' },
      { id: 'torch', name: '🔦 Torch (Full Light)' }
    ];

    var currentPlaylist = [
      { face: 'time', duration: 4 },
      { face: 'date', duration: 3 },
      { face: 'happy', duration: 4 },
      { face: 'cool', duration: 3 }
    ];
    var isSlideshowActive = false;
    var dragSrcIdx = -1;

    function renderPlaylistUI() {
      var cont = document.getElementById('playlistList');
      if (!cont) return;
      cont.innerHTML = '';

      currentPlaylist.forEach(function(item, idx) {
        var row = document.createElement('div');
        row.className = 'slide-item';
        row.draggable = true;

        // Drag & drop handlers
        row.addEventListener('dragstart', function(e) {
          dragSrcIdx = idx;
          row.classList.add('dragging');
          e.dataTransfer.effectAllowed = 'move';
        });
        row.addEventListener('dragend', function() {
          row.classList.remove('dragging');
        });
        row.addEventListener('dragover', function(e) {
          e.preventDefault();
          e.dataTransfer.dropEffect = 'move';
        });
        row.addEventListener('drop', function(e) {
          e.preventDefault();
          if (dragSrcIdx !== -1 && dragSrcIdx !== idx) {
            var moved = currentPlaylist.splice(dragSrcIdx, 1)[0];
            currentPlaylist.splice(idx, 0, moved);
            renderPlaylistUI();
            sendPlaylistToESP32();
          }
        });

        // Reorder buttons (Mobile fallback for drag & drop)
        var reorderCol = document.createElement('div');
        reorderCol.className = 'reorder-col';
        reorderCol.innerHTML = '<button class="reorder-btn" onclick="moveSlide(' + idx + ', -1)">▲</button>' +
                               '<button class="reorder-btn" onclick="moveSlide(' + idx + ', 1)">▼</button>';
        row.appendChild(reorderCol);

        // Face selector dropdown
        var sel = document.createElement('select');
        sel.className = 'slide-select';
        AVAILABLE_FACES.forEach(function(f) {
          var opt = document.createElement('option');
          opt.value = f.id;
          opt.innerText = f.name;
          if (f.id === item.face) opt.selected = true;
          sel.appendChild(opt);
        });
        sel.onchange = function() {
          item.face = sel.value;
          sendPlaylistToESP32();
        };
        row.appendChild(sel);

        // Duration stepper [-] [4s] [+]
        var durCtrl = document.createElement('div');
        durCtrl.className = 'dur-ctrl';
        durCtrl.innerHTML = '<button class="step-btn" onclick="changeSlideDuration(' + idx + ', -1)">-</button>' +
                            '<span class="dur-val">' + item.duration + 's</span>' +
                            '<button class="step-btn" onclick="changeSlideDuration(' + idx + ', 1)">+</button>';
        row.appendChild(durCtrl);

        // Delete button [- / ✕]
        var delBtn = document.createElement('button');
        delBtn.className = 'del-btn';
        delBtn.innerHTML = '✕';
        delBtn.title = 'Remove Slide';
        delBtn.onclick = function() {
          removeSlideItem(idx);
        };
        row.appendChild(delBtn);

        cont.appendChild(row);
      });
    }

    function addSlideItem() {
      if (currentPlaylist.length >= 16) {
        alert('Maximum 16 slides allowed!');
        return;
      }
      currentPlaylist.push({ face: 'happy', duration: 3 });
      renderPlaylistUI();
      sendPlaylistToESP32();
    }

    function removeSlideItem(idx) {
      if (currentPlaylist.length <= 1) {
        alert('At least 1 slide is required in playlist!');
        return;
      }
      currentPlaylist.splice(idx, 1);
      renderPlaylistUI();
      sendPlaylistToESP32();
    }

    function changeSlideDuration(idx, delta) {
      var newDur = currentPlaylist[idx].duration + delta;
      if (newDur >= 1 && newDur <= 60) {
        currentPlaylist[idx].duration = newDur;
        renderPlaylistUI();
        sendPlaylistToESP32();
      }
    }

    function moveSlide(idx, delta) {
      var target = idx + delta;
      if (target >= 0 && target < currentPlaylist.length) {
        var item = currentPlaylist.splice(idx, 1)[0];
        currentPlaylist.splice(target, 0, item);
        renderPlaylistUI();
        sendPlaylistToESP32();
      }
    }

    function toggleSlideshow() {
      isSlideshowActive = !isSlideshowActive;
      updateSlideshowUI();
      sendPlaylistToESP32();
    }

    function updateSlideshowUI() {
      var btn = document.getElementById('slideshowToggleBtn');
      var label = document.getElementById('currentLabel');
      if (isSlideshowActive) {
        if (btn) {
          btn.innerText = '⏸ Stop Slideshow';
          btn.style.background = 'linear-gradient(135deg, #ef4444, #f59e0b)';
        }
        if (label) {
          label.innerText = '🎬 SLIDESHOW RUNNING';
          label.style.color = '#38bdf8';
        }
        document.querySelectorAll('.card').forEach(c => c.classList.remove('selected'));
      } else {
        if (btn) {
          btn.innerText = '▶ Start Slideshow';
          btn.style.background = 'linear-gradient(135deg, #10b981, #06b6d4)';
        }
        if (label) {
          label.innerText = 'SINGLE MODE';
          label.style.color = '#a5b4fc';
        }
      }
    }

    function sendPlaylistToESP32() {
      var items = currentPlaylist.map(function(item) {
        return item.face + ':' + item.duration;
      }).join(',');
      fetch('/setslides?active=' + (isSlideshowActive ? '1' : '0') + '&items=' + encodeURIComponent(items))
        .catch(function(e) { console.error('Slideshow update failed:', e); });
    }

    function syncTimeWithDevice() {
      var now = new Date();
      var epoch = Math.floor(now.getTime() / 1000);
      var tzMinutes = -now.getTimezoneOffset(); // e.g. +330 for IST (+5:30)
      fetch('/synctime?epoch=' + epoch + '&tz=' + tzMinutes)
        .then(function(r) { return r.text(); })
        .then(function(msg) {
          console.log('Time Sync:', msg);
          var btn = document.getElementById('syncBtn');
          if (btn) {
            var tStr = now.toLocaleTimeString([], {hour: '2-digit', minute:'2-digit'});
            btn.innerHTML = '✓ Time Synced (' + tStr + ')';
            btn.style.color = '#34d399';
            btn.style.borderColor = '#10b981';
            btn.style.background = 'rgba(16, 185, 129, 0.2)';
          }
        })
        .catch(function(err) { console.error('Time Sync error:', err); });
    }

    // Single Face Tap Mode: Turns off slideshow & locks onto tapped face!
    function setFace(face, label, el) {
      if (navigator.vibrate) navigator.vibrate(30);

      isSlideshowActive = false;
      updateSlideshowUI();

      document.querySelectorAll('.card').forEach(c => c.classList.remove('selected'));
      if (el) el.classList.add('selected');
      document.getElementById('currentLabel').innerText = label + ' (LOCKED)';

      fetch('/set?face=' + face).catch(err => console.log(err));
    }

    function handleBdayInput(val) {
      if (val.length > 10) val = val.substring(0, 10);
      fetch('/setbday?name=' + encodeURIComponent(val)).catch(function(e) {});
    }

    function saveAndPlayBday() {
      var inp = document.getElementById('bdayInput');
      var name = inp ? inp.value.trim() : 'BUDDY';
      if (name.length > 10) name = name.substring(0, 10);
      if (name.length === 0) name = 'BUDDY';
      if (inp) inp.value = name.toUpperCase();

      handleBdayInput(name);
      var card = document.getElementById('card_birthday');
      setFace('birthday', 'HBD ' + name.toUpperCase() + ' 🎂', card);
    }

    function selectBirthday(el) {
      var inp = document.getElementById('bdayInput');
      var name = (inp && inp.value.trim().length > 0) ? inp.value.trim() : 'BUDDY';
      setFace('birthday', 'HBD ' + name.toUpperCase() + ' 🎂', el);
    }

    function loadSavedBday() {
      fetch('/getbday')
        .then(function(r) { return r.text(); })
        .then(function(name) {
          if (name && name.length > 0) {
            var inp = document.getElementById('bdayInput');
            if (inp) inp.value = name.toUpperCase();
          }
        }).catch(function(e) {});
    }

    window.addEventListener('DOMContentLoaded', function() {
      renderPlaylistUI();
      syncTimeWithDevice();
      loadSavedBday();
      setInterval(syncTimeWithDevice, 30000);
    });
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

// ==========================================
// 12 HIGH-EXPRESSION ANIMATED ROBOT & EMOJI FACES (25 FPS)
// Real expressive eyes, gaze tracking, micro-animations, eyelids, cheeks & props
// ==========================================

// 1. Idle Face (Alive pet: breathing bob, gaze tracking, smooth natural blinking, blushing cheeks, smile)
void drawIdleFace(int frame) {
  display.clearDisplay();

  int cycle = frame % 100;
  int bob = ((cycle % 30) < 15) ? 0 : 1; // Subtle natural breathing bob

  // Gaze tracking: glance right, glance left, or look forward
  int lookX = 0;
  if (cycle >= 50 && cycle < 75) {
    lookX = 6;  // Glance right
  } else if (cycle >= 15 && cycle < 35) {
    lookX = -6; // Glance left
  }

  // Smooth natural blinking + occasional playful double-blink
  bool isBlinking = (cycle >= 42 && cycle <= 46) || (cycle >= 88 && cycle <= 90);

  // Eyebrows (subtly tilted with gaze)
  display.fillRoundRect(22, 10 + bob - (lookX < 0 ? 1 : 0), 32, 4, 2, SSD1306_WHITE);
  display.fillRoundRect(74, 10 + bob - (lookX > 0 ? 1 : 0), 32, 4, 2, SSD1306_WHITE);

  if (isBlinking) {
    // Eyelid slit during blink with eyelashes
    display.fillRoundRect(20, 29 + bob, 36, 6, 3, SSD1306_WHITE);
    display.fillRoundRect(72, 29 + bob, 36, 6, 3, SSD1306_WHITE);
  } else {
    // Left Eye
    display.fillRoundRect(20, 16 + bob, 36, 32, 10, SSD1306_WHITE);
    // Right Eye
    display.fillRoundRect(72, 16 + bob, 36, 32, 10, SSD1306_WHITE);

    // Inner pupils tracking gaze
    display.fillCircle(38 + lookX, 32 + bob, 7, SSD1306_BLACK);
    display.fillCircle(90 + lookX, 32 + bob, 7, SSD1306_BLACK);

    // Dual Catchlight Twinkles (soulful OLED eyes)
    display.fillCircle(35 + lookX, 28 + bob, 2, SSD1306_WHITE);
    display.fillCircle(87 + lookX, 28 + bob, 2, SSD1306_WHITE);
    display.drawPixel(41 + lookX, 35 + bob, SSD1306_WHITE);
    display.drawPixel(93 + lookX, 35 + bob, SSD1306_WHITE);
  }

  // Cute blushing cheeks
  display.fillCircle(14, 44 + bob, 3, SSD1306_WHITE);
  display.fillCircle(114, 44 + bob, 3, SSD1306_WHITE);

  // Soft contented smiling mouth
  display.drawCircle(64, 47 + bob, 7, SSD1306_WHITE);
  display.fillRect(54, 40 + bob, 20, 9, SSD1306_BLACK);
}

// 2. Happy Face (Joyful bounce, smiling arch eyes, pulsing blush, open laugh with tongue & sparkles)
void drawHappyFace(int frame) {
  display.clearDisplay();

  int bounce = (frame % 8 < 4) ? 0 : 2; // Cheerful bouncing motion

  // Eyebrows raised high in joy
  display.fillRoundRect(22, 8 + bounce, 32, 3, 1, SSD1306_WHITE);
  display.fillRoundRect(74, 8 + bounce, 32, 3, 1, SSD1306_WHITE);

  // Smiling joy arch eyes (thick expressive double curves)
  for (int t = 0; t < 3; t++) {
    display.drawCircle(38, 36 + bounce, 16 + t, SSD1306_WHITE);
    display.drawCircle(90, 36 + bounce, 16 + t, SSD1306_WHITE);
  }
  // Clear lower half so they remain graceful upward smiling arches
  display.fillRect(16, 36 + bounce, 96, 24, SSD1306_BLACK);

  // Pulsing blushing cheek pads
  int bR = (frame % 6 < 3) ? 4 : 5;
  display.fillCircle(18, 42 + bounce, bR, SSD1306_WHITE);
  display.fillCircle(110, 42 + bounce, bR, SSD1306_WHITE);

  // Big joyful open smile with bouncing tongue
  display.fillRoundRect(50, 43 + bounce, 28, 15, 7, SSD1306_WHITE);
  display.fillRoundRect(52, 45 + bounce, 24, 11, 5, SSD1306_BLACK);
  display.fillCircle(64, 53 + bounce, 5, SSD1306_WHITE); // Cute tongue

  // Twinkling joy sparkles near eyes
  if ((frame % 6) < 4) {
    int sx1 = 12, sy1 = 18 + bounce;
    display.drawLine(sx1 - 3, sy1, sx1 + 3, sy1, SSD1306_WHITE);
    display.drawLine(sx1, sy1 - 3, sx1, sy1 + 3, SSD1306_WHITE);

    int sx2 = 116, sy2 = 18 + bounce;
    display.drawLine(sx2 - 3, sy2, sx2 + 3, sy2, SSD1306_WHITE);
    display.drawLine(sx2, sy2 - 3, sx2, sy2 + 3, SSD1306_WHITE);
  }
}

// 3. Love Face (Heart eyes with real heartbeat pulse, floating hearts, shy blush & sweet smile)
void drawLoveFace(int frame) {
  display.clearDisplay();

  // Heartbeat pulse rhythm (thump-thump)
  int pulse = (frame % 10 < 3) ? 28 : ((frame % 10 < 6) ? 24 : 26);

  // Main beating heart eyes
  drawHeart(38, 26, pulse);
  drawHeart(90, 26, pulse);

  // Floating mini-hearts drifting upwards in a gentle wave
  int fY1 = 52 - ((frame * 2) % 56);
  int fX1 = 12 + (int)(sin(frame * 0.25) * 4);
  if (fY1 >= 0 && fY1 < 56) drawHeart(fX1, fY1, 8);

  int fY2 = 52 - (((frame + 14) * 2) % 56);
  int fX2 = 116 + (int)(sin((frame + 14) * 0.25) * 4);
  if (fY2 >= 0 && fY2 < 56) drawHeart(fX2, fY2, 8);

  // Shy diagonal blush marks (// and \)
  display.drawLine(16, 42, 22, 38, SSD1306_WHITE);
  display.drawLine(20, 44, 26, 40, SSD1306_WHITE);
  display.drawLine(106, 38, 112, 42, SSD1306_WHITE);
  display.drawLine(102, 40, 108, 44, SSD1306_WHITE);

  // Sweet smiling open mouth with pink/white tongue
  display.fillRoundRect(52, 45, 24, 13, 6, SSD1306_WHITE);
  display.fillCircle(64, 53, 5, SSD1306_BLACK);
}

// 4. Playful Wink (Left winking with fluttery lashes, right sparkling eye with rotating twinkle, smirk & tongue)
void drawWinkFace(int frame) {
  display.clearDisplay();

  // Left Eye: Graceful closed winking smile curve with 3 eyelashes
  for (int t = 0; t < 3; t++) {
    display.drawCircle(38, 29, 14 + t, SSD1306_WHITE);
  }
  display.fillRect(18, 14, 40, 16, SSD1306_BLACK);
  // Fluttery eyelashes
  display.drawLine(23, 31, 18, 27, SSD1306_WHITE);
  display.drawLine(21, 35, 16, 34, SSD1306_WHITE);
  display.drawLine(23, 39, 19, 42, SSD1306_WHITE);

  // Left eyebrow tilted down in a wink
  display.fillRoundRect(22, 14, 32, 3, 1, SSD1306_WHITE);

  // Right Eye: Wide open, sparkling
  display.fillRoundRect(72, 16, 36, 32, 10, SSD1306_WHITE);
  display.fillCircle(90, 32, 7, SSD1306_BLACK);

  // Rotating 4-point twinkle star highlight inside right pupil
  int starRot = frame % 4;
  if (starRot < 2) {
    display.drawLine(86, 32, 94, 32, SSD1306_WHITE);
    display.drawLine(90, 28, 90, 36, SSD1306_WHITE);
  } else {
    display.drawLine(87, 29, 93, 35, SSD1306_WHITE);
    display.drawLine(87, 35, 93, 29, SSD1306_WHITE);
  }
  display.drawPixel(90, 32, SSD1306_WHITE);

  // Right eyebrow raised in expression
  display.fillRoundRect(74, 10, 32, 3, 1, SSD1306_WHITE);

  // Cheeks: Left cheek has a cute blushing heart, right has a soft dot
  drawHeart(22, 46, 8);
  display.fillCircle(112, 42, 3, SSD1306_WHITE);

  // Playful smirk smile with cute tongue sticking out (blep!)
  display.drawCircle(58, 44, 12, SSD1306_WHITE);
  display.fillRect(44, 34, 28, 12, SSD1306_BLACK);
  // Cheeky tongue
  display.fillRoundRect(66, 47, 8, 8, 3, SSD1306_WHITE);
  display.drawLine(70, 47, 70, 52, SSD1306_BLACK);
}

// 5. Cool Face (Dark aviator sunglasses with dynamic sweeping gleam, raised brow, smug smirk & smoke puff)
void drawCoolFace(int frame) {
  display.clearDisplay();

  // Dark stylish sunglasses frames
  display.fillRoundRect(18, 16, 40, 26, 6, SSD1306_WHITE);
  display.fillRoundRect(70, 16, 40, 26, 6, SSD1306_WHITE);
  // Center nose bridge & side temple arms
  display.fillRoundRect(56, 22, 16, 6, 2, SSD1306_WHITE);
  display.drawLine(18, 20, 6, 24, SSD1306_WHITE);
  display.drawLine(110, 20, 122, 24, SSD1306_WHITE);

  // Dynamic white reflection gleam sweeping across both lenses smoothly
  int gleam = (frame * 3) % 55;
  if (gleam < 34) {
    // Left lens reflection
    display.drawLine(20 + gleam, 40, 26 + gleam, 18, SSD1306_BLACK);
    display.drawLine(21 + gleam, 40, 27 + gleam, 18, SSD1306_BLACK);
    // Right lens reflection
    display.drawLine(72 + gleam, 40, 78 + gleam, 18, SSD1306_BLACK);
    display.drawLine(73 + gleam, 40, 79 + gleam, 18, SSD1306_BLACK);
  }

  // Raised cool arched eyebrow over the glasses!
  display.fillRoundRect(22, 10, 32, 3, 1, SSD1306_WHITE);

  // Smug corner smirk mouth
  display.drawLine(58, 52, 74, 50, SSD1306_WHITE);
  display.drawLine(74, 50, 78, 46, SSD1306_WHITE);

  // Toothpick stick tilting upwards
  display.drawLine(72, 50, 84, 56, SSD1306_WHITE);

  // Animated drifting smoke puff
  int sStep = frame % 16;
  int sX = 86 + (sStep / 2);
  int sY = 56 - sStep;
  if (sY >= 10) {
    display.drawCircle(sX, sY, 2 + (sStep / 6), SSD1306_WHITE);
  }
}

// 6. Sleepy Face (Drooping closed eye curves, cartoon breathing snot bubble that expands and pops, drifting Zzz)
void drawSleepyFace(int frame) {
  display.clearDisplay();

  // Relaxed sleeping curved eye slits (U U)
  for (int t = 0; t < 3; t++) {
    display.drawCircle(36, 26, 14 + t, SSD1306_WHITE);
    display.drawCircle(92, 26, 14 + t, SSD1306_WHITE);
  }
  display.fillRect(16, 12, 96, 16, SSD1306_BLACK);

  // Drooping relaxed eyebrows
  display.fillRoundRect(20, 20, 32, 2, 1, SSD1306_WHITE);
  display.fillRoundRect(76, 20, 32, 2, 1, SSD1306_WHITE);

  // Classic cartoon breathing snot bubble: grows and pops!
  int bCycle = frame % 28;
  if (bCycle < 24) {
    int bR = 2 + (bCycle * 7 / 24);
    display.drawCircle(68 + bR / 2, 45 - bR / 2, bR, SSD1306_WHITE);
    display.drawPixel(68 + bR / 2 - bR / 3, 45 - bR / 2 - bR / 3, SSD1306_WHITE); // Highlight
  } else {
    // Pop sparkle burst!
    display.drawPixel(73, 40, SSD1306_WHITE);
    display.drawPixel(77, 37, SSD1306_WHITE);
    display.drawPixel(75, 45, SSD1306_WHITE);
  }

  // Drifting "Zzz": Three staggered letters floating in a sine wave
  int z1 = (frame * 2) % 40;
  int z2 = (frame * 2 + 14) % 40;
  int z3 = (frame * 2 + 28) % 40;

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(92 + (int)(sin(z1 * 0.25) * 4), 32 - z1);
  display.print("z");
  display.setCursor(102 + (int)(sin(z2 * 0.25) * 4), 28 - z2);
  display.print("Z");
  display.setTextSize(2);
  display.setCursor(112 + (int)(sin(z3 * 0.25) * 4), 22 - z3);
  display.print("Z");

  // Tiny relaxed snoozing mouth
  display.fillCircle(58, 48, 3, SSD1306_WHITE);
  display.fillCircle(58, 48, 2, SSD1306_BLACK);
}

// 7. Angry Face (Furrowed V-brows, intense piercing eyes, clenched teeth grimace, rage vein & puffing steam)
void drawAngryFace(int frame) {
  display.clearDisplay();

  int shake = (frame % 2 == 0) ? 1 : -1; // Furious screen shake vibration

  // Fierce V-furrowed Eyebrows (thick angled slashes)
  for (int i = 0; i < 4; i++) {
    display.drawLine(18 + shake, 10 + i, 56 + shake, 20 + i, SSD1306_WHITE);
    display.drawLine(110 + shake, 10 + i, 72 + shake, 20 + i, SSD1306_WHITE);
  }

  // Intense Piercing Eyes
  display.fillRoundRect(22 + shake, 20, 32, 22, 6, SSD1306_WHITE);
  display.fillRoundRect(74 + shake, 20, 32, 22, 6, SSD1306_WHITE);
  // Cut top-inner slant to match furious furrowed brows
  display.fillTriangle(38 + shake, 20, 54 + shake, 20, 54 + shake, 28, SSD1306_BLACK);
  display.fillTriangle(74 + shake, 20, 90 + shake, 20, 74 + shake, 28, SSD1306_BLACK);

  // Black pupils looking furiously forward
  display.fillCircle(36 + shake, 29, 4, SSD1306_BLACK);
  display.fillCircle(92 + shake, 29, 4, SSD1306_BLACK);

  // Throbbing Anime Rage Vein on Temple
  if ((frame % 6) < 4) {
    int vx = 112 + shake, vy = 8;
    display.drawLine(vx, vy, vx + 8, vy, SSD1306_WHITE);
    display.drawLine(vx + 4, vy - 4, vx + 4, vy + 4, SSD1306_WHITE);
    display.drawLine(vx, vy - 4, vx + 8, vy + 4, SSD1306_WHITE);
    display.drawLine(vx, vy + 4, vx + 8, vy - 4, SSD1306_WHITE);
  }

  // Steam clouds puffing out from both sides
  int sR = 2 + (frame % 4);
  display.drawCircle(8 + shake, 30, sR, SSD1306_WHITE);
  display.drawCircle(120 + shake, 30, sR, SSD1306_WHITE);

  // Clenched Teeth Grimace Mouth
  display.fillRoundRect(48 + shake, 48, 32, 10, 3, SSD1306_WHITE);
  display.drawFastHLine(48 + shake, 53, 32, SSD1306_BLACK);
  for (int tx = 54 + shake; tx < 78 + shake; tx += 6) {
    display.drawFastVLine(tx, 48, 10, SSD1306_BLACK);
  }
}

// 8. Sad Face (Sorrowful brows, glassy tear-filled eyes with water wobble, rolling teardrop & quivering pout)
void drawSadFace(int frame) {
  display.clearDisplay();

  // Sorrowful arched eyebrows (slanting down-outwards)
  for (int i = 0; i < 3; i++) {
    display.drawLine(20, 18 - i, 54, 12 - i, SSD1306_WHITE);
    display.drawLine(108, 18 - i, 74, 12 - i, SSD1306_WHITE);
  }

  // Glassy Tear-Filled Eyes
  display.fillRoundRect(20, 18, 36, 30, 10, SSD1306_WHITE);
  display.fillRoundRect(72, 18, 36, 30, 10, SSD1306_WHITE);

  // Waterline wobble inside pupils
  int wave = (frame % 6 < 3) ? 1 : 0;
  display.fillCircle(38, 34 + wave, 7, SSD1306_BLACK);
  display.fillCircle(90, 34 + wave, 7, SSD1306_BLACK);

  // Watery catchlight reflections
  display.fillCircle(35, 27, 3, SSD1306_WHITE);
  display.fillCircle(87, 27, 3, SSD1306_WHITE);
  display.fillCircle(41, 35, 2, SSD1306_WHITE);
  display.fillCircle(93, 35, 2, SSD1306_WHITE);

  // Animated rolling teardrop swelling and running down the cheek
  int tearStage = frame % 22;
  int tearY = 36 + tearStage * 22 / 22;
  display.fillCircle(92, tearY, 3, SSD1306_WHITE);
  display.fillTriangle(92 - 3, tearY, 92 + 3, tearY, 92, tearY - 4, SSD1306_WHITE);
  // Splash at bottom
  if (tearStage >= 20) {
    display.drawPixel(90, 60, SSD1306_WHITE);
    display.drawPixel(94, 60, SSD1306_WHITE);
  }

  // Quivering down-turned pout mouth
  int lipTremble = (frame % 4 < 2) ? 0 : 1;
  display.drawCircle(64, 58 + lipTremble, 10, SSD1306_WHITE);
  display.fillRect(50, 58 + lipTremble, 28, 12, SSD1306_BLACK);
}

// 9. Surprised Face (Shock jolt, high-shot eyebrows, huge popped eyes with shaking pupils, dropped jaw & sweat drop)
void drawSurprisedFace(int frame) {
  display.clearDisplay();

  int popY = (frame % 12 < 3) ? -2 : 0; // Shock jolt

  // Eyebrows shot high up near the top of the display!
  display.fillRoundRect(22, 6 + popY, 32, 3, 1, SSD1306_WHITE);
  display.fillRoundRect(74, 6 + popY, 32, 3, 1, SSD1306_WHITE);

  // Enormous wide popped eyes
  display.fillRoundRect(20, 12 + popY, 36, 36, 12, SSD1306_WHITE);
  display.fillRoundRect(72, 12 + popY, 36, 36, 12, SSD1306_WHITE);

  // Tiny shocked pupils shaking in disbelief
  int sShk = (frame % 2 == 0) ? 1 : -1;
  display.fillCircle(38 + sShk, 30 + popY, 4, SSD1306_BLACK);
  display.fillCircle(90 + sShk, 30 + popY, 4, SSD1306_BLACK);
  display.drawPixel(37 + sShk, 29 + popY, SSD1306_WHITE);
  display.drawPixel(89 + sShk, 29 + popY, SSD1306_WHITE);

  // Giant dropped-jaw 'O' mouth stretching downwards
  display.fillRoundRect(56, 44 + popY, 16, 18, 7, SSD1306_WHITE);
  display.fillRoundRect(58, 46 + popY, 12, 14, 5, SSD1306_BLACK);

  // Anime sweat drop dripping down the side of the temple
  int swY = 16 + (frame % 16);
  display.fillCircle(116, swY, 3, SSD1306_WHITE);
  display.fillTriangle(113, swY, 119, swY, 116, swY - 5, SSD1306_WHITE);
}

// 10. Laughing Face (Belly laugh bounce, tight squint eyes > <, projectile tears of joy, vibrating mouth & tongue)
void drawLaughingFace(int frame) {
  display.clearDisplay();

  int bounce = (frame % 4 < 2) ? 0 : 3; // Rhythmic belly laugh shaking

  // Tightly squinted laughing eyes (> <) with thick strokes
  for (int i = -1; i <= 1; i++) {
    // Left eye (>)
    display.drawLine(22, 22 + bounce + i, 44, 32 + bounce + i, SSD1306_WHITE);
    display.drawLine(44, 32 + bounce + i, 22, 42 + bounce + i, SSD1306_WHITE);

    // Right eye (<)
    display.drawLine(106, 22 + bounce + i, 84, 32 + bounce + i, SSD1306_WHITE);
    display.drawLine(84, 32 + bounce + i, 106, 42 + bounce + i, SSD1306_WHITE);
  }

  // Projectile tears of joy spurting outwards from both eyes!
  int t1 = frame % 8;
  int t2 = (frame + 4) % 8;
  // Left tears
  display.fillCircle(14 - t1, 28 - t1 / 2, 2, SSD1306_WHITE);
  display.fillCircle(12 - t2, 36 + t2 / 2, 2, SSD1306_WHITE);
  // Right tears
  display.fillCircle(114 + t1, 28 - t1 / 2, 2, SSD1306_WHITE);
  display.fillCircle(116 + t2, 36 + t2 / 2, 2, SSD1306_WHITE);

  // Massive joyful laughing open mouth with bouncing tongue
  display.fillRoundRect(46, 38 + bounce, 36, 22, 10, SSD1306_WHITE);
  display.fillRoundRect(48, 40 + bounce, 32, 18, 8, SSD1306_BLACK);
  // Bouncing tongue
  display.fillCircle(64, 52 + bounce, 8, SSD1306_WHITE);
}

// 11. Confused Face (One brow raised sky-high, one eye squinting, wavy squiggly mouth & animated floating '?')
void drawConfusedFace(int frame) {
  display.clearDisplay();

  // Asymmetrical Eyebrows
  display.fillRoundRect(22, 14, 32, 4, 2, SSD1306_WHITE); // Left normal brow
  display.fillRoundRect(74, 5, 32, 4, 2, SSD1306_WHITE);  // Right raised sky-high!

  // Asymmetrical Eyes
  // Left Eye: Curious round eye looking up-right
  display.fillRoundRect(20, 20, 36, 28, 8, SSD1306_WHITE);
  display.fillCircle(40, 32, 6, SSD1306_BLACK);
  display.fillCircle(38, 30, 2, SSD1306_WHITE);

  // Right Eye: Squinted narrow eye under the raised brow
  display.fillRoundRect(74, 15, 32, 18, 6, SSD1306_WHITE);
  display.fillCircle(90, 24, 4, SSD1306_BLACK);

  // Animated Question Mark '?' floating and bobbing over the raised brow
  int qBob = (frame % 8 < 4) ? 0 : 2;
  int qx = 114, qy = 6 + qBob;
  display.drawCircle(qx + 3, qy + 3, 3, SSD1306_WHITE);
  display.fillRect(qx - 1, qy + 3, 4, 4, SSD1306_BLACK);
  display.drawLine(qx + 3, qy + 6, qx + 3, qy + 8, SSD1306_WHITE);
  display.drawPixel(qx + 3, qy + 11, SSD1306_WHITE);

  // Wavy Squiggly Mouth ("errr... what?")
  display.drawLine(48, 52, 56, 49, SSD1306_WHITE);
  display.drawLine(56, 49, 64, 55, SSD1306_WHITE);
  display.drawLine(64, 55, 72, 49, SSD1306_WHITE);
  display.drawLine(72, 49, 80, 53, SSD1306_WHITE);
}

// 12. Dizzy Face (Hypnotic spinning spiral eyes, orbiting cartoon stars around head, loopy mouth & tongue)
void drawDizzyFace(int frame) {
  display.clearDisplay();

  // Hypnotic Spinning Spiral Eyes
  int rot = (frame * 35) % 360;
  float rad = rot * 3.14159f / 180.0f;

  for (int side = 0; side < 2; side++) {
    int cx = (side == 0) ? 38 : 90;
    int cy = 32;

    // Outer eye circle
    display.drawCircle(cx, cy, 16, SSD1306_WHITE);
    display.drawCircle(cx, cy, 15, SSD1306_WHITE);

    // Dynamic rotating spiral spokes
    for (int a = 0; a < 3; a++) {
      float ang = rad + a * 2.094f; // 120 degrees apart
      int x1 = cx + (int)(cos(ang) * 4);
      int y1 = cy + (int)(sin(ang) * 4);
      int x2 = cx + (int)(cos(ang + 0.5f) * 8);
      int y2 = cy + (int)(sin(ang + 0.5f) * 8);
      int x3 = cx + (int)(cos(ang + 1.0f) * 13);
      int y3 = cy + (int)(sin(ang + 1.0f) * 13);
      display.drawLine(cx, cy, x1, y1, SSD1306_WHITE);
      display.drawLine(x1, y1, x2, y2, SSD1306_WHITE);
      display.drawLine(x2, y2, x3, y3, SSD1306_WHITE);
    }
  }

  // 3 Cartoon Dizzy Stars orbiting in an ellipse around the head
  float starOrbit = (frame * 12) * 3.14159f / 180.0f;
  for (int s = 0; s < 3; s++) {
    float sAng = starOrbit + s * 2.094f;
    int sx = 64 + (int)(cos(sAng) * 48);
    int sy = 12 + (int)(sin(sAng) * 7);

    // Draw 4-point cartoon star
    display.drawLine(sx - 3, sy, sx + 3, sy, SSD1306_WHITE);
    display.drawLine(sx, sy - 3, sx, sy + 3, SSD1306_WHITE);
    display.drawPixel(sx, sy, SSD1306_WHITE);
  }

  // Loopy dazed wavy mouth with tongue hanging out sideways
  display.drawLine(50, 52, 58, 49, SSD1306_WHITE);
  display.drawLine(58, 49, 66, 54, SSD1306_WHITE);
  display.drawLine(66, 54, 76, 50, SSD1306_WHITE);

  // Dangling loopy tongue (blep)
  display.fillRoundRect(68, 51, 8, 9, 3, SSD1306_WHITE);
  display.drawLine(72, 51, 72, 57, SSD1306_BLACK);
}

// 13. Torch Mode (100% All Pixels ON at Maximum Brightness)
void drawTorchMode() {
  display.fillScreen(SSD1306_WHITE);
}

// 14. Happy Birthday (Screen 1: HAPPY -> Screen 2: BIRTHDAY -> Screen 3: Custom Name -> Screen 4: Cake)
void drawBirthdayFace(int frame) {
  display.clearDisplay();

  // 8.8-second loop using millis for smooth, reliable screen switching
  unsigned long t = millis() % 8800; // 0 to 8799 ms

  // Get active custom name (trimmed, uppercase, max 10 chars)
  String name = bdayName;
  name.trim();
  if (name.length() == 0) name = "BUDDY";
  if (name.length() > 10) name = name.substring(0, 10);
  name.toUpperCase();
  int nameLen = name.length();

  if (t < 2200) {
    // ========================================================
    // SCREEN 1: "HAPPY" (0 to 2200ms)
    // ========================================================
    display.setTextColor(SSD1306_WHITE);

    // Festive ornamental top & bottom borders
    display.drawFastHLine(18, 6, 92, SSD1306_WHITE);
    display.fillCircle(64, 6, 2, SSD1306_WHITE);
    display.drawFastHLine(18, 57, 92, SSD1306_WHITE);
    display.fillCircle(64, 57, 2, SSD1306_WHITE);

    // Floating party balloons on sides
    // Left balloon
    display.fillCircle(12, 30, 6, SSD1306_WHITE);
    display.fillTriangle(12, 36, 10, 39, 14, 39, SSD1306_WHITE);
    display.drawLine(12, 39, 10, 48, SSD1306_WHITE);
    // Right balloon
    display.fillCircle(116, 26, 6, SSD1306_WHITE);
    display.fillTriangle(116, 32, 114, 35, 118, 35, SSD1306_WHITE);
    display.drawLine(116, 35, 118, 45, SSD1306_WHITE);

    // "HAPPY" in big letters
    display.setTextSize(2);
    int bobY = 24 + ((frame % 6 < 3) ? 0 : 2);
    display.setCursor(34, bobY);
    display.print("HAPPY");

    // Falling confetti
    for (int i = 0; i < 8; i++) {
      int cx = (i * 17 + frame * 3) % 128;
      int cy = (i * 11 + frame * 2) % 64;
      if (cx > 18 && cx < 110 && (cy < 18 || cy > 44)) {
        display.drawPixel(cx, cy, SSD1306_WHITE);
      }
    }

  } else if (t < 4400) {
    // ========================================================
    // SCREEN 2: "BIRTHDAY" (2200 to 4400ms)
    // ========================================================
    display.setTextColor(SSD1306_WHITE);

    // Festive border
    display.drawFastHLine(10, 8, 108, SSD1306_WHITE);
    display.drawFastHLine(10, 55, 108, SSD1306_WHITE);

    // Party poppers on bottom corners shooting sparks
    // Left popper cone
    display.fillTriangle(6, 58, 6, 48, 14, 58, SSD1306_WHITE);
    display.drawLine(12, 50, 24, 38, SSD1306_WHITE);
    display.drawLine(14, 52, 28, 44, SSD1306_WHITE);
    display.drawPixel(25, 36, SSD1306_WHITE);
    display.drawPixel(29, 42, SSD1306_WHITE);

    // Right popper cone
    display.fillTriangle(122, 58, 122, 48, 114, 58, SSD1306_WHITE);
    display.drawLine(116, 50, 104, 38, SSD1306_WHITE);
    display.drawLine(114, 52, 100, 44, SSD1306_WHITE);
    display.drawPixel(103, 36, SSD1306_WHITE);
    display.drawPixel(99, 42, SSD1306_WHITE);

    // "BIRTHDAY" in big letters (8 chars * 12 = 96px, centered at x=16)
    display.setTextSize(2);
    int bobY = 24 + ((frame % 6 < 3) ? 0 : 2);
    display.setCursor(16, bobY);
    display.print("BIRTHDAY");

    // Confetti particles
    for (int i = 0; i < 10; i++) {
      int sx = (i * 13 + frame * 4) % 128;
      int sy = (i * 7 + frame * 3) % 20 + 2;
      display.drawPixel(sx, sy, SSD1306_WHITE);
    }

  } else if (t < 6600) {
    // ========================================================
    // SCREEN 3: CUSTOM NAME (e.g. "BUDDY") (4400 to 6600ms)
    // ========================================================
    display.setTextColor(SSD1306_WHITE);

    // Animated celebration border
    for (int i = 0; i < 128; i += 4) {
      if ((frame + i) % 8 < 4) {
        display.drawPixel(i, 2, SSD1306_WHITE);
        display.drawPixel(i, 61, SSD1306_WHITE);
      }
    }
    for (int i = 0; i < 64; i += 4) {
      if ((frame + i) % 8 < 4) {
        display.drawPixel(2, i, SSD1306_WHITE);
        display.drawPixel(125, i, SSD1306_WHITE);
      }
    }

    // Display Name in Big Bold Centered Text
    int bobY = 14 + ((frame % 4 < 2) ? 0 : 2);
    if (nameLen <= 5) {
      // Very big size 3 for short names (e.g. BUDDY)
      display.setTextSize(3);
      int textW = nameLen * 18;
      int nameX = (128 - textW) / 2;
      display.setCursor(nameX, bobY);
      display.print(name);
    } else {
      // Size 2 for names 6 to 10 characters (up to 120px)
      display.setTextSize(2);
      int textW = nameLen * 12;
      int nameX = (128 - textW) / 2;
      display.setCursor(nameX, bobY + 2);
      display.print(name);
    }

    // Pulsing heart below name
    int pulse = (frame % 6 < 3) ? 0 : 3;
    drawHeart(64, 49, 13 + pulse);

    // Side celebratory stars
    display.drawPixel(38, 48, SSD1306_WHITE);
    display.drawPixel(38 - 1, 48, SSD1306_WHITE);
    display.drawPixel(38 + 1, 48, SSD1306_WHITE);
    display.drawPixel(38, 47, SSD1306_WHITE);
    display.drawPixel(38, 49, SSD1306_WHITE);

    display.drawPixel(90, 48, SSD1306_WHITE);
    display.drawPixel(90 - 1, 48, SSD1306_WHITE);
    display.drawPixel(90 + 1, 48, SSD1306_WHITE);
    display.drawPixel(90, 47, SSD1306_WHITE);
    display.drawPixel(90, 49, SSD1306_WHITE);

  } else {
    // ========================================================
    // SCREEN 4: 3-TIER CAKE & "HBD <NAME>" (6600 to 8800ms)
    // ========================================================
    display.setTextColor(SSD1306_WHITE);

    // Cake base (3-tier)
    display.fillRoundRect(44, 46, 40, 9, 2, SSD1306_WHITE); // Bottom tier
    display.fillRoundRect(50, 37, 28, 9, 2, SSD1306_WHITE); // Middle tier
    display.fillRoundRect(56, 29, 16, 8, 2, SSD1306_WHITE); // Top tier

    // Frosting drips
    display.drawPixel(48, 48, SSD1306_BLACK);
    display.drawPixel(56, 48, SSD1306_BLACK);
    display.drawPixel(64, 48, SSD1306_BLACK);
    display.drawPixel(72, 48, SSD1306_BLACK);

    // Candle
    display.fillRect(63, 21, 3, 8, SSD1306_WHITE);

    // Flickering candle flame
    int fl = frame % 4;
    int fx = 64 + (fl == 1 ? -1 : (fl == 3 ? 1 : 0));
    display.fillCircle(fx, 17, 2, SSD1306_WHITE);
    display.drawPixel(fx, 14, SSD1306_WHITE);

    // Cake plate
    display.drawFastHLine(36, 56, 56, SSD1306_WHITE);

    // Top Header: "Happy Birthday" + Name
    display.setTextSize(1);
    display.setCursor(22, 2);
    display.print("Happy Birthday");

    int headerNameX = (128 - nameLen * 6) / 2;
    display.setCursor(headerNameX, 12);
    display.print(name);

    // Confetti rain
    for (int i = 0; i < 10; i++) {
      int px = (i * 13 + frame * 3) % 128;
      int py = (i * 7 + frame * 2) % 40 + 16;
      if (px < 40 || px > 88) {
        display.drawPixel(px, py, SSD1306_WHITE);
      }
    }
  }
}

// ==========================================
// CLOCK & DATE SLIDES (With Smooth Slide Transition)
// ==========================================

// Helper: Draw smooth circular-geometry vector digits (Any width, height, stroke)
void drawCircularDigit(int x, int y, char c, int w, int h, int stroke) {
  int r = w / 2;
  int rIn = max(1, r - stroke);
  int midY = y + h / 2;

  switch (c) {
    case '0':
      display.fillRoundRect(x, y, w, h, r, SSD1306_WHITE);
      display.fillRoundRect(x + stroke, y + stroke, w - 2 * stroke, h - 2 * stroke, rIn, SSD1306_BLACK);
      break;

    case '1': {
      int barX = x + (w - stroke) / 2;
      display.fillRoundRect(barX, y, stroke, h, max(1, stroke / 2), SSD1306_WHITE);
      display.fillRoundRect(barX - stroke, y + 2, stroke, stroke, max(1, stroke / 2), SSD1306_WHITE);
      break;
    }

    case '2': {
      int topH = h * 55 / 100;
      display.fillRoundRect(x, y, w, topH, r, SSD1306_WHITE);
      display.fillRoundRect(x + stroke, y + stroke, w - 2 * stroke, topH - stroke, rIn, SSD1306_BLACK);
      display.fillRect(x, y + h * 28 / 100, w * 60 / 100, h * 27 / 100, SSD1306_BLACK);
      for (int i = 0; i < stroke; i++) {
        display.drawLine(x + w - stroke + i, y + h * 45 / 100, x + i, y + h - stroke, SSD1306_WHITE);
      }
      display.fillRoundRect(x, y + h - stroke, w, stroke, max(1, stroke / 2), SSD1306_WHITE);
      break;
    }

    case '3': {
      int hHalf = h / 2 + 1;
      display.fillRoundRect(x, y, w, hHalf, r, SSD1306_WHITE);
      display.fillRoundRect(x + stroke, y + stroke, w - 2 * stroke, hHalf - 2 * stroke, rIn, SSD1306_BLACK);
      display.fillRect(x, y + stroke, w * 55 / 100, hHalf - 2 * stroke, SSD1306_BLACK);
      display.fillRoundRect(x, y + h - hHalf, w, hHalf, r, SSD1306_WHITE);
      display.fillRoundRect(x + stroke, y + h - hHalf + stroke, w - 2 * stroke, hHalf - 2 * stroke, rIn, SSD1306_BLACK);
      display.fillRect(x, y + h - hHalf + stroke, w * 55 / 100, hHalf - 2 * stroke, SSD1306_BLACK);
      display.fillRoundRect(x + stroke, midY - stroke / 2, w / 2, stroke, max(1, stroke / 2), SSD1306_WHITE);
      break;
    }

    case '4': {
      int vX = x + w - stroke - 1;
      display.fillRoundRect(vX, y, stroke, h, max(1, stroke / 2), SSD1306_WHITE);
      int hY = y + h * 60 / 100;
      display.fillRoundRect(x, hY, w, stroke, max(1, stroke / 2), SSD1306_WHITE);
      for (int i = 0; i < stroke; i++) {
        display.drawLine(x + i, hY, x + w * 40 / 100 + i, y, SSD1306_WHITE);
      }
      break;
    }

    case '5': {
      int topH = h * 45 / 100;
      int botH = h - topH + 2;
      display.fillRoundRect(x, y, w, stroke, max(1, stroke / 2), SSD1306_WHITE);
      display.fillRoundRect(x, y, stroke, topH, max(1, stroke / 2), SSD1306_WHITE);
      display.fillRoundRect(x, y + h - botH, w, botH, r, SSD1306_WHITE);
      display.fillRoundRect(x + stroke, y + h - botH + stroke, w - 2 * stroke, botH - 2 * stroke, rIn, SSD1306_BLACK);
      display.fillRect(x, y + h - botH + stroke, w * 55 / 100, botH - 2 * stroke, SSD1306_BLACK);
      display.fillRoundRect(x, y + h - botH, w * 70 / 100, stroke, max(1, stroke / 2), SSD1306_WHITE);
      break;
    }

    case '6': {
      int botH = h * 58 / 100;
      int topH = h * 55 / 100;
      // Top curved hook
      display.fillRoundRect(x, y, w, topH, r, SSD1306_WHITE);
      display.fillRoundRect(x + stroke, y + stroke, w - 2 * stroke, topH - stroke, rIn, SSD1306_BLACK);
      display.fillRect(x + w * 45 / 100, y + stroke, w * 55 / 100, topH, SSD1306_BLACK);
      // Bottom circular loop
      display.fillRoundRect(x, y + h - botH, w, botH, r, SSD1306_WHITE);
      display.fillRoundRect(x + stroke, y + h - botH + stroke, w - 2 * stroke, botH - 2 * stroke, rIn, SSD1306_BLACK);
      // Left smooth spine
      display.fillRoundRect(x, y + stroke, stroke, h - 2 * stroke, max(1, stroke / 2), SSD1306_WHITE);
      break;
    }

    case '7': {
      display.fillRoundRect(x, y, w, stroke, max(1, stroke / 2), SSD1306_WHITE);
      for (int i = 0; i < stroke; i++) {
        display.drawLine(x + w - stroke + i, y, x + w * 20 / 100 + i, y + h - 1, SSD1306_WHITE);
      }
      break;
    }

    case '8': {
      int hHalf = h / 2 + 1;
      display.fillRoundRect(x, y, w, hHalf, r, SSD1306_WHITE);
      display.fillRoundRect(x + stroke, y + stroke, w - 2 * stroke, hHalf - 2 * stroke, rIn, SSD1306_BLACK);
      display.fillRoundRect(x, y + h - hHalf, w, hHalf, r, SSD1306_WHITE);
      display.fillRoundRect(x + stroke, y + h - hHalf + stroke, w - 2 * stroke, hHalf - 2 * stroke, rIn, SSD1306_BLACK);
      break;
    }

    case '9': {
      int topH = h * 58 / 100;
      int botH = h * 55 / 100;
      // Bottom curved hook
      display.fillRoundRect(x, y + h - botH, w, botH, r, SSD1306_WHITE);
      display.fillRoundRect(x + stroke, y + h - botH + stroke, w - 2 * stroke, botH - 2 * stroke, rIn, SSD1306_BLACK);
      display.fillRect(x, y + h - botH, w * 55 / 100, botH - stroke, SSD1306_BLACK);
      // Top circular loop
      display.fillRoundRect(x, y, w, topH, r, SSD1306_WHITE);
      display.fillRoundRect(x + stroke, y + stroke, w - 2 * stroke, topH - stroke, rIn, SSD1306_BLACK);
      // Right smooth spine
      display.fillRoundRect(x + w - stroke, y + stroke, stroke, h - 2 * stroke, max(1, stroke / 2), SSD1306_WHITE);
      break;
    }

    case ':': {
      int dotR = max(1, stroke / 2);
      display.fillCircle(x + w / 2, y + h * 35 / 100, dotR, SSD1306_WHITE);
      display.fillCircle(x + w / 2, y + h * 68 / 100, dotR, SSD1306_WHITE);
      break;
    }
  }
}

// Slide 1: TIME (Clean circular font, blinking colon, AM/PM badge & live seconds)
void drawSlide1_Time(int xOffset, struct tm *t) {
  display.setTextWrap(false);
  display.setTextColor(SSD1306_WHITE);

  int h12 = t->tm_hour % 12;
  if (h12 == 0) h12 = 12;

  char hourBuf[4];
  char minBuf[4];
  char secBuf[4];
  snprintf(hourBuf, sizeof(hourBuf), "%02d", h12);
  snprintf(minBuf, sizeof(minBuf), "%02d", t->tm_min);
  snprintf(secBuf, sizeof(secBuf), "%02d", t->tm_sec);
  const char *ampm = (t->tm_hour >= 12) ? "PM" : "AM";

  const char *shortDays[] = { "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT" };
  const char *shortMonths[] = { "JAN", "FEB", "MAR", "APR", "MAY", "JUN", "JUL", "AUG", "SEP", "OCT", "NOV", "DEC" };
  int wday = (t->tm_wday >= 0 && t->tm_wday <= 6) ? t->tm_wday : 0;
  int mon  = (t->tm_mon >= 0 && t->tm_mon <= 11) ? t->tm_mon : 0;

  // 1. Top Header: Day & Date banner centered (e.g. "SUN  *  08 NOV")
  char topHeader[32];
  snprintf(topHeader, sizeof(topHeader), "%s  *  %02d %s", shortDays[wday], t->tm_mday, shortMonths[mon]);
  display.setTextSize(1);
  int headerW = strlen(topHeader) * 6;
  int headerX = xOffset + (128 - headerW) / 2;
  display.setCursor(headerX, 4);
  display.print(topHeader);

  // 2. Main Row: Balanced Circular Digits (HH : MM)
  int digW = 17;
  int digH = 33;
  int digStroke = 4;
  int yDig = 18;

  // Hour digits
  drawCircularDigit(xOffset + 8, yDig, hourBuf[0], digW, digH, digStroke);
  drawCircularDigit(xOffset + 28, yDig, hourBuf[1], digW, digH, digStroke);

  // Blinking circular colon dots (500ms ON / 500ms OFF)
  bool colonOn = (millis() % 1000 < 500);
  if (colonOn) {
    display.fillCircle(xOffset + 49, yDig + 10, 2, SSD1306_WHITE);
    display.fillCircle(xOffset + 49, yDig + 23, 2, SSD1306_WHITE);
  }

  // Minute digits
  drawCircularDigit(xOffset + 55, yDig, minBuf[0], digW, digH, digStroke);
  drawCircularDigit(xOffset + 75, yDig, minBuf[1], digW, digH, digStroke);

  // 3. Right Side: AM/PM Pill Badge + Live Circular Seconds
  display.fillRoundRect(xOffset + 98, 20, 24, 12, 3, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(xOffset + 104, 22);
  display.print(ampm);
  display.setTextColor(SSD1306_WHITE);

  // Live Seconds in mini circular digits
  drawCircularDigit(xOffset + 99, 36, secBuf[0], 9, 15, 2);
  drawCircularDigit(xOffset + 112, 36, secBuf[1], 9, 15, 2);

  display.setTextWrap(true);
}

// Slide 2: DATE (Circular font numbers, elegant badge & pearl separator)
void drawSlide2_Date(int xOffset, struct tm *t) {
  display.setTextWrap(false);
  display.setTextColor(SSD1306_WHITE);

  const char *fullMonths[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
  };
  const char *fullDays[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
  };

  int mon  = (t->tm_mon >= 0 && t->tm_mon <= 11) ? t->tm_mon : 0;
  int wday = (t->tm_wday >= 0 && t->tm_wday <= 6) ? t->tm_wday : 0;
  int year = t->tm_year + 1900;

  // 1. Top: Day of Week in rounded pill badge
  const char *dayName = fullDays[wday];
  int dayLen = strlen(dayName);
  int badgeW = dayLen * 6 + 14;
  int badgeX = xOffset + (128 - badgeW) / 2;
  display.fillRoundRect(badgeX, 2, badgeW, 12, 3, SSD1306_WHITE);
  display.setTextColor(SSD1306_BLACK, SSD1306_WHITE);
  display.setTextSize(1);
  display.setCursor(badgeX + 7, 4);
  display.print(dayName);
  display.setTextColor(SSD1306_WHITE);

  // 2. Middle Left: Big Circular Day of Month (e.g. 08)
  char dayBuf[4];
  snprintf(dayBuf, sizeof(dayBuf), "%02d", t->tm_mday);
  drawCircularDigit(xOffset + 14, 18, dayBuf[0], 18, 33, 4);
  drawCircularDigit(xOffset + 36, 18, dayBuf[1], 18, 33, 4);

  // 3. Middle Right: Full Month Name & Year in Circular Digits
  display.setTextSize(1);
  display.setCursor(xOffset + 63, 20);
  display.print(fullMonths[mon]);

  // Year in circular digits
  char yearBuf[6];
  snprintf(yearBuf, sizeof(yearBuf), "%04d", year);
  drawCircularDigit(xOffset + 63, 33, yearBuf[0], 10, 16, 2);
  drawCircularDigit(xOffset + 77, 33, yearBuf[1], 10, 16, 2);
  drawCircularDigit(xOffset + 91, 33, yearBuf[2], 10, 16, 2);
  drawCircularDigit(xOffset + 105, 33, yearBuf[3], 10, 16, 2);

  // 4. Bottom: Decorative separator with center pearl
  display.drawFastHLine(xOffset + 10, 56, 48, SSD1306_WHITE);
  display.fillCircle(xOffset + 64, 56, 2, SSD1306_WHITE);
  display.drawFastHLine(xOffset + 70, 56, 48, SSD1306_WHITE);

  display.setTextWrap(true);
}

// ==========================================
// DIWALI CELEBRATION ANIMATION (5 Phases)
// ==========================================
// ==========================================
// DIWALI ANIMATION: Realistic Diya & Progressive Animated Rangoli
// 1. Stylish "HAPPY DIWALI" Typography
// 2. Realistic Clay Diya with Slow-Motion Flickering Flame & Halo
// 3. Step-by-Step Animated Rangoli Construction (Center -> Full Mandala)
// 4. Grand Completed Celebration
// ==========================================
void drawDiwaliFace(int frame) {
  display.clearDisplay();

  // 11.0 second complete cycle using millis (smooth 25 FPS time-base)
  unsigned long t = millis() % 11000; // 0 to 10999 ms

  if (t < 2200) {
    // ========================================================
    // SECTION 1: STYLISH "HAPPY DIWALI" TYPOGRAPHY (0 to 2200ms)
    // Clean, bold, elegant font with festive ornamental borders
    // ========================================================
    display.setTextColor(SSD1306_WHITE);

    // Ornate Top & Bottom Divider Lines with center lotus pearl
    display.drawFastHLine(16, 4, 96, SSD1306_WHITE);
    display.fillCircle(64, 4, 2, SSD1306_WHITE);
    display.drawPixel(64, 1, SSD1306_WHITE);
    display.drawPixel(64, 7, SSD1306_WHITE);

    display.drawFastHLine(16, 59, 96, SSD1306_WHITE);
    display.fillCircle(64, 59, 2, SSD1306_WHITE);
    display.drawPixel(64, 56, SSD1306_WHITE);
    display.drawPixel(64, 62, SSD1306_WHITE);

    // Corner decorative sparkles
    if (frame % 2 == 0) {
      display.drawPixel(6, 6, SSD1306_WHITE);
      display.drawPixel(121, 6, SSD1306_WHITE);
      display.drawPixel(6, 57, SSD1306_WHITE);
      display.drawPixel(121, 57, SSD1306_WHITE);
    }

    // Render "HAPPY" in FreeSansBold9pt7b
    display.setFont(&FreeSansBold9pt7b);
    display.setTextSize(1);
    int16_t x1, y1;
    uint16_t w1, h1;
    display.getTextBounds("HAPPY", 0, 0, &x1, &y1, &w1, &h1);
    int happyX = (128 - w1) / 2 - x1;
    display.setCursor(happyX, 24);
    display.print("HAPPY");

    // Render "DIWALI" in FreeSansBold9pt7b
    display.getTextBounds("DIWALI", 0, 0, &x1, &y1, &w1, &h1);
    int diwaliX = (128 - w1) / 2 - x1;
    display.setCursor(diwaliX, 48);
    display.print("DIWALI");

    // Reset font back to default system font
    display.setFont(NULL);

    // Twinkling festive side stars
    for (int i = 0; i < 4; i++) {
      int sx = (i < 2) ? (10 + i * 4) : (114 + (i - 2) * 4);
      int sy = 24 + ((i % 2 == 0) ? -6 : 14);
      if ((frame + i) % 3 < 2) display.drawPixel(sx, sy, SSD1306_WHITE);
    }

  } else if (t < 5200) {
    // ========================================================
    // SECTION 2: REALISTIC CLAY DIYA WITH SLOW-MOTION FLAME (2200 to 5200ms)
    // Terracotta clay diya: base, bowl, oil pool, cotton wick, & slow-motion breathing flame!
    // ========================================================
    unsigned long dt = t - 2200; // 0 to 2999 ms
    const int cx = 64, cy = 44;  // Center of Diya

    // 1. Terracotta Pedestal Base (Stand)
    display.fillRoundRect(cx - 10, cy + 9, 20, 4, 1, SSD1306_WHITE);
    display.drawFastHLine(cx - 13, cy + 13, 26, SSD1306_WHITE); // Bottom foot line

    // 2. Terracotta Clay Bowl (Mitti ka Deepak)
    // Smooth tapering earthen curve
    for (int dy = 0; dy <= 12; dy++) {
      float u = dy / 12.0f;
      // Width tapers from 23px at top (dy=0) down to 10px at bottom (dy=12)
      int w = (int)(23.0f - 13.0f * u * u);
      display.drawFastHLine(cx - w, cy - 3 + dy, 2 * w + 1, SSD1306_WHITE);
    }

    // Carved decorative clay band on bowl
    display.drawFastHLine(cx - 16, cy + 3, 33, SSD1306_BLACK);
    for (int d = -12; d <= 12; d += 4) {
      display.drawPixel(cx + d, cy + 5, SSD1306_BLACK);
    }

    // 3. Diya Top Rim & Inner Oil Reservoir
    display.fillRoundRect(cx - 24, cy - 5, 48, 5, 2, SSD1306_WHITE); // Top wide lip
    display.fillRoundRect(cx - 19, cy - 4, 38, 3, 1, SSD1306_BLACK); // Inner oil pool

    // 4. Cotton Wick
    display.drawFastVLine(cx - 1, cy - 10, 7, SSD1306_WHITE);
    display.drawFastVLine(cx, cy - 10, 7, SSD1306_WHITE);
    display.drawFastVLine(cx + 1, cy - 10, 7, SSD1306_WHITE);

    // 5. SLOW-MOTION FLICKERING FLAME
    // Slow, realistic breathing motion (~1.2 Hz)
    float sway = 2.4f * sin(dt * 0.0035f) + 0.8f * sin(dt * 0.008f);
    float flameH = 18.0f + 3.0f * sin(dt * 0.005f); // Flame stretches & breathes
    int wickY = cy - 10; // Top of wick

    // Draw realistic almond/teardrop shaped flame
    for (int h = 0; h <= (int)flameH; h++) {
      float v = (float)h / flameH; // 0.0 (base) to 1.0 (apex)
      // Teardrop width: widest at lower 35%, tapers to apex at top
      float w = 5.6f * sin(v * 3.14159f) * (1.3f - 0.6f * v);
      int fx = cx + (int)(sway * v * v); // Tip sways more than base
      int fy = wickY - h;

      if (w > 0.4f) {
        display.drawFastHLine(fx - (int)w, fy, (int)(2 * w) + 1, SSD1306_WHITE);
      } else {
        display.drawPixel(fx, fy, SSD1306_WHITE);
      }

      // Inner cooler core (dark silhouette near wick center)
      if (h >= 2 && h <= 8 && w > 1.8f) {
        display.drawFastHLine(fx - 1, fy, 3, SSD1306_BLACK);
      }
    }

    // 6. Radiant Glowing Halo (Pulsing Light Aura around flame)
    int auraR = 15 + (int)(3.0f * sin(dt * 0.006f));
    int auraY = wickY - 7;
    for (int a = 0; a < 16; a++) {
      float ang = a * 0.3927f;
      int ax = cx + (int)(auraR * cos(ang));
      int ay = auraY + (int)(auraR * 0.82f * sin(ang));
      if ((frame + a) % 2 == 0 && ax >= 0 && ax < 128 && ay >= 0 && ay < 64) {
        display.drawPixel(ax, ay, SSD1306_WHITE);
      }
    }

    // Micro-sparkles slowly drifting upward from flame tip
    for (int s = 0; s < 3; s++) {
      int spY = (wickY - (int)flameH - 2) - ((int)(dt * 0.02f + s * 8) % 18);
      int spX = cx + (int)(sway) + ((s % 2 == 0) ? -2 : 2) * (s + 1);
      if (spY >= 0 && spY < 64 && spX >= 0 && spX < 128) {
        display.drawPixel(spX, spY, SSD1306_WHITE);
      }
    }

    // Soft corner diya dots
    display.drawPixel(10, 52, SSD1306_WHITE);
    display.drawPixel(118, 52, SSD1306_WHITE);

  } else if (t < 9000) {
    // ========================================================
    // SECTION 3: STEP-BY-STEP ANIMATED RANGOLI CONSTRUCTION (5200 to 9000ms)
    // The sacred Mandala draws itself progressively: Center Lotus -> Inner Star -> Outer Scallops -> Pearls!
    // ========================================================
    unsigned long rt = t - 5200; // 0 to 3799 ms
    float prog = (float)rt / 3800.0f; // 0.0 to 1.0 (construction progress)
    const int cx = 64, cy = 32;

    // --- STEP 1: Center Lotus Blossom (prog: 0.0 to 0.30) ---
    display.fillCircle(cx, cy, 3, SSD1306_WHITE); // Central sacred Bindu

    float p1 = (prog < 0.30f) ? (prog / 0.30f) : 1.0f;
    int maxPetals = (int)(p1 * 8.9f); // Petals appear 1 by 1

    for (int i = 0; i < 8; i++) {
      if (i < maxPetals) {
        float ang = i * 0.7854f; // 45 degrees
        float dist = 7.0f * ((i == maxPetals - 1) ? (p1 * 8.0f - i) : 1.0f);
        if (dist > 7.0f) dist = 7.0f;

        int px = cx + (int)(dist * cos(ang));
        int py = cy + (int)(dist * 0.85f * sin(ang));
        display.fillCircle(px, py, 2, SSD1306_WHITE);
      }
    }

    // --- STEP 2: Concentric Mandala Ring & Star Peaks (prog: 0.25 to 0.60) ---
    if (prog > 0.25f) {
      float p2 = (prog < 0.60f) ? ((prog - 0.25f) / 0.35f) : 1.0f;

      // Inner ring expands
      int ring1R = (int)(9.0f + 4.0f * p2);
      display.drawCircle(cx, cy, ring1R, SSD1306_WHITE);

      // 8 Expanding Star Peaks radiating outward
      int peakLen = (int)(13.0f + 8.0f * p2); // Extends to 21px
      for (int i = 0; i < 8; i++) {
        float ang = i * 0.7854f + 0.3927f; // Offset by 22.5 deg
        int kx = cx + (int)(peakLen * cos(ang));
        int ky = cy + (int)(peakLen * 0.85f * sin(ang));

        // Draw radial peak line
        int inX = cx + (int)(ring1R * cos(ang));
        int inY = cy + (int)(ring1R * 0.85f * sin(ang));
        display.drawLine(inX, inY, kx, ky, SSD1306_WHITE);
        display.fillCircle(kx, ky, 1, SSD1306_WHITE);
      }
    }

    // --- STEP 3: Connecting Geometric Star Web & Outer Ring (prog: 0.55 to 0.85) ---
    if (prog > 0.55f) {
      float p3 = (prog < 0.85f) ? ((prog - 0.55f) / 0.30f) : 1.0f;

      // Connect adjacent star points to form complete Kolam Star
      int numLines = (int)(p3 * 8.5f);
      for (int i = 0; i < 8; i++) {
        if (i < numLines) {
          float ang1 = i * 0.7854f + 0.3927f;
          float ang2 = (i + 1) * 0.7854f + 0.3927f;
          int xA = cx + (int)(21 * cos(ang1));
          int yA = cy + (int)(21 * 0.85f * sin(ang1));
          int xB = cx + (int)(21 * cos(ang2));
          int yB = cy + (int)(21 * 0.85f * sin(ang2));
          display.drawLine(xA, yA, xB, yB, SSD1306_WHITE);
        }
      }

      // Outer circular boundary ring sweeping around
      int sweepAngle = (int)(p3 * 360.0f);
      for (int deg = 0; deg < sweepAngle; deg += 6) {
        float rad = deg * 0.01745f;
        int rx = cx + (int)(25 * cos(rad));
        int ry = cy + (int)(25 * 0.85f * sin(rad));
        display.drawPixel(rx, ry, SSD1306_WHITE);
      }
    }

    // --- STEP 4: 16 Auspicious Pearl Dots & Corner Diyas (prog: 0.80 to 1.0) ---
    if (prog > 0.80f) {
      float p4 = (prog - 0.80f) / 0.20f;
      int numDots = (int)(p4 * 16.5f);

      for (int i = 0; i < 16; i++) {
        if (i < numDots) {
          float ang = i * 0.3927f;
          int dx = cx + (int)(29 * cos(ang));
          int dy = cy + (int)(29 * 0.85f * sin(ang));
          if (dx >= 0 && dx < 128 && dy >= 0 && dy < 64) {
            display.drawPixel(dx, dy, SSD1306_WHITE);
          }
        }
      }

      // Corner festive lights
      if (p4 > 0.5f) {
        display.drawCircle(10, 10, 2, SSD1306_WHITE);
        display.drawCircle(118, 10, 2, SSD1306_WHITE);
        display.drawCircle(10, 54, 2, SSD1306_WHITE);
        display.drawCircle(118, 54, 2, SSD1306_WHITE);
      }
    }

  } else {
    // ========================================================
    // SECTION 4: GRAND COMPLETED RANGOLI WITH GLOWING CENTER DIYA (9000 to 11000ms)
    // Full Rangoli is complete, and the realistic Diya glows at the center!
    // ========================================================
    const int cx = 64, cy = 32;

    // Full 8-Point Sacred Mandala Pattern
    display.drawCircle(cx, cy, 14, SSD1306_WHITE);
    display.drawCircle(cx, cy, 25, SSD1306_WHITE);

    // 8 Petals
    for (int i = 0; i < 8; i++) {
      float ang = i * 0.7854f;
      int px = cx + (int)(18 * cos(ang));
      int py = cy + (int)(18 * 0.85f * sin(ang));
      display.fillCircle(px, py, 2, SSD1306_WHITE);

      // Star peak lines
      float a1 = i * 0.7854f + 0.3927f;
      float a2 = (i + 1) * 0.7854f + 0.3927f;
      int xA = cx + (int)(21 * cos(a1));
      int yA = cy + (int)(21 * 0.85f * sin(a1));
      int xB = cx + (int)(21 * cos(a2));
      int yB = cy + (int)(21 * 0.85f * sin(a2));
      display.drawLine(xA, yA, xB, yB, SSD1306_WHITE);
    }

    // 16 Outer Sacred Pearls
    for (int i = 0; i < 16; i++) {
      float ang = i * 0.3927f;
      int dx = cx + (int)(29 * cos(ang));
      int dy = cy + (int)(29 * 0.85f * sin(ang));
      if (dx >= 0 && dx < 128 && dy >= 0 && dy < 64) {
        display.drawPixel(dx, dy, SSD1306_WHITE);
      }
    }

    // Glowing Diya at Center of Rangoli
    display.fillRoundRect(cx - 8, cy + 1, 16, 5, 2, SSD1306_WHITE); // Clay bowl
    display.fillRoundRect(cx - 6, cy + 2, 12, 2, 1, SSD1306_BLACK); // Oil pool
    display.drawFastVLine(cx, cy - 3, 4, SSD1306_WHITE);            // Wick

    // Flickering slow flame
    float sway = 1.8f * sin(millis() * 0.005f);
    int flameH = 9 + (int)(2.0f * sin(millis() * 0.007f));
    display.fillTriangle(cx + (int)sway, cy - 3 - flameH, cx - 3, cy - 3, cx + 3, cy - 3, SSD1306_WHITE);
    display.drawPixel(cx + (int)sway, cy - 4 - flameH, SSD1306_WHITE);

    // Corner festive diya lamps
    for (int c = 0; c < 4; c++) {
      int cornerX = (c % 2 == 0) ? 12 : 116;
      int cornerY = (c < 2) ? 12 : 52;
      display.fillRoundRect(cornerX - 4, cornerY, 8, 3, 1, SSD1306_WHITE);
      display.drawPixel(cornerX, cornerY - 2, SSD1306_WHITE);
      if ((frame + c) % 2 == 0) display.drawPixel(cornerX, cornerY - 3, SSD1306_WHITE);
    }

    // Celestial celebration sparkles
    for (int s = 0; s < 8; s++) {
      int sx = 8 + (s * 17 + frame * 3) % 112;
      int sy = 6 + (s * 13 + frame * 2) % 52;
      if ((sx < cx - 32 || sx > cx + 32) && ((frame + s) % 3 < 2)) {
        display.drawPixel(sx, sy, SSD1306_WHITE);
      }
    }
  }
}

// ==========================================
// LORD RAM ANIMATION: Cinematic Slow-Motion Archery with Aerial Firework Finale
// Ram Ji on LEFT draws Kodanda Bow -> Arrow glides in SLOW MOTION -> Explodes into Glorious Fireworks!
// ==========================================
void drawRamFace(int frame) {
  display.clearDisplay();

  // Smooth 25 FPS time-base using millis (6.8 second complete loop)
  unsigned long t = millis() % 6800; // 0 to 6799 ms

  // Ram Ji Position on LEFT
  const int bx = 14; // Base X
  const int by = 5;  // Base Y

  // Stages:
  // Stage 1 (0 to 1100ms): Stance & Nock Arrow
  // Stage 2 (1100 to 2200ms): Draw Bowstring & Flex Bow
  // Stage 3 (2200 to 2900ms): Divine Astra Charge at Arrowhead
  // Stage 4 (2900 to 5000ms): Release & Slow-Motion Arrow Flight Across Sky (2.1s)
  // Stage 5 (5000 to 6800ms): Breathtaking Multi-Phase Firework Spectacle (1.8s)
  int stage = 1;
  if (t < 1100) stage = 1;
  else if (t < 2200) stage = 2;
  else if (t < 2900) stage = 3;
  else if (t < 5000) stage = 4;
  else stage = 5;

  // Compute Bowstring Pull Position (nockX)
  int nockX = 26; // Resting string position at x = 26
  if (stage == 1) {
    nockX = 26;
  } else if (stage == 2) {
    float p = (t - 1100) / 1100.0f; // 0.0 to 1.0
    float ease = p * p * (3.0f - 2.0f * p);
    nockX = (int)(26.0f - 12.0f * ease); // Pulled back from 26 to 14
  } else if (stage == 3) {
    nockX = 14; // Full draw hold
  } else {
    nockX = 26; // Released
  }

  // ----------------------------------------------------
  // 1. ENVIRONMENT: Ground, Drifting Clouds & Twinkling Stars
  // ----------------------------------------------------
  // Ground line
  display.drawFastHLine(0, 54, 128, SSD1306_WHITE);
  // Grass tufts
  display.drawPixel(4, 53, SSD1306_WHITE);
  display.drawPixel(28, 53, SSD1306_WHITE);
  display.drawPixel(58, 53, SSD1306_WHITE);
  display.drawPixel(92, 53, SSD1306_WHITE);
  display.drawPixel(122, 53, SSD1306_WHITE);

  // Soft drifting clouds in the sky
  int cloud1X = (int)((t / 90) % 160) - 20;
  if (cloud1X >= -20 && cloud1X < 120) {
    display.drawFastHLine(cloud1X, 6, 14, SSD1306_WHITE);
    display.drawFastHLine(cloud1X + 3, 5, 8, SSD1306_WHITE);
  }
  int cloud2X = (int)(((t + 3400) / 110) % 170) - 20;
  if (cloud2X >= -20 && cloud2X < 120) {
    display.drawFastHLine(cloud2X, 12, 12, SSD1306_WHITE);
    display.drawFastHLine(cloud2X + 2, 11, 7, SSD1306_WHITE);
  }

  // Twinkling celestial stars (sparkle subtly)
  if ((frame % 3) != 0) display.drawPixel(48, 4, SSD1306_WHITE);
  if ((frame % 4) != 1) display.drawPixel(78, 9, SSD1306_WHITE);
  if ((frame % 3) != 2) display.drawPixel(108, 4, SSD1306_WHITE);
  if ((frame % 5) != 0) display.drawPixel(124, 12, SSD1306_WHITE);

  // ----------------------------------------------------
  // 2. BHAGWAN RAM SILHOUETTE (Standing proudly on LEFT)
  // ----------------------------------------------------
  // Divine Halo (Prabhamandal) around head
  int haloR = (stage == 3) ? 10 : 9;
  display.drawCircle(bx + 4, by + 12, haloR, SSD1306_WHITE);
  if (stage == 3 && (frame % 2 == 0)) {
    // Energy charge rays from halo in Stage 3
    display.drawPixel(bx + 4, by + 12 - 12, SSD1306_WHITE);
    display.drawPixel(bx + 4 - 12, by + 12, SSD1306_WHITE);
    display.drawPixel(bx + 4 + 12, by + 12, SSD1306_WHITE);
  }

  // High Golden Crown (Kirit Mukut) with peak pearl
  display.fillTriangle(bx + 4, by - 1, bx, by + 6, bx + 8, by + 6, SSD1306_WHITE);
  display.fillRect(bx - 1, by + 6, 11, 2, SSD1306_WHITE); // Crown base band
  display.drawPixel(bx + 4, by - 3, SSD1306_WHITE);       // Peak jewel/pearl

  // Head & Divine Tilak
  display.fillCircle(bx + 4, by + 12, 4, SSD1306_WHITE);
  display.drawFastVLine(bx + 4, by + 10, 3, SSD1306_BLACK); // Urdhva Pundra Tilak
  // Flowing Royal Hair behind
  display.fillRect(bx - 2, by + 13, 3, 7, SSD1306_WHITE);
  // Kundal (Earring)
  display.drawPixel(bx + 8, by + 13, SSD1306_WHITE);

  // Neck & Broad Torso
  display.fillRect(bx + 2, by + 17, 5, 3, SSD1306_WHITE);
  display.fillRect(bx, by + 20, 9, 14, SSD1306_WHITE);
  // Royal Sash / Angavastra across chest
  display.drawLine(bx, by + 20, bx + 8, by + 30, SSD1306_BLACK);

  // Tarkash (Quiver with arrows) over back
  display.fillRect(bx - 5, by + 19, 4, 11, SSD1306_WHITE);
  display.drawLine(bx - 4, by + 19, bx - 7, by + 12, SSD1306_WHITE); // Arrow 1
  display.drawLine(bx - 2, by + 19, bx - 5, by + 13, SSD1306_WHITE); // Arrow 2
  display.drawPixel(bx - 7, by + 11, SSD1306_WHITE);                  // Arrow feather
  display.drawPixel(bx - 5, by + 12, SSD1306_WHITE);

  // Dhoti / Pitambar & Legs
  display.fillRect(bx - 1, by + 34, 11, 3, SSD1306_WHITE); // Kamarbandh belt
  display.fillRect(bx, by + 37, 4, 15, SSD1306_WHITE);     // Left leg
  display.fillRect(bx + 5, by + 37, 4, 15, SSD1306_WHITE); // Right leg
  display.fillRect(bx - 1, by + 52, 5, 2, SSD1306_WHITE);  // Left paduka
  display.fillRect(bx + 5, by + 52, 5, 2, SSD1306_WHITE);  // Right paduka

  // Left Arm: Holds the bow forward firmly at grip (31, 32)
  display.drawLine(bx + 8, by + 22, 31, 32, SSD1306_WHITE);
  display.drawLine(bx + 8, by + 23, 31, 33, SSD1306_WHITE);
  display.fillCircle(31, 32, 2, SSD1306_WHITE); // Left hand clasping bow

  // Right Arm: Dynamic drawing motion!
  if (stage == 1) {
    // Hand placing arrow on string
    display.drawLine(bx + 4, by + 24, 25, 32, SSD1306_WHITE);
    display.fillCircle(25, 32, 1, SSD1306_WHITE);
  } else if (stage == 2 || stage == 3) {
    // Right arm pulls string back towards chest/ear!
    int elbowX = bx - 2;
    int elbowY = by + 25;
    display.drawLine(bx + 2, by + 22, elbowX, elbowY, SSD1306_WHITE); // Upper arm
    display.drawLine(elbowX, elbowY, nockX, 32, SSD1306_WHITE);        // Forearm to nock
    display.fillCircle(nockX, 32, 2, SSD1306_WHITE);                   // Hand pulling string
  } else {
    // Released follow-through pose (hand opened gracefully)
    display.drawLine(bx + 2, by + 22, bx + 10, by + 28, SSD1306_WHITE);
    display.drawPixel(bx + 11, by + 29, SSD1306_WHITE);
  }

  // ----------------------------------------------------
  // 3. Kodanda Dhanush (Authentic Recurve Bow on Left)
  // ----------------------------------------------------
  int bowTopY = 12, bowBotY = 52;
  int tipX = 26;

  // Stave flex: grip pulls back slightly under high draw tension
  int gripX = (stage == 2 || stage == 3) ? 30 : 31;

  // Draw authentic Kodanda recurve stave
  for (int dy = 0; dy <= 20; dy++) {
    float u = dy / 20.0f;
    // Arches forward to x=34 at mid-limb, recurves to x=26 at tips
    int wx = (int)(gripX + 13.0f * u - 18.0f * u * u + 0.5f);

    // Upper limb
    int yUp = 32 - dy;
    if (yUp >= bowTopY) {
      display.drawPixel(wx, yUp, SSD1306_WHITE);
      display.drawPixel(wx - 1, yUp, SSD1306_WHITE); // 2px thickness
    }
    // Lower limb
    int yDown = 32 + dy;
    if (yDown <= bowBotY) {
      display.drawPixel(wx, yDown, SSD1306_WHITE);
      display.drawPixel(wx - 1, yDown, SSD1306_WHITE); // 2px thickness
    }
  }

  // Horn Tips (Koti notches)
  display.drawPixel(tipX - 1, bowTopY, SSD1306_WHITE);
  display.drawPixel(tipX, bowTopY - 1, SSD1306_WHITE);
  display.drawPixel(tipX - 1, bowBotY, SSD1306_WHITE);
  display.drawPixel(tipX, bowBotY + 1, SSD1306_WHITE);

  // Bow Grip wrap
  display.fillRect(gripX - 2, 30, 3, 5, SSD1306_WHITE);

  // ----------------------------------------------------
  // 4. BOWSTRING (Pratyancha) ANIMATION
  // ----------------------------------------------------
  if (stage == 2 || stage == 3) {
    // String is PULLED BACK into dynamic sharp '>' angle by right hand!
    display.drawLine(tipX, bowTopY, nockX, 32, SSD1306_WHITE);
    display.drawLine(tipX, bowBotY, nockX, 32, SSD1306_WHITE);
  } else {
    // String is straight taut at resting position
    // Subtle recoil vibration in the first 160ms of release
    int recoil = (stage == 4 && (t - 2900 < 160)) ? (((frame % 2 == 0) ? 1 : -1)) : 0;
    display.drawLine(tipX + recoil, bowTopY, tipX + recoil, bowBotY, SSD1306_WHITE);
  }

  // ----------------------------------------------------
  // 5. THE ARROW (DIVYA BAAN) & SLOW-MOTION FLIGHT
  // ----------------------------------------------------
  if (stage == 1) {
    // Arrow placed on resting bow
    display.drawFastHLine(nockX, 32, 22, SSD1306_WHITE);
    display.fillTriangle(nockX + 26, 32, nockX + 21, 29, nockX + 21, 35, SSD1306_WHITE);

  } else if (stage == 2) {
    // Arrow being drawn back with string
    display.drawFastHLine(nockX, 32, 24, SSD1306_WHITE);
    display.drawFastHLine(nockX, 33, 24, SSD1306_WHITE);
    // Arrowhead
    display.fillTriangle(nockX + 28, 32, nockX + 23, 29, nockX + 23, 35, SSD1306_WHITE);
    // Tail fletchings at nock
    display.drawLine(nockX, 32, nockX - 3, 29, SSD1306_WHITE);
    display.drawLine(nockX, 33, nockX - 3, 36, SSD1306_WHITE);

  } else if (stage == 3) {
    // FULL DRAW: Arrow charged with Divine Radiant Aura!
    display.drawFastHLine(nockX, 32, 26, SSD1306_WHITE);
    display.drawFastHLine(nockX, 33, 26, SSD1306_WHITE);
    // Sharp arrowhead at x = 40
    int tipArrowX = nockX + 26;
    display.fillTriangle(tipArrowX + 6, 32, tipArrowX, 28, tipArrowX, 36, SSD1306_WHITE);
    // Tail fletchings
    display.drawLine(nockX, 32, nockX - 4, 28, SSD1306_WHITE);
    display.drawLine(nockX, 33, nockX - 4, 37, SSD1306_WHITE);

    // Divine Energy Charging: Pulsing radiance around arrowhead!
    int pulse = (frame % 4);
    display.drawCircle(tipArrowX + 6, 32, 3 + pulse, SSD1306_WHITE);
    display.drawPixel(tipArrowX + 11 + pulse, 32, SSD1306_WHITE);
    display.drawPixel(tipArrowX + 6, 32 - 5 - pulse, SSD1306_WHITE);
    display.drawPixel(tipArrowX + 6, 32 + 5 + pulse, SSD1306_WHITE);

  } else if (stage == 4) {
    // ========================================================
    // STAGE 4: SLOW MOTION FLIGHT ACROSS THE SKY (2.1 SECONDS)
    // ========================================================
    unsigned long flightTime = t - 2900; // 0 to 2099 ms
    float progress = (float)flightTime / 2100.0f; // 0.0 to 1.0

    // Smooth horizontal position across open sky (x: 36 -> 102)
    int arrowHeadX = 36 + (int)(progress * 66.0f);

    // Aerodynamic trajectory: gentle subtle arc in flight, rising towards explosion point
    int arrowY = 32 - (int)(10.0f * sin(progress * 1.57f)); // Rises smoothly from 32 to 22

    // Double-thick 20px straight golden shaft
    display.drawFastHLine(arrowHeadX - 20, arrowY, 20, SSD1306_WHITE);
    display.drawFastHLine(arrowHeadX - 20, arrowY + 1, 20, SSD1306_WHITE);

    // Sharp Barbed Arrowhead cutting through the air
    display.fillTriangle(arrowHeadX + 6, arrowY, arrowHeadX, arrowY - 4, arrowHeadX, arrowY + 5, SSD1306_WHITE);
    display.fillTriangle(arrowHeadX, arrowY - 1, arrowHeadX + 3, arrowY, arrowHeadX, arrowY + 2, SSD1306_BLACK);

    // Tail Feathers (Fletchings)
    display.drawLine(arrowHeadX - 20, arrowY, arrowHeadX - 24, arrowY - 4, SSD1306_WHITE);
    display.drawLine(arrowHeadX - 17, arrowY, arrowHeadX - 21, arrowY - 4, SSD1306_WHITE);
    display.drawLine(arrowHeadX - 20, arrowY + 1, arrowHeadX - 24, arrowY + 5, SSD1306_WHITE);
    display.drawLine(arrowHeadX - 17, arrowY + 1, arrowHeadX - 21, arrowY + 5, SSD1306_WHITE);

    // Pulsing Divine Aura ring around flying arrowhead
    display.drawCircle(arrowHeadX + 6, arrowY, 2 + (frame % 3), SSD1306_WHITE);

    // Glowing Particle Exhaust Trail (Slow-motion sparkles drifting behind)
    for (int i = 1; i <= 6; i++) {
      int tx = arrowHeadX - 20 - i * 6;
      int ty = arrowY + ((i % 2 == 0) ? -1 : 1) * ((frame + i) % 3);
      if (tx >= 28 && tx < 128) {
        display.drawPixel(tx, ty, SSD1306_WHITE);
        display.drawPixel(tx + 1, arrowY, SSD1306_WHITE);
      }
    }

  } else {
    // ========================================================
    // STAGE 5: SPECTACULAR MULTI-PHASE FIREWORK FINALE (1.8s)
    // No text! Pure, dazzling pyrotechnic fireworks in the sky!
    // ========================================================
    unsigned long fwTime = t - 5000; // 0 to 1799 ms
    const int cx = 100, cy = 22;     // Main firework explosion center

    if (fwTime < 250) {
      // 1. Initial Blast Flash & Expanding Shockwave
      int flashR = (int)((fwTime / 250.0f) * 14.0f);
      display.fillCircle(cx, cy, 3, SSD1306_WHITE);
      display.drawCircle(cx, cy, flashR, SSD1306_WHITE);
      if (flashR > 4) display.drawCircle(cx, cy, flashR - 4, SSD1306_WHITE);

      // Flash spikes
      for (int a = 0; a < 8; a++) {
        float ang = a * 0.785f;
        int sx = cx + (int)((flashR + 4) * cos(ang));
        int sy = cy + (int)((flashR + 4) * sin(ang));
        display.drawLine(cx, cy, sx, sy, SSD1306_WHITE);
      }

    } else if (fwTime < 1200) {
      // 2. Full Chrysanthemum Starburst Explosion (16-Point Radial Rays)
      float prog = (fwTime - 250) / 950.0f; // 0.0 to 1.0
      float ease = prog * (2.0f - prog);    // Decelerating expansion
      float maxDist = 28.0f * ease;
      float gravity = 6.0f * prog * prog;   // Gentle gravity droop

      // 16 radial streaming firework arms
      for (int i = 0; i < 16; i++) {
        float ang = i * 0.3927f; // 2 * PI / 16
        float vx = cos(ang);
        float vy = sin(ang);

        // Organic speed variation across arms
        float spd = 0.85f + 0.3f * ((i * 7) % 5) / 4.0f;
        float d = maxDist * spd;

        int px = cx + (int)(d * vx);
        int py = cy + (int)(d * 0.82f * vy + gravity);

        if (px >= 0 && px < 128 && py >= 0 && py < 54) {
          // Leading bright ember (2x2)
          display.fillRect(px, py, 2, 2, SSD1306_WHITE);

          // Sparkling tail sparks behind ember
          if (d > 7.0f) {
            int tx1 = cx + (int)((d - 5.0f) * vx);
            int ty1 = cy + (int)((d - 5.0f) * 0.82f * vy + gravity * 0.75f);
            if (tx1 >= 0 && tx1 < 128 && ty1 >= 0 && ty1 < 54)
              display.drawPixel(tx1, ty1, SSD1306_WHITE);
          }
          if (d > 14.0f && ((frame + i) % 2 == 0)) {
            int tx2 = cx + (int)((d - 11.0f) * vx);
            int ty2 = cy + (int)((d - 11.0f) * 0.82f * vy + gravity * 0.5f);
            if (tx2 >= 0 && tx2 < 128 && ty2 >= 0 && ty2 < 54)
              display.drawPixel(tx2, ty2, SSD1306_WHITE);
          }
        }
      }

      // Secondary Satellite Micro-Bursts popping around the main shell
      if (fwTime >= 500) {
        // Micro-burst 1 at top left (76, 14)
        int mb1R = (int)(((fwTime - 500) / 700.0f) * 8.0f);
        display.drawCircle(76, 14, mb1R, SSD1306_WHITE);
        display.drawPixel(76 + mb1R + 1, 14, SSD1306_WHITE);
        display.drawPixel(76 - mb1R - 1, 14, SSD1306_WHITE);
        display.drawPixel(76, 14 - mb1R - 1, SSD1306_WHITE);
        display.drawPixel(76, 14 + mb1R + 1, SSD1306_WHITE);
      }
      if (fwTime >= 700) {
        // Micro-burst 2 at bottom right (118, 32)
        int mb2R = (int)(((fwTime - 700) / 500.0f) * 7.0f);
        display.drawCircle(118, 32, mb2R, SSD1306_WHITE);
        display.drawPixel(118 + mb2R, 32, SSD1306_WHITE);
        display.drawPixel(118 - mb2R, 32, SSD1306_WHITE);
        display.drawPixel(118, 32 - mb2R, SSD1306_WHITE);
        display.drawPixel(118, 32 + mb2R, SSD1306_WHITE);
      }

    } else {
      // 3. Falling Golden Willow (Kamuro Glittering Rain Effect)
      // Embers slowly drift downward like waterfall of twinkling sparks
      unsigned long fallTime = fwTime - 1200; // 0 to 599 ms
      float fallProg = fallTime / 600.0f;

      for (int i = 0; i < 22; i++) {
        // Spread across firework area
        int sx = cx - 24 + ((i * 11) % 52) + (int)(2.0f * sin(i * 1.3f + frame * 0.4f));
        int sy = cy + 2 + (int)(fallProg * (24.0f + ((i * 5) % 18)));

        if (sx >= 0 && sx < 128 && sy >= 0 && sy < 54) {
          // Twinkling sparkle effect (flickers naturally)
          if ((frame + i) % 3 < 2) {
            display.drawPixel(sx, sy, SSD1306_WHITE);
            // Little sparkle cross on select embers
            if ((i % 4 == 0) && (fallTime < 400)) {
              display.drawPixel(sx - 1, sy, SSD1306_WHITE);
              display.drawPixel(sx + 1, sy, SSD1306_WHITE);
              display.drawPixel(sx, sy - 1, SSD1306_WHITE);
              display.drawPixel(sx, sy + 1, SSD1306_WHITE);
            }
          }
        }
      }
    }
  }
}

// ==========================================
// FIREWORKS / PATAKE ANIMATION (5 Phases)
// ==========================================
void drawFireworksFace(int frame) {
  display.clearDisplay();
  int phase = (frame / 25) % 5;

  if (phase == 0) {
    // Phase 1: Skyshot rocket launching + explosion at top
    int progress = frame % 25;
    int rocketY;

    if (progress < 15) {
      // Rocket flying up
      rocketY = 60 - (progress * 4);
      // Rocket body (small rectangle)
      display.fillRect(62, rocketY, 4, 8, SSD1306_WHITE);
      // Nose cone (triangle)
      display.fillTriangle(60, rocketY, 64, rocketY - 5, 68, rocketY, SSD1306_WHITE);
      // Exhaust trail
      for (int t = 1; t <= 6; t++) {
        int ty = rocketY + 8 + t * 3;
        if (ty < 64 && (frame + t) % 2 == 0) {
          display.drawPixel(63 + (t % 3 - 1), ty, SSD1306_WHITE);
          display.drawPixel(64, ty, SSD1306_WHITE);
        }
      }
      // Tail flame
      int flk = frame % 3;
      display.fillCircle(64, rocketY + 10, 2 + flk, SSD1306_WHITE);
    } else {
      // Explosion burst at top!
      int burstFrame = progress - 15;
      int burstR = burstFrame * 3 + 2;
      int bx2 = 64, by2 = 10;
      // Radial lines exploding outward
      for (int i = 0; i < 12; i++) {
        int dx = 0, dy = 0;
        switch (i % 12) {
          case 0: dx = burstR; dy = 0; break;
          case 1: dx = burstR * 87 / 100; dy = -burstR / 2; break;
          case 2: dx = burstR / 2; dy = -burstR * 87 / 100; break;
          case 3: dx = 0; dy = -burstR; break;
          case 4: dx = -burstR / 2; dy = -burstR * 87 / 100; break;
          case 5: dx = -burstR * 87 / 100; dy = -burstR / 2; break;
          case 6: dx = -burstR; dy = 0; break;
          case 7: dx = -burstR * 87 / 100; dy = burstR / 2; break;
          case 8: dx = -burstR / 2; dy = burstR * 87 / 100; break;
          case 9: dx = 0; dy = burstR; break;
          case 10: dx = burstR / 2; dy = burstR * 87 / 100; break;
          case 11: dx = burstR * 87 / 100; dy = burstR / 2; break;
        }
        int ex = bx2 + dx;
        int ey = by2 + dy;
        if (ex >= 0 && ex < 128 && ey >= 0 && ey < 64) {
          display.drawLine(bx2, by2, ex, ey, SSD1306_WHITE);
          display.fillCircle(ex, ey, 1, SSD1306_WHITE);
        }
      }
      // Center flash
      if (burstFrame < 3)
        display.fillCircle(bx2, by2, 4 - burstFrame, SSD1306_WHITE);
    }

    // Ground line
    display.drawFastHLine(0, 62, 128, SSD1306_WHITE);

  } else if (phase == 1) {
    // Phase 2: Multiple burst explosions at random positions
    // Use frame-seeded pseudo-random positions
    for (int b = 0; b < 3; b++) {
      int bx2 = 20 + (b * 43 + (frame / 4) * 17) % 88;
      int by2 = 8 + (b * 19 + (frame / 4) * 7) % 30;
      int age = (frame + b * 8) % 12;

      if (age < 10) {
        int r = age * 2 + 2;
        // Expanding particle circle
        for (int i = 0; i < 8; i++) {
          int dx = 0, dy = 0;
          switch (i) {
            case 0: dx = r; dy = 0; break;
            case 1: dx = r * 7 / 10; dy = -r * 7 / 10; break;
            case 2: dx = 0; dy = -r; break;
            case 3: dx = -r * 7 / 10; dy = -r * 7 / 10; break;
            case 4: dx = -r; dy = 0; break;
            case 5: dx = -r * 7 / 10; dy = r * 7 / 10; break;
            case 6: dx = 0; dy = r; break;
            case 7: dx = r * 7 / 10; dy = r * 7 / 10; break;
          }
          int px = bx2 + dx;
          int py = by2 + dy;
          if (px >= 0 && px < 128 && py >= 0 && py < 64) {
            display.drawPixel(px, py, SSD1306_WHITE);
            // Trailing dots
            if (age < 6 && px + 1 < 128) display.drawPixel(px + 1, py, SSD1306_WHITE);
          }
        }
        // Center flash for new bursts
        if (age < 3)
          display.fillCircle(bx2, by2, 3 - age, SSD1306_WHITE);
      }
    }

    // Additional scattered sparkle particles
    for (int i = 0; i < 20; i++) {
      int sx = (i * 7 + frame * 5) % 128;
      int sy = (i * 13 + frame * 3) % 50 + 5;
      if ((frame + i) % 4 < 2)
        display.drawPixel(sx, sy, SSD1306_WHITE);
    }

    // Ground
    display.drawFastHLine(0, 62, 128, SSD1306_WHITE);

  } else if (phase == 2) {
    // Phase 3: Sparkler animation - radiating random short lines from center
    int cx = 64, cy = 32;

    // Core bright point
    display.fillCircle(cx, cy, 2, SSD1306_WHITE);

    // Radiating sparks - different each frame for sparkler effect
    for (int i = 0; i < 16; i++) {
      // Pseudo-random direction and length based on frame + i
      int seed = (frame * 7 + i * 13) % 100;
      int len = 4 + seed % 12;
      int angle_idx = (frame * 3 + i * 23) % 16;
      int dx = 0, dy = 0;
      switch (angle_idx) {
        case 0:  dx = len; dy = 0; break;
        case 1:  dx = len * 92 / 100; dy = -len * 38 / 100; break;
        case 2:  dx = len * 71 / 100; dy = -len * 71 / 100; break;
        case 3:  dx = len * 38 / 100; dy = -len * 92 / 100; break;
        case 4:  dx = 0; dy = -len; break;
        case 5:  dx = -len * 38 / 100; dy = -len * 92 / 100; break;
        case 6:  dx = -len * 71 / 100; dy = -len * 71 / 100; break;
        case 7:  dx = -len * 92 / 100; dy = -len * 38 / 100; break;
        case 8:  dx = -len; dy = 0; break;
        case 9:  dx = -len * 92 / 100; dy = len * 38 / 100; break;
        case 10: dx = -len * 71 / 100; dy = len * 71 / 100; break;
        case 11: dx = -len * 38 / 100; dy = len * 92 / 100; break;
        case 12: dx = 0; dy = len; break;
        case 13: dx = len * 38 / 100; dy = len * 92 / 100; break;
        case 14: dx = len * 71 / 100; dy = len * 71 / 100; break;
        case 15: dx = len * 92 / 100; dy = len * 38 / 100; break;
      }
      int ex = cx + dx;
      int ey = cy + dy;
      if (ex >= 0 && ex < 128 && ey >= 0 && ey < 64) {
        display.drawLine(cx, cy, ex, ey, SSD1306_WHITE);
        // Bright tip
        display.drawPixel(ex, ey, SSD1306_WHITE);
      }
    }

    // Falling spark particles (gravity)
    for (int i = 0; i < 8; i++) {
      int px = cx + ((frame * 3 + i * 17) % 40) - 20;
      int py = cy + ((frame + i * 5) % 20);
      if (px >= 0 && px < 128 && py < 64 && (frame + i) % 3 < 2)
        display.drawPixel(px, py, SSD1306_WHITE);
    }

    // "SPARKLER" text
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(36, 56);
    display.print("SPARKLER");

  } else if (phase == 3) {
    // Phase 4: Ground fireworks (anar/flower pot) - fountain pattern
    int potX = 60, potY = 56;

    // Cone/pot shape at bottom
    display.fillTriangle(potX, potY, potX + 8, potY, potX + 4, potY - 10, SSD1306_WHITE);
    display.fillRect(potX - 2, potY, 12, 4, SSD1306_WHITE);

    // Ground line
    display.drawFastHLine(0, 60, 128, SSD1306_WHITE);

    // Particles shooting upward and falling down (fountain)
    for (int i = 0; i < 20; i++) {
      int seed = (i * 7 + 3);
      int spread = (seed % 11) - 5; // horizontal spread -5 to +5
      int upSpeed = 3 + (seed % 4);
      int age = (frame + i * 3) % 18;

      // Parabolic trajectory (up then down with gravity)
      int py = potY - 12 - (upSpeed * age) + (age * age / 3); // gravity pulls back
      int px = potX + 4 + (spread * age / 4);

      if (py >= 0 && py < 60 && px >= 0 && px < 128 && age < 16) {
        display.drawPixel(px, py, SSD1306_WHITE);
        // Thicker particles near source
        if (age < 6 && px + 1 < 128)
          display.drawPixel(px + 1, py, SSD1306_WHITE);
      }
    }

    // Bright flash at pot mouth
    int fl4 = frame % 3;
    display.fillCircle(potX + 4, potY - 11, 2 + fl4, SSD1306_WHITE);

    // Text
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(42, 2);
    display.print("~ ANAR ~");

  } else {
    // Phase 5: Grand finale - multiple rockets, bursts, ground sparkles, text
    // "HAPPY DIWALI" text at top
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(22, 0);
    display.print("HAPPY DIWALI!");

    // Multiple small rockets at different stages
    for (int r = 0; r < 3; r++) {
      int rx = 20 + r * 40;
      int stage = (frame + r * 8) % 20;

      if (stage < 10) {
        // Rocket going up
        int ry = 55 - stage * 4;
        display.fillRect(rx, ry, 3, 5, SSD1306_WHITE);
        display.fillTriangle(rx - 1, ry, rx + 1, ry - 3, rx + 4, ry, SSD1306_WHITE);
        // Trail
        for (int t = 1; t <= 3; t++) {
          if (ry + 5 + t * 3 < 60)
            display.drawPixel(rx + 1, ry + 5 + t * 3, SSD1306_WHITE);
        }
      } else {
        // Burst!
        int burstAge = stage - 10;
        int br = burstAge * 2 + 1;
        int by2 = 55 - 40;
        for (int i = 0; i < 8; i++) {
          int dx = 0, dy = 0;
          switch (i) {
            case 0: dx = br; dy = 0; break;
            case 1: dx = br * 7 / 10; dy = -br * 7 / 10; break;
            case 2: dx = 0; dy = -br; break;
            case 3: dx = -br * 7 / 10; dy = -br * 7 / 10; break;
            case 4: dx = -br; dy = 0; break;
            case 5: dx = -br * 7 / 10; dy = br * 7 / 10; break;
            case 6: dx = 0; dy = br; break;
            case 7: dx = br * 7 / 10; dy = br * 7 / 10; break;
          }
          int px = rx + 1 + dx;
          int py = by2 + dy;
          if (px >= 0 && px < 128 && py >= 8 && py < 60)
            display.drawPixel(px, py, SSD1306_WHITE);
        }
        if (burstAge < 2)
          display.fillCircle(rx + 1, by2, 2, SSD1306_WHITE);
      }
    }

    // Ground sparkles
    for (int i = 0; i < 128; i += 5) {
      if ((frame + i) % 4 < 2)
        display.drawPixel(i, 58 + (i % 3), SSD1306_WHITE);
    }

    // Ground firework at center
    int fAge = frame % 10;
    for (int i = 0; i < 6; i++) {
      int fpx = 64 + ((i * 5) - 15);
      int fpy = 56 - fAge * 2 + (fAge * fAge / 6);
      if (fpy >= 10 && fpy < 58 && fpx >= 0 && fpx < 128)
        display.drawPixel(fpx, fpy, SSD1306_WHITE);
    }

    // Ground line
    display.drawFastHLine(0, 61, 128, SSD1306_WHITE);
  }
}

// Master function: Draw Clock with Smooth Slide Transition
void drawClockFaceSlides() {
  display.clearDisplay();

  time_t nowSec;
  time(&nowSec);
  struct tm *t = gmtime(&nowSec);

  // 8-second total carousel loop:
  // 0 - 3500ms: Slide 1 (Time) stationary
  // 3500 - 4000ms: Smooth horizontal slide from Slide 1 -> Slide 2
  // 4000 - 7500ms: Slide 2 (Date) stationary
  // 7500 - 8000ms: Smooth horizontal slide from Slide 2 -> Slide 1
  unsigned long cycle = millis() % 8000;

  if (cycle < 3500) {
    drawSlide1_Time(0, t);
  } else if (cycle < 4000) {
    float p = (cycle - 3500) / 500.0f; // 0.0 to 1.0
    float ease = p * p * (3.0f - 2.0f * p); // Cubic ease in-out
    int shift = (int)(ease * 128.0f);
    drawSlide1_Time(-shift, t);
    drawSlide2_Date(128 - shift, t);
  } else if (cycle < 7500) {
    drawSlide2_Date(0, t);
  } else {
    float p = (cycle - 7500) / 500.0f; // 0.0 to 1.0
    float ease = p * p * (3.0f - 2.0f * p); // Cubic ease in-out
    int shift = (int)(ease * 128.0f);
    drawSlide2_Date(-shift, t);
    drawSlide1_Time(128 - shift, t);
  }
}

// Standalone TIME Face (Uses same modern design from user sketch)
void drawTimeFaceStandalone() {
  display.clearDisplay();
  time_t nowSec;
  time(&nowSec);
  struct tm *t = gmtime(&nowSec);
  drawSlide1_Time(0, t);
}

// Standalone DATE Face (Uses same modern design from user sketch)
void drawDateFaceStandalone() {
  display.clearDisplay();
  time_t nowSec;
  time(&nowSec);
  struct tm *t = gmtime(&nowSec);
  drawSlide2_Date(0, t);
}

// Master Face Renderer by Identifier Name
void renderFaceByName(const String &face) {
  if (face == "idle") {
    drawIdleFace(animFrame);
  } else if (face == "happy") {
    drawHappyFace(animFrame);
  } else if (face == "love") {
    drawLoveFace(animFrame);
  } else if (face == "wink") {
    drawWinkFace(animFrame);
  } else if (face == "cool") {
    drawCoolFace(animFrame);
  } else if (face == "sleepy") {
    drawSleepyFace(animFrame);
  } else if (face == "angry") {
    drawAngryFace(animFrame);
  } else if (face == "sad") {
    drawSadFace(animFrame);
  } else if (face == "surprised") {
    drawSurprisedFace(animFrame);
  } else if (face == "laughing") {
    drawLaughingFace(animFrame);
  } else if (face == "confused") {
    drawConfusedFace(animFrame);
  } else if (face == "dizzy") {
    drawDizzyFace(animFrame);
  } else if (face == "birthday") {
    drawBirthdayFace(animFrame);
  } else if (face == "diwali") {
    drawDiwaliFace(animFrame);
  } else if (face == "ram") {
    drawRamFace(animFrame);
  } else if (face == "fireworks") {
    drawFireworksFace(animFrame);
  } else if (face == "time") {
    drawTimeFaceStandalone();
  } else if (face == "date") {
    drawDateFaceStandalone();
  } else if (face == "clock") {
    drawClockFaceSlides();
  } else if (face == "torch" || face == "pixel_test") {
    drawTorchMode();
  } else {
    drawIdleFace(animFrame);
  }
}

// Hardware-level smooth horizontal slide compositing across pages
void renderSlideTransition(int shift) {
  uint8_t *screen = display.getBuffer();
  for (int page = 0; page < 8; page++) {
    int pageOffset = page * 128;
    for (int x = 0; x < 128; x++) {
      int srcA_x = x + shift;
      if (srcA_x >= 0 && srcA_x < 128) {
        screen[pageOffset + x] = slideBufA[pageOffset + srcA_x];
      } else {
        int srcB_x = x - (128 - shift);
        if (srcB_x >= 0 && srcB_x < 128) {
          screen[pageOffset + x] = slideBufB[pageOffset + srcB_x];
        } else {
          screen[pageOffset + x] = 0;
        }
      }
    }
  }
}

// Slideshow Engine: Cycles slides by duration with smooth slide transitions
void handleSlideshow() {
  if (playlistCount == 0) {
    renderFaceByName(currentFace);
    return;
  }

  unsigned long now = millis();

  if (inTransition) {
    unsigned long elapsed = now - transitionStartTime;
    if (elapsed >= TRANSITION_DURATION) {
      // Transition complete!
      inTransition = false;
      currentSlideIndex = transitionToIndex;
      slideStartTime = now;
      renderFaceByName(playlist[currentSlideIndex].face);
    } else {
      // Calculate cubic easing
      float p = (float)elapsed / (float)TRANSITION_DURATION;
      float ease = p * p * (3.0f - 2.0f * p);
      int shift = (int)(ease * 128.0f);
      renderSlideTransition(shift);
    }
  } else {
    unsigned long duration = (unsigned long)playlist[currentSlideIndex].durationSec * 1000UL;
    if (now - slideStartTime >= duration) {
      if (playlistCount > 1) {
        transitionFromIndex = currentSlideIndex;
        transitionToIndex = (currentSlideIndex + 1) % playlistCount;

        // Take snapshots of outgoing slide and incoming slide
        renderFaceByName(playlist[transitionFromIndex].face);
        memcpy(slideBufA, display.getBuffer(), 1024);

        renderFaceByName(playlist[transitionToIndex].face);
        memcpy(slideBufB, display.getBuffer(), 1024);

        inTransition = true;
        transitionStartTime = now;
        renderSlideTransition(0);
      } else {
        slideStartTime = now;
        renderFaceByName(playlist[currentSlideIndex].face);
      }
    } else {
      renderFaceByName(playlist[currentSlideIndex].face);
    }
  }
}

// ==========================================
// RENDER CURRENT FACE / ACTIVE MODE
// ==========================================
void renderCurrentFace() {
  if (slideshowActive) {
    handleSlideshow();
  } else {
    renderFaceByName(currentFace);
  }
  display.display();
}

// ==========================================
// NVS SETTINGS: Persistent Save / Load
// ==========================================
void saveSettings() {
  prefs.begin("mybot", false); // false = read-write
  prefs.putString("face", currentFace);
  prefs.putBool("slideshow", slideshowActive);
  prefs.putString("bdayName", bdayName);
  // Save full playlist as compact string: "face:dur,face:dur,..."
  String pl = "";
  for (int i = 0; i < playlistCount; i++) {
    if (i > 0) pl += ",";
    pl += playlist[i].face + ":" + String(playlist[i].durationSec);
  }
  prefs.putString("playlist", pl);
  prefs.end();
  Serial.printf("[NVS] Settings saved: face=%s slideshow=%d playlist=%s\n",
                currentFace.c_str(), slideshowActive, pl.c_str());
}

void loadSettings() {
  prefs.begin("mybot", true); // true = read-only
  currentFace    = prefs.getString("face", "idle");
  slideshowActive = prefs.getBool("slideshow", false);
  bdayName        = prefs.getString("bdayName", "BUDDY");
  String pl       = prefs.getString("playlist", "");
  prefs.end();

  // Parse playlist string
  if (pl.length() > 0) {
    playlistCount = 0;
    int start = 0;
    while (start < (int)pl.length() && playlistCount < MAX_SLIDES) {
      int comma = pl.indexOf(',', start);
      String token = (comma == -1) ? pl.substring(start) : pl.substring(start, comma);
      token.trim();
      if (token.length() > 0) {
        int colon = token.indexOf(':');
        if (colon != -1) {
          playlist[playlistCount].face        = token.substring(0, colon);
          playlist[playlistCount].durationSec = token.substring(colon + 1).toInt();
          if (playlist[playlistCount].durationSec < 1) playlist[playlistCount].durationSec = 3;
          playlistCount++;
        }
      }
      if (comma == -1) break;
      start = comma + 1;
    }
  }

  if (slideshowActive) {
    currentSlideIndex = 0;
    slideStartTime    = millis();
    inTransition      = false;
  }
  Serial.printf("[NVS] Loaded: face=%s slideshow=%d playlistCount=%d\n",
                currentFace.c_str(), slideshowActive, playlistCount);
}

// ==========================================
// WEB SERVER HANDLERS
// ==========================================
void handleRoot() { server.send(200, "text/html", INDEX_HTML); }

// Single Face Selector: Stops slideshow, locks face, and saves to NVS
void handleSetFace() {
  if (server.hasArg("face")) {
    currentFace = server.arg("face");
    if (server.hasArg("name")) {
      String n = server.arg("name");
      n.trim();
      if (n.length() > 10) n = n.substring(0, 10);
      if (n.length() > 0) bdayName = n;
    }
    slideshowActive = false;
    animFrame = 0;
    saveSettings(); // <<< Persistent save to NVS flash!
    Serial.printf("[WEB] Emotion set to: %s (Single Mode) — Saved to NVS\n", currentFace.c_str());
    server.send(200, "text/plain", "OK: " + currentFace);
  } else {
    server.send(400, "text/plain", "Missing face parameter");
  }
}

void handleSetBday() {
  if (server.hasArg("name")) {
    String n = server.arg("name");
    n.trim();
    if (n.length() > 10) n = n.substring(0, 10);
    if (n.length() > 0) bdayName = n;
    saveSettings();
    Serial.printf("[BDAY] Name saved to NVS: %s\n", bdayName.c_str());
    server.send(200, "text/plain", "OK: " + bdayName);
  } else {
    server.send(400, "text/plain", "Missing name");
  }
}

// Custom Playlist / Slideshow Setup — Saves to NVS
void handleSetSlides() {
  if (server.hasArg("active")) {
    slideshowActive = (server.arg("active") == "1");
    if (slideshowActive) {
      currentSlideIndex = 0;
      slideStartTime = millis();
      inTransition = false;
    }
  }
  if (server.hasArg("items")) {
    String itemsStr = server.arg("items");
    playlistCount = 0;
    int start = 0;
    while (start < (int)itemsStr.length() && playlistCount < MAX_SLIDES) {
      int comma = itemsStr.indexOf(',', start);
      String token = (comma == -1) ? itemsStr.substring(start) : itemsStr.substring(start, comma);
      token.trim();
      if (token.length() > 0) {
        int colon = token.indexOf(':');
        if (colon != -1) {
          playlist[playlistCount].face = token.substring(0, colon);
          playlist[playlistCount].durationSec = token.substring(colon + 1).toInt();
          if (playlist[playlistCount].durationSec < 1) playlist[playlistCount].durationSec = 3;
          playlistCount++;
        }
      }
      if (comma == -1) break;
      start = comma + 1;
    }
    currentSlideIndex = 0;
    slideStartTime = millis();
    inTransition = false;
    Serial.printf("[SLIDESHOW] Loaded %d slides. Active: %d\n", playlistCount, slideshowActive);
  }
  saveSettings(); // <<< Persistent save to NVS flash!
  server.send(200, "text/plain", "OK: Slides Updated & Saved");
}


// Handle Time Sync from User Phone / Browser
void handleSyncTime() {
  if (server.hasArg("epoch")) {
    time_t epoch = (time_t)server.arg("epoch").toInt();
    long tzOffsetSec = 0;
    if (server.hasArg("tz")) {
      tzOffsetSec = server.arg("tz").toInt() * 60; // minutes to seconds
    }
    struct timeval tv;
    tv.tv_sec = epoch + tzOffsetSec;
    tv.tv_usec = 0;
    settimeofday(&tv, NULL);
    timeSynced = true;
    Serial.printf("[TIME] Synced with user device! Epoch: %ld, TZ: %ld sec\n", (long)epoch, tzOffsetSec);
    server.send(200, "text/plain", "OK: Time Synced");
  } else {
    server.send(400, "text/plain", "Missing epoch parameter");
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

  // Load saved face/slideshow settings from NVS flash memory
  loadSettings();

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
  // The SuperMini onboard voltage regulator can brown out at peak TX power (19.5 dBm),
  // which can cause the Wi-Fi radio to reset. Limiting to 8.5 dBm ensures rock-solid stability.
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
  server.on("/setslides", HTTP_GET, handleSetSlides);
  server.on("/synctime", HTTP_GET, handleSyncTime);
  server.on("/setbday", HTTP_GET, handleSetBday);
  server.on("/getbday", HTTP_GET, []() {
    server.send(200, "text/plain", bdayName);
  });
  server.on("/sleep", HTTP_GET, []() {
    server.send(200, "text/plain", "OK: Turning off WiFi to save battery");
    delay(200);
    disableWiFi();
  });

  // Web Browser OTA Route (Upload .bin binary directly via any web browser without external software)
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
  // 2. OLED Animation Update
  // ==========================================
  // Keep face animation natural (180ms per frame)
  static unsigned long lastFaceFrame = 0;
  if (now - lastFaceFrame >= 40) {
    lastFaceFrame = now;
    animFrame++;
  }

  // Display refresh: butter-smooth 40ms (~25 FPS) for ALL animations & expressive emojis
  int animInterval = 40;
  if (now - lastAnimUpdate >= animInterval) {
    lastAnimUpdate = now;
    if (oledConnected) {
      renderCurrentFace();

      // Top-right corner indicators (shown on emoji faces, suppressed on clean clock/date):
      if (wifiEnabled && currentFace != "time" && currentFace != "date" && currentFace != "clock" && currentFace != "ram" && currentFace != "diwali" && currentFace != "birthday" && currentFace != "torch") {
        if (WiFi.softAPgetStationNum() > 0) {
          display.fillCircle(124, 4, 2, SSD1306_WHITE);
        } else if ((animFrame % 4) < 2) {
          display.fillCircle(124, 4, 2, SSD1306_WHITE);
        }
        display.display();
      }
    }
  }

  // Small delay yields CPU to FreeRTOS idle task
  delay(5);
}
