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

    <!-- 13. Happy Birthday Abhishek -->
    <div class="card" onclick="setFace('birthday', 'HBD Abhishek 🎂', this)" style="background: linear-gradient(135deg, rgba(255,215,0,0.15), rgba(236,72,153,0.2)); border-color: rgba(255,215,0,0.4);">
      <span class="emoji">🎂</span>
      <div class="title" style="color:#fbbf24;">Birthday</div>
      <div class="desc">HBD Abhishek!</div>
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

    <!-- 14. Dead Pixel Diagnostic Test -->
    <div class="card test-card" onclick="setFace('pixel_test', 'Dead Pixel Test', this)">
      <span class="emoji">🧪</span>
      <div>
        <div class="title">Pixel Diagnostics Mode</div>
        <div class="desc">All Pixels White & Black Test to spot dead/stuck pixels</div>
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
      { id: 'fireworks', name: '🎆 Patake/Fireworks' }
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

    window.addEventListener('DOMContentLoaded', function() {
      renderPlaylistUI();
      syncTimeWithDevice();
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
// CLOCK & DATE SLIDES (With Smooth Slide Transition)
// ==========================================

// Helper: Draw smooth rounded vector digits (52px tall, round corners, NO slash in 0)
void drawSmoothBigDigit(int x, int y, char c, int w, int h) {
  int r = 8; // Corner radius
  int t = 5; // Stroke thickness

  switch (c) {
    case '0':
      // Clean round oval with NO line in the middle
      display.fillRoundRect(x, y, w, h, r, SSD1306_WHITE);
      display.fillRoundRect(x + t, y + t, w - 2 * t, h - 2 * t, 4, SSD1306_BLACK);
      break;

    case '1':
      display.fillRoundRect(x + w - t - 2, y, t + 1, h, 2, SSD1306_WHITE);
      display.fillRoundRect(x + w - t - 9, y + 2, 8, t, 2, SSD1306_WHITE);
      break;

    case '2': {
      display.fillRoundRect(x, y, w, 18, r, SSD1306_WHITE);
      display.fillRoundRect(x, y + t, w - t, 13, 3, SSD1306_BLACK);
      display.fillRoundRect(x, y + h - t, w, t, 2, SSD1306_WHITE);
      for (int i = 0; i < t; i++) {
        display.drawLine(x + w - t + i, y + 14, x + i, y + h - t, SSD1306_WHITE);
      }
      break;
    }

    case '3': {
      int topH = h / 2 + 1;
      int botH = h - topH + 2;
      display.fillRoundRect(x, y, w, topH, r, SSD1306_WHITE);
      display.fillRoundRect(x + t, y + t, w - 2 * t, topH - 2 * t, 3, SSD1306_BLACK);
      display.fillRect(x, y + t, t + 1, topH - 2 * t, SSD1306_BLACK);
      display.fillRoundRect(x, y + topH - 2, w, botH, r, SSD1306_WHITE);
      display.fillRoundRect(x + t, y + topH - 2 + t, w - 2 * t, botH - 2 * t, 3, SSD1306_BLACK);
      display.fillRect(x, y + topH - 2 + t, t + 1, botH - 2 * t, SSD1306_BLACK);
      display.fillRoundRect(x + 5, y + topH - 3, w - 8, t, 2, SSD1306_WHITE);
      break;
    }

    case '4':
      display.fillRoundRect(x + w - t - 1, y, t + 1, h, 2, SSD1306_WHITE);
      display.fillRoundRect(x, y, t, 32, 2, SSD1306_WHITE);
      display.fillRoundRect(x, y + 29, w, t, 2, SSD1306_WHITE);
      break;

    case '5': {
      int botH = h / 2 + 4;
      display.fillRoundRect(x, y, w, t, 2, SSD1306_WHITE);
      display.fillRect(x, y, t, 25, SSD1306_WHITE);
      display.fillRoundRect(x, y + h - botH, w, botH, r, SSD1306_WHITE);
      display.fillRoundRect(x + t, y + h - botH + t, w - 2 * t, botH - 2 * t, 3, SSD1306_BLACK);
      display.fillRect(x, y + h - botH + t, t + 1, botH - 2 * t, SSD1306_BLACK);
      display.fillRoundRect(x, y + h - botH, w - 4, t, 2, SSD1306_WHITE);
      break;
    }

    case '6': {
      int botH = h / 2 + 3;
      display.fillRoundRect(x, y, t, h, 2, SSD1306_WHITE);
      display.fillRoundRect(x, y, w, t, 2, SSD1306_WHITE);
      display.fillRoundRect(x, y + h - botH, w, botH, 7, SSD1306_WHITE);
      display.fillRoundRect(x + t, y + h - botH + t, w - 2 * t, botH - 2 * t, 3, SSD1306_BLACK);
      break;
    }

    case '7':
      display.fillRoundRect(x, y, w, 6, 2, SSD1306_WHITE);
      for (int i = 0; i < t; i++) {
        display.drawLine(x + w - t + i, y + 5, x + 3 + i, y + h - 1, SSD1306_WHITE);
      }
      break;

    case '8': {
      int topH = h / 2 + 1;
      int botH = h - topH + 2;
      display.fillRoundRect(x, y, w, topH, 7, SSD1306_WHITE);
      display.fillRoundRect(x + t, y + t, w - 2 * t, topH - 2 * t, 3, SSD1306_BLACK);
      display.fillRoundRect(x, y + topH - 2, w, botH, 7, SSD1306_WHITE);
      display.fillRoundRect(x + t, y + topH - 2 + t, w - 2 * t, botH - 2 * t, 3, SSD1306_BLACK);
      break;
    }

    case '9': {
      int topH = h / 2 + 3;
      // Top complete solid loop
      display.fillRoundRect(x, y, w, topH, 7, SSD1306_WHITE);
      display.fillRoundRect(x + t, y + t, w - 2 * t, topH - 2 * t, 3, SSD1306_BLACK);
      // Right vertical stem from top to bottom (solid unbroken line!)
      display.fillRoundRect(x + w - t, y, t, h, 2, SSD1306_WHITE);
      // Bottom horizontal base bar (Classic digital 9, NO upward hook like 'g'!)
      display.fillRoundRect(x, y + h - t, w, t, 2, SSD1306_WHITE);
      break;
    }

    default:
      display.fillRoundRect(x, y, w, h, r, SSD1306_WHITE);
      break;
  }
}

// Slide 1: TIME (Massive 52px Hour, Equal Distance Blinking Colon, Right Margin Safe from Bezel)
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

  // Coordinates with safe clearance from 3D-printed enclosure bezel:
  // Left margin: 8px. Right margin: 12px (PM will never cut!). Top margin: 6px.
  int digitW = 22;
  int digitH = 52;
  int hourY  = 6;

  int hourX1 = xOffset + 8;
  int hourX2 = xOffset + 8 + digitW + 3; // xOffset + 33
  int hourRight = hourX2 + digitW;       // xOffset + 55

  // Equal Distance on both sides of Colon:
  // Gap from Hour to Colon = 8px. Gap from Colon to Minutes = 9px.
  int colonX = xOffset + 65;
  int rightX = xOffset + 76;
  int ampmX  = xOffset + 98; // Ends at ~116px -> 12px safe clearance from right bezel!

  // 1. Draw Massive Smooth Hour Digits (Round corners, NO slash in 0, solid unbroken 9)
  drawSmoothBigDigit(hourX1, hourY, hourBuf[0], digitW, digitH);
  drawSmoothBigDigit(hourX2, hourY, hourBuf[1], digitW, digitH);

  // 2. Colon ':' dots (Blinks with second: 500ms ON / 500ms OFF)
  bool colonVisible = (millis() % 1000 < 500);
  if (colonVisible) {
    display.fillCircle(colonX, 17, 2, SSD1306_WHITE);
    display.fillCircle(colonX, 45, 2, SSD1306_WHITE);
  }

  // 3. Minutes (FreeSansBold 12pt)
  display.setFont(&FreeSansBold12pt7b);
  display.setTextSize(1);
  display.setCursor(rightX, 26);
  display.print(minBuf);

  // 4. Seconds (FreeSansBold 9pt) & AM/PM (FreeSans 9pt)
  display.setFont(&FreeSansBold9pt7b);
  display.setCursor(rightX, 52);
  display.print(secBuf);

  display.setFont(&FreeSans9pt7b);
  display.setCursor(ampmX, 52);
  display.print(ampm);

  // Reset to default font & wrap so other animations/faces work normally
  display.setFont(NULL);
  display.setTextWrap(true);
  display.setTextSize(1);
}

// Slide 2: DATE (Line 1: "02 November", Line 2: "Sunday, 2026")
// Mathematically centered in both X and Y on 0.96 OLED (128x64)
void drawSlide2_Date(int xOffset, struct tm *t) {
  display.setTextWrap(false);
  display.setTextColor(SSD1306_WHITE);

  const char *fullMonths[] = {
    "January", "February", "March", "April", "May", "June",
    "July", "August", "September", "October", "November", "December"
  };
  const char *shortMonths[] = {
    "Jan", "Feb", "Mar", "Apr", "May", "Jun",
    "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
  };
  const char *fullDays[] = {
    "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
  };
  const char *shortDays[] = {
    "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
  };

  int mon  = (t->tm_mon >= 0 && t->tm_mon <= 11) ? t->tm_mon : 0;
  int wday = (t->tm_wday >= 0 && t->tm_wday <= 6) ? t->tm_wday : 0;
  int year = t->tm_year + 1900;

  // 1. Measure Line 1: Day of Month + Month (e.g. "02 November")
  char dateBuf[32];
  snprintf(dateBuf, sizeof(dateBuf), "%02d %s", t->tm_mday, fullMonths[mon]);

  display.setFont(&FreeSansBold9pt7b);
  display.setTextSize(1);
  int16_t x1, y1;
  uint16_t w1, h1;
  display.getTextBounds(dateBuf, 0, 0, &x1, &y1, &w1, &h1);
  if (w1 > 120) {
    snprintf(dateBuf, sizeof(dateBuf), "%02d %s.", t->tm_mday, shortMonths[mon]);
    display.getTextBounds(dateBuf, 0, 0, &x1, &y1, &w1, &h1);
  }

  // 2. Measure Line 2: Day of Week, Year (e.g. "Sunday, 2026")
  char dayYearBuf[32];
  snprintf(dayYearBuf, sizeof(dayYearBuf), "%s, %04d", fullDays[wday], year);

  display.setFont(&FreeSans9pt7b);
  int16_t x2, y2;
  uint16_t w2, h2;
  display.getTextBounds(dayYearBuf, 0, 0, &x2, &y2, &w2, &h2);
  if (w2 > 120) {
    snprintf(dayYearBuf, sizeof(dayYearBuf), "%s, %04d", shortDays[wday], year);
    display.getTextBounds(dayYearBuf, 0, 0, &x2, &y2, &w2, &h2);
  }

  // 3. Exact Mathematical Centering (X and Y)
  // X: Left margin == Right margin for each line
  int line1X = xOffset + (128 - w1) / 2 - x1;
  int line2X = xOffset + (128 - w2) / 2 - x2;

  // Y: Calculate combined height and center vertically (Top Margin == Bottom Margin)
  int gap = 8;
  int totalH = h1 + gap + h2;
  int topMargin = (64 - totalH) / 2;
  int line1Y = topMargin - y1;
  int line2Y = (topMargin + h1 + gap) - y2;

  // Draw Line 1 (Bold Month & Day)
  display.setFont(&FreeSansBold9pt7b);
  display.setCursor(line1X, line1Y);
  display.print(dateBuf);

  // Draw Line 2 (Day of Week & Year)
  display.setFont(&FreeSans9pt7b);
  display.setCursor(line2X, line2Y);
  display.print(dayYearBuf);

  // Reset to default font & wrap so other animations/faces work normally
  display.setFont(NULL);
  display.setTextWrap(true);
  display.setTextSize(1);
}

// ==========================================
// DIWALI CELEBRATION ANIMATION (5 Phases)
// ==========================================
void drawDiwaliFace(int frame) {
  display.clearDisplay();
  int phase = (frame / 25) % 5;

  if (phase == 0) {
    // Phase 1: Animated diya with flickering flame + rangoli dots
    // Diya body (oil lamp) - bowl shape at center
    int cx = 64, cy = 42;
    display.fillCircle(cx, cy, 10, SSD1306_WHITE);
    display.fillRect(cx - 10, cy - 2, 20, 2, SSD1306_BLACK); // cut top flat
    display.fillRect(cx - 12, cy, 24, 8, SSD1306_BLACK);      // cut bottom
    display.fillCircle(cx, cy + 2, 10, SSD1306_WHITE);
    display.fillRect(cx - 10, cy - 10, 20, 10, SSD1306_BLACK);
    // Redraw bowl properly
    display.fillRoundRect(cx - 12, cy - 2, 24, 10, 4, SSD1306_WHITE);
    display.fillRoundRect(cx - 10, cy, 20, 4, 2, SSD1306_BLACK);
    // Diya lip/spout
    display.fillRect(cx - 14, cy - 3, 28, 3, SSD1306_WHITE);
    // Wick
    display.drawFastVLine(cx, cy - 8, 6, SSD1306_WHITE);
    // Flickering flame
    int fl = frame % 5;
    int fx = cx + (fl == 1 ? -1 : (fl == 3 ? 1 : 0));
    int fh = (fl == 2 || fl == 4) ? 5 : 4;
    display.fillCircle(fx, cy - 12, 3, SSD1306_WHITE);
    display.fillCircle(fx, cy - 12 - fh, 2, SSD1306_WHITE);
    display.drawPixel(fx, cy - 18, SSD1306_WHITE);
    // Small base/stand
    display.fillRect(cx - 4, cy + 7, 8, 3, SSD1306_WHITE);

    // "SHUBH DEEPAWALI" text at top
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(12, 2);
    display.print("SHUBH DEEPAWALI");

    // Rangoli dot pattern around diya (circular dots)
    for (int i = 0; i < 8; i++) {
      // Simple integer trig approximation using lookup
      int dx = 0, dy = 0;
      switch (i % 8) {
        case 0: dx = 22; dy = 0; break;
        case 1: dx = 15; dy = -15; break;
        case 2: dx = 0; dy = -22; break;
        case 3: dx = -15; dy = -15; break;
        case 4: dx = -22; dy = 0; break;
        case 5: dx = -15; dy = 15; break;
        case 6: dx = 0; dy = 22; break;
        case 7: dx = 15; dy = 15; break;
      }
      int px = cx + dx;
      int py = (cy - 4) + dy;
      if (px > 1 && px < 126 && py > 1 && py < 62) {
        if ((frame + i) % 3 == 0)
          display.fillCircle(px, py, 2, SSD1306_WHITE);
        else
          display.drawCircle(px, py, 2, SSD1306_WHITE);
      }
    }

  } else if (phase == 1) {
    // Phase 2: Multiple diyas in a row (doorstep decoration)
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(34, 0);
    display.print("~ DIWALI ~");

    for (int d = 0; d < 5; d++) {
      int dx = 8 + d * 24;
      int dy = 38;
      // Diya bowl
      display.fillRoundRect(dx - 6, dy, 16, 6, 3, SSD1306_WHITE);
      display.fillRect(dx - 8, dy - 1, 20, 2, SSD1306_WHITE);
      // Base
      display.fillRect(dx - 2, dy + 6, 8, 2, SSD1306_WHITE);
      // Wick
      display.drawFastVLine(dx + 2, dy - 6, 6, SSD1306_WHITE);
      // Independent flickering flame per diya
      int fl = (frame + d * 7) % 5;
      int ffx = dx + 2 + (fl == 1 ? -1 : (fl == 3 ? 1 : 0));
      int ffy = dy - 10;
      display.fillCircle(ffx, ffy, 2, SSD1306_WHITE);
      display.fillCircle(ffx, ffy - 2 - (fl % 2), 1, SSD1306_WHITE);
      display.drawPixel(ffx, ffy - 5, SSD1306_WHITE);
    }

    // Decorative dots along bottom
    for (int i = 0; i < 128; i += 4) {
      if ((frame + i) % 6 < 3)
        display.drawPixel(i, 54 + (i % 5), SSD1306_WHITE);
    }
    // Small stars twinkling
    for (int i = 0; i < 6; i++) {
      int sx = (i * 23 + frame * 2) % 128;
      int sy = 12 + (i * 3) % 10;
      if ((frame + i) % 4 < 2) display.drawPixel(sx, sy, SSD1306_WHITE);
    }

  } else if (phase == 2) {
    // Phase 3: "HAPPY DIWALI" text with sparkle border animation
    display.setTextColor(SSD1306_WHITE);

    // Animated sparkle border
    for (int i = 0; i < 128; i += 3) {
      int offset = (frame * 2 + i) % 8;
      if (offset < 4) {
        display.drawPixel(i, 0, SSD1306_WHITE);
        display.drawPixel(i, 1, SSD1306_WHITE);
        display.drawPixel(127 - i, 62, SSD1306_WHITE);
        display.drawPixel(127 - i, 63, SSD1306_WHITE);
      }
    }
    for (int i = 0; i < 64; i += 3) {
      int offset = (frame * 2 + i) % 8;
      if (offset < 4) {
        display.drawPixel(0, i, SSD1306_WHITE);
        display.drawPixel(1, i, SSD1306_WHITE);
        display.drawPixel(126, 63 - i, SSD1306_WHITE);
        display.drawPixel(127, 63 - i, SSD1306_WHITE);
      }
    }

    // "HAPPY" in big text
    display.setTextSize(2);
    int bobY = 10 + ((frame % 6 < 3) ? 0 : 2);
    display.setCursor(20, bobY);
    display.print("HAPPY");
    // "DIWALI" below
    display.setCursor(14, bobY + 22);
    display.print("DIWALI");

    // Corner decorations (small diyas)
    for (int c = 0; c < 4; c++) {
      int cx2 = (c < 2) ? 8 : 118;
      int cy2 = (c % 2 == 0) ? 8 : 56;
      if ((frame + c * 3) % 4 < 3)
        display.fillCircle(cx2, cy2, 2, SSD1306_WHITE);
    }

  } else if (phase == 3) {
    // Phase 4: Rangoli pattern - geometric symmetric design
    int cx = 64, cy = 32;
    int rot = (frame * 2) % 16;

    // Central flower/mandala
    display.fillCircle(cx, cy, 4, SSD1306_WHITE);
    display.drawCircle(cx, cy, 8, SSD1306_WHITE);
    display.drawCircle(cx, cy, 14, SSD1306_WHITE);
    display.drawCircle(cx, cy, 22, SSD1306_WHITE);

    // Petal dots on inner ring (8 petals)
    int petalR = 11;
    for (int i = 0; i < 8; i++) {
      int pdx = 0, pdy = 0;
      int idx = (i + rot / 2) % 8;
      switch (idx) {
        case 0: pdx = petalR; pdy = 0; break;
        case 1: pdx = petalR * 7 / 10; pdy = -petalR * 7 / 10; break;
        case 2: pdx = 0; pdy = -petalR; break;
        case 3: pdx = -petalR * 7 / 10; pdy = -petalR * 7 / 10; break;
        case 4: pdx = -petalR; pdy = 0; break;
        case 5: pdx = -petalR * 7 / 10; pdy = petalR * 7 / 10; break;
        case 6: pdx = 0; pdy = petalR; break;
        case 7: pdx = petalR * 7 / 10; pdy = petalR * 7 / 10; break;
      }
      display.fillCircle(cx + pdx, cy + pdy, 2, SSD1306_WHITE);
    }

    // Outer decorative dots on outer ring
    int outerR = 19;
    for (int i = 0; i < 12; i++) {
      // Approximate positions for 12 points
      int odx = 0, ody = 0;
      switch (i % 12) {
        case 0: odx = outerR; ody = 0; break;
        case 1: odx = outerR * 87 / 100; ody = -outerR / 2; break;
        case 2: odx = outerR / 2; ody = -outerR * 87 / 100; break;
        case 3: odx = 0; ody = -outerR; break;
        case 4: odx = -outerR / 2; ody = -outerR * 87 / 100; break;
        case 5: odx = -outerR * 87 / 100; ody = -outerR / 2; break;
        case 6: odx = -outerR; ody = 0; break;
        case 7: odx = -outerR * 87 / 100; ody = outerR / 2; break;
        case 8: odx = -outerR / 2; ody = outerR * 87 / 100; break;
        case 9: odx = 0; ody = outerR; break;
        case 10: odx = outerR / 2; ody = outerR * 87 / 100; break;
        case 11: odx = outerR * 87 / 100; ody = outerR / 2; break;
      }
      if ((frame + i) % 3 == 0)
        display.fillCircle(cx + odx, cy + ody, 2, SSD1306_WHITE);
      else
        display.drawPixel(cx + odx, cy + ody, SSD1306_WHITE);
    }

    // Radial lines from center to edges (rotating)
    for (int i = 0; i < 4; i++) {
      int lx = 0, ly = 0;
      int idx2 = (i + rot / 4) % 4;
      switch (idx2) {
        case 0: lx = 28; ly = 0; break;
        case 1: lx = 0; ly = -28; break;
        case 2: lx = -28; ly = 0; break;
        case 3: lx = 0; ly = 28; break;
      }
      display.drawLine(cx, cy, cx + lx, cy + ly, SSD1306_WHITE);
    }

    // Corner dots
    display.fillCircle(10, 8, 2, SSD1306_WHITE);
    display.fillCircle(118, 8, 2, SSD1306_WHITE);
    display.fillCircle(10, 56, 2, SSD1306_WHITE);
    display.fillCircle(118, 56, 2, SSD1306_WHITE);

  } else {
    // Phase 5: Full celebration scene - diya + text + sparkles + stars
    // Small diya at bottom-left
    display.fillRoundRect(4, 50, 14, 5, 2, SSD1306_WHITE);
    display.fillRect(2, 49, 18, 2, SSD1306_WHITE);
    display.drawFastVLine(10, 43, 6, SSD1306_WHITE);
    int fl2 = frame % 4;
    display.fillCircle(10 + (fl2 == 1 ? -1 : 0), 40, 2, SSD1306_WHITE);
    display.drawPixel(10, 37, SSD1306_WHITE);

    // Small diya at bottom-right
    display.fillRoundRect(110, 50, 14, 5, 2, SSD1306_WHITE);
    display.fillRect(108, 49, 18, 2, SSD1306_WHITE);
    display.drawFastVLine(116, 43, 6, SSD1306_WHITE);
    int fl3 = (frame + 2) % 4;
    display.fillCircle(116 + (fl3 == 2 ? 1 : 0), 40, 2, SSD1306_WHITE);
    display.drawPixel(116, 37, SSD1306_WHITE);

    // "HAPPY DIWALI" text centered
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(16, 4);
    display.print("~ HAPPY DIWALI ~");
    display.setTextSize(1);
    display.setCursor(12, 18);
    display.print("SHUBH DEEPAWALI");

    // Stars raining down
    for (int i = 0; i < 15; i++) {
      int sx = (i * 9 + frame * 3) % 128;
      int sy = (i * 7 + frame * 2) % 40 + 24;
      display.drawPixel(sx, sy, SSD1306_WHITE);
      display.drawPixel(sx + 1, sy, SSD1306_WHITE);
      display.drawPixel(sx, sy + 1, SSD1306_WHITE);
    }

    // Sparkle dots everywhere
    for (int i = 0; i < 10; i++) {
      int px = (i * 13 + frame * 5) % 128;
      int py = (i * 11 + frame * 3) % 30 + 28;
      if ((frame + i) % 3 < 2)
        display.drawPixel(px, py, SSD1306_WHITE);
    }
  }
}

// ==========================================
// LORD RAM ANIMATION: Cinematic Slow-Motion Archery Scene
// Ram Ji stands on LEFT, animates drawing Kodanda Bow, & shoots Arrow in SLOW MOTION across the sky!
// ==========================================
void drawRamFace(int frame) {
  display.clearDisplay();

  // Smooth 25 FPS time-base using millis (6.4 second complete loop)
  unsigned long t = millis() % 6400; // 0 to 6399 ms

  // Ram Ji Position on LEFT
  const int bx = 14; // Base X
  const int by = 5;  // Base Y

  // Determine Animation Stage
  // Stage 1 (0 to 1200ms): Stance & Nock Arrow
  // Stage 2 (1200 to 2400ms): Draw Bowstring & Flex Bow
  // Stage 3 (2400 to 3200ms): Divine Astra Charge (Full Draw)
  // Stage 4 (3200 to 5600ms): Release & Slow-Motion Arrow Flight Across Sky (2.4s)
  // Stage 5 (5600 to 6400ms): Target Impact Flash & Victory
  int stage = 1;
  if (t < 1200) stage = 1;
  else if (t < 2400) stage = 2;
  else if (t < 3200) stage = 3;
  else if (t < 5600) stage = 4;
  else stage = 5;

  // Compute Bowstring Pull Position (nockX)
  int nockX = 26; // Resting string position at x = 26
  if (stage == 1) {
    nockX = 26;
  } else if (stage == 2) {
    float p = (t - 1200) / 1200.0f; // 0.0 to 1.0
    float ease = p * p * (3.0f - 2.0f * p); // Smooth ease in-out
    nockX = (int)(26.0f - 12.0f * ease);    // Pulled back from 26 to 14
  } else if (stage == 3) {
    nockX = 14; // Full draw hold
  } else {
    nockX = 26; // Released forward
  }

  // ----------------------------------------------------
  // 1. ENVIRONMENT: Ground, Floating Clouds & Twinkling Stars
  // ----------------------------------------------------
  // Ground line
  display.drawFastHLine(0, 54, 128, SSD1306_WHITE);
  // Grass tufts on ground
  display.drawPixel(4, 53, SSD1306_WHITE);
  display.drawPixel(28, 53, SSD1306_WHITE);
  display.drawPixel(62, 53, SSD1306_WHITE);
  display.drawPixel(95, 53, SSD1306_WHITE);
  display.drawPixel(122, 53, SSD1306_WHITE);

  // Soft drifting clouds in the upper sky (adds cinematic slow-mo depth)
  int cloud1X = (int)((t / 80) % 160) - 20;
  if (cloud1X >= -20 && cloud1X < 120) {
    display.drawFastHLine(cloud1X, 8, 16, SSD1306_WHITE);
    display.drawFastHLine(cloud1X + 4, 7, 8, SSD1306_WHITE);
  }
  int cloud2X = (int)(((t + 3200) / 100) % 170) - 20;
  if (cloud2X >= -20 && cloud2X < 120) {
    display.drawFastHLine(cloud2X, 14, 14, SSD1306_WHITE);
    display.drawFastHLine(cloud2X + 3, 13, 8, SSD1306_WHITE);
  }

  // Subtle celestial stars
  display.drawPixel(50, 4, SSD1306_WHITE);
  display.drawPixel(75, 11, SSD1306_WHITE);
  display.drawPixel(105, 5, SSD1306_WHITE);

  // ----------------------------------------------------
  // 2. BHAGWAN RAM SILHOUETTE (Standing proudly on LEFT)
  // ----------------------------------------------------
  // Divine Halo (Prabhamandal) around head
  int haloR = (stage == 3) ? 10 : 9; // Expands when divine power charges
  display.drawCircle(bx + 4, by + 12, haloR, SSD1306_WHITE);
  if (stage == 3 && (frame % 2 == 0)) {
    // Radiating aura rays in Stage 3
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
    // Released follow-through pose (hand opened after release)
    display.drawLine(bx + 2, by + 22, bx + 10, by + 28, SSD1306_WHITE);
    display.drawPixel(bx + 11, by + 29, SSD1306_WHITE);
  }

  // ----------------------------------------------------
  // 3. KODANDA DHANUSH (Authentic Recurve Bow on Left)
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
    // String is PULLED BACK into dynamic '>' angle by right hand!
    display.drawLine(tipX, bowTopY, nockX, 32, SSD1306_WHITE);
    display.drawLine(tipX, bowBotY, nockX, 32, SSD1306_WHITE);
  } else {
    // String is straight taut at resting position
    // Subtle recoil vibration in the first 160ms of release
    int recoil = (stage == 4 && (t - 3200 < 160)) ? (((frame % 2 == 0) ? 1 : -1)) : 0;
    display.drawLine(tipX + recoil, bowTopY, tipX + recoil, bowBotY, SSD1306_WHITE);
  }

  // ----------------------------------------------------
  // 5. THE ARROW (DIVYA BAAN) & SLOW-MOTION FLIGHT
  // ----------------------------------------------------
  if (stage == 1) {
    // Arrow placed on resting bow
    display.drawFastHLine(nockX, 32, 22, SSD1306_WHITE);
    display.fillTriangle(nockX + 26, 32, nockX + 21, 29, nockX + 21, 35, SSD1306_WHITE);

    // Text
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(54, 4);
    display.print("JAI SHRI RAM");

  } else if (stage == 2) {
    // Arrow being drawn back with string
    display.drawFastHLine(nockX, 32, 24, SSD1306_WHITE);
    display.drawFastHLine(nockX, 33, 24, SSD1306_WHITE);
    // Arrowhead
    display.fillTriangle(nockX + 28, 32, nockX + 23, 29, nockX + 23, 35, SSD1306_WHITE);
    // Tail fletchings at nock
    display.drawLine(nockX, 32, nockX - 3, 29, SSD1306_WHITE);
    display.drawLine(nockX, 33, nockX - 3, 36, SSD1306_WHITE);

    // Text
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(48, 4);
    display.print("PRATYANCHA...");

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

    // Text
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(44, 4);
    display.print("~ DIVYA BAAN ~");

  } else if (stage == 4) {
    // ========================================================
    // STAGE 4: SLOW MOTION FLIGHT ACROSS THE SKY (2.4 SECONDS)
    // ========================================================
    unsigned long flightTime = t - 3200; // 0 to 2399 ms
    float progress = (float)flightTime / 2400.0f; // 0.0 to 1.0

    // Smooth horizontal position across open sky (x: 36 -> 122)
    int arrowHeadX = 36 + (int)(progress * 86.0f);

    // Aerodynamic trajectory: gentle subtle arc in flight
    int arrowY = 32 - (int)(6.0f * sin(progress * 3.14159f));

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

    // Top Slow-Motion indicator & text
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(46, 4);
    display.print("JAI SHRI RAM");

  } else {
    // ========================================================
    // STAGE 5: TARGET IMPACT FLASH & CELESTIAL VICTORY (800ms)
    // ========================================================
    unsigned long impTime = t - 5600; // 0 to 799 ms
    int impactX = 120, impactY = 32;

    // Expanding concentric shockwave rings
    int r1 = (int)((impTime / 800.0f) * 22.0f);
    display.drawCircle(impactX, impactY, r1, SSD1306_WHITE);
    if (r1 > 4) display.drawCircle(impactX, impactY, r1 - 4, SSD1306_WHITE);

    // Radiant Starburst explosion rays
    for (int a = 0; a < 8; a++) {
      float ang = a * 0.785f;
      int rx1 = impactX + (int)(5 * cos(ang));
      int ry1 = impactY + (int)(5 * sin(ang));
      int rx2 = impactX + (int)((r1 + 6) * cos(ang));
      int ry2 = impactY + (int)((r1 + 6) * sin(ang));
      if (rx2 >= 0 && rx2 < 128 && ry2 >= 0 && ry2 < 64) {
        display.drawLine(rx1, ry1, rx2, ry2, SSD1306_WHITE);
      }
    }

    // Victory Banner
    display.setTextColor(SSD1306_WHITE);
    display.setTextSize(1);
    display.setCursor(40, 18);
    display.print("~ VICTORY ~");
    display.setTextSize(1);
    display.setCursor(34, 34);
    display.print("JAI SHRI RAM");

    // Celestial sparkles raining
    for (int i = 0; i < 8; i++) {
      int sx = 30 + (i * 12 + frame * 4) % 80;
      int sy = 6 + (i * 7 + frame * 3) % 46;
      display.drawPixel(sx, sy, SSD1306_WHITE);
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
  } else if (face == "pixel_test") {
    drawPixelTestMode(animFrame);
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
    slideshowActive = false;
    animFrame = 0;
    saveSettings(); // <<< Persistent save to NVS flash!
    Serial.printf("[WEB] Emotion set to: %s (Single Mode) — Saved to NVS\n", currentFace.c_str());
    server.send(200, "text/plain", "OK: " + currentFace);
  } else {
    server.send(400, "text/plain", "Missing face parameter");
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
  server.on("/setslides", HTTP_GET, handleSetSlides);
  server.on("/synctime", HTTP_GET, handleSyncTime);
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
  // 2. OLED Animation Update
  // ==========================================
  // Keep face animation natural (180ms per frame)
  static unsigned long lastFaceFrame = 0;
  if (now - lastFaceFrame >= 180) {
    lastFaceFrame = now;
    animFrame++;
  }

  // Display refresh: 40ms (~25 FPS) for butter-smooth sliding transitions & second ticks,
  // 180ms when stationary face is shown
  int animInterval = (slideshowActive || currentFace == "time" || currentFace == "clock" || currentFace == "ram" || currentFace == "fireworks") ? 40 : 180;
  if (now - lastAnimUpdate >= animInterval) {
    lastAnimUpdate = now;
    if (oledConnected) {
      renderCurrentFace();

      // Top-right corner indicators (shown on emoji faces, suppressed on clean clock/date):
      if (wifiEnabled && currentFace != "time" && currentFace != "date" && currentFace != "clock" && currentFace != "ram") {
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
