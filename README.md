# 🤖 DESK BUDDY — Animated Robot Companion, Smart Clock & Festive OLED Display

<div align="center">

![Platform](https://img.shields.io/badge/Platform-ESP32--C3%20SuperMini-brightgreen?style=for-the-badge&logo=espressif)
![Framework](https://img.shields.io/badge/Framework-Arduino-blue?style=for-the-badge&logo=arduino)
![Display](https://img.shields.io/badge/OLED-SSD1306%20128x64-orange?style=for-the-badge)
![License](https://img.shields.io/badge/License-MIT-purple?style=for-the-badge)
![Animation](https://img.shields.io/badge/Animation-25%20FPS%20Fluid-red?style=for-the-badge)

**An ultra-expressive, open-source animated desk companion, smart circular clock, festive celebration engine, and portable OLED dashboard powered by the tiny ESP32-C3 SuperMini.**

[Features](#-key-features) • [Hardware & Wiring](#-hardware--wiring) • [Getting Started](#-getting-started) • [Web Controller](#-web-controller-ui) • [OTA Updates](#-wireless-ota-updates) • [License](#-license)

</div>

---

## 🌟 Overview

**DESK BUDDY** transforms an ESP32-C3 SuperMini and a 0.96" SSD1306 OLED display into a living, emotive desktop robot pet and multifunctional smart display. 

Equipped with a built-in Wi-Fi SoftAP and responsive dark-mode web controller, you can switch emotions on the fly, customize birthday greetings, schedule automated slide transitions, update firmware over the air without plugging in a cable, and enjoy fluid 25 FPS vector animations.

---

## ✨ Key Features

### 🎭 1. 12 High-Expression Animated Robot / Pet Faces (25 FPS)
Every emotion features organic micro-animations, lifelike eye physics, dynamic gaze tracking, eyelid movements, and contextual emotional props:
* **Idle:** Soulful eyes that breathe, look left/forward/right, perform natural double-blinks, and feature blushing cheeks with a gentle smile.
* **Happy:** Cheerful body bounce, joy-arch squinting eyes, pulsing rosy cheek pads, bouncing open laugh with tongue, and star sparkles.
* **Love:** Pulsing heartbeat rhythm, floating mini-hearts drifting into the sky, shy blush markings, and a sweet smile.
* **Wink:** Graceful winking left eye with eyelashes, right eye with an animated rotating 4-point twinkle star in the pupil, cheeky smirk, and playful tongue.
* **Cool:** Aviator sunglasses with an animated white light-beam reflection sweep, raised eyebrow, smug smirk, and smoking toothpick.
* **Sleepy:** Drooping eyelids, drifting "Zzz" letters, and an animated cartoon snot bubble that expands and pops.
* **Angry:** Fierce V-furrowed brows, piercing focused eyes, clenched-teeth grimace, puffing steam clouds, screen shake, and an anime rage vein (`💢`).
* **Sad:** Slanted sorrowful brows, glassy tear-filled eyes with water wobble, rolling teardrops streaming down the cheek with splash, and a quivering pout.
* **Surprised:** Shock jolt, eyebrows shot high, dilated popped eyes with trembling pupils, dropped jaw 'O' mouth, and dripping sweat drop.
* **Laughing:** Deep squint laughing eyes (`> <`), projectile tears of joy spurting outwards, vibrating chin, and hearty belly-laugh bounce.
* **Confused:** Asymmetrical raised eyebrow, quizzical side gaze, squiggly mouth, and an animated floating question mark (`?`).
* **Dizzy:** Hypnotic 360° rotating spiral eyes, 3 cartoon stars orbiting around the head in a 3D ellipse, and a loopy dazed tongue.

---

### 🪔 2. Festive Celebrations & Animations
* **Diwali Celebration:**
  - Stylish festive typography.
  - Realistic earthen clay Diya with slow-motion flickering flame and glowing halo.
  - Step-by-step progressive Rangoli mandala drawing animation.
* **Lord Ram Cinematic Archery:**
  - Lord Ram silhouette on left drawing the Kodanda bow in slow-motion.
  - Realistic arrow release gliding gracefully across the night sky.
  - Grand multi-burst firework finale.
* **Grand Fireworks & Sky Shots:**
  - Multi-stage rocketry, crackling sparklers, ground flowerpots (anar), and cascading willow starbursts.

---

### 🎂 3. Birthday Celebration Mode
* **NVS Persistent Custom Name:** Set any name (up to 10 characters) directly from the Web UI.
* **4-Screen Automated Storyline:**
  1. `HAPPY` intro screen.
  2. `BIRTHDAY` celebration screen.
  3. Custom recipient name in bold circular typography with a pulsing heart.
  4. 3-tier birthday cake with lit flickering candle.

---

### 🕒 4. Modern Circular Smart Clock & Date
* **Circular Stadium Typography:** Hours and Minutes rendered in smooth rounded vector geometry.
* **Blinking Colon:** Circular colon dots with 500ms real-time blink.
* **AM/PM Badge & Live Seconds:** Sleek rounded pill badge with live ticking circular seconds.
* **Top Day & Date Header:** Balanced widescreen layout with zero bezel cutoffs.
* **Date Slide:** Centered pill day-of-week badge, large circular day number, clean month name, circular year digits, and bottom pearl separator.
* **Hardware-Level Slide Transitions:** Smooth horizontal sliding effect between Time and Date.

---

### 🔦 5. Torch / Flashlight Mode
* 100% full-screen pure white illumination at maximum OLED brightness (ideal as a nightlight or OLED pixel tester).

---

### 🌐 6. Web Controller & Battery Optimization
* **Dark Neumorphic Web App:** Built-in web dashboard accessible from any phone or PC.
* **Captive Portal:** Auto-opens the control page on smartphones upon connecting to Wi-Fi.
* **Smart Battery Saver:** Wi-Fi automatically sleeps if no client is connected to preserve battery.
* **Dual OTA Support:** Update firmware via web browser drag-and-drop or directly via PlatformIO over Wi-Fi (`192.168.4.1`).

---

## 🛠 Hardware & Wiring

### Bill of Materials (BOM)
| Component | Specification | Description |
| :--- | :--- | :--- |
| **Microcontroller** | ESP32-C3 SuperMini | 160MHz RISC-V, 4MB Flash, Wi-Fi 4 + BLE 5 |
| **Display** | 0.96" I2C OLED (SSD1306) | 128x64 Monochrome (White or Blue) |
| **Power Supply** | 3.7V LiPo or USB-C (5V) | Portable battery or desktop cable |
| **Jumper Wires** | 4-Pin Ribbon / Solid Core | Short wiring for compact enclosure |

### Pinout Connection Table
```
  ESP32-C3 SuperMini                0.96" SSD1306 OLED (I2C)
┌────────────────────┐             ┌────────────────────────┐
│               3.3V │────────────>│ VCC                    │
│                GND │────────────>│ GND                    │
│         GPIO 6 (D4)│────────────>│ SDA                    │
│         GPIO 7 (D5)│────────────>│ SCL                    │
└────────────────────┘             └────────────────────────┘
```
> [!NOTE]
> The ESP32-C3 SuperMini I2C bus runs at **400 kHz Fast Mode** for crisp 25 FPS animation rendering.

---

## 🚀 Getting Started

### Prerequisites
- [VS Code](https://code.visualstudio.com/) with the [PlatformIO IDE Extension](https://platformio.org/install/ide?install=vscode) installed.
- USB-C cable for initial firmware flashing.

### Installation & Flashing

1. **Clone the repository:**
   ```bash
   git clone https://github.com/jaykushwaha108/DESK-BUDDY-.git
   cd DESK-BUDDY-
   ```

2. **Open in VS Code:**
   Open the cloned folder in VS Code. PlatformIO will automatically initialize the environment and download dependencies (`Adafruit SSD1306`, `Adafruit GFX Library`).

3. **Build & Flash (via USB Cable):**
   - Connect your ESP32-C3 SuperMini via USB-C.
   - Click the PlatformIO **Upload** button (checkmark icon in the status bar) or run:
     ```bash
     pio run --target upload
     ```

4. **Verify Boot:**
   The OLED display will immediately illuminate with the **Idle** animated face!

---

## 📱 Web Controller UI

1. On your phone or laptop, search for Wi-Fi networks and connect to:
   - **SSID:** `my bot`
   - **Password:** `88888888`
2. Open your browser and navigate to:
   ```
   http://192.168.4.1
   ```
   *(Captive portal will automatically prompt you to sign in on Android / iOS).*
3. **Controls available on the web page:**
   - Tap any emotion card to switch faces instantly.
   - Enter a custom name in the **Birthday** card and tap **Save**.
   - Enable **Slideshow Mode** and configure custom transition timers.
   - Switch to **Torch Mode**, **Smart Clock**, or **Festive Animations**.

---

## 📡 Wireless OTA Updates

Never open your 3D-printed enclosure or connect a USB cable again!

### Method 1: Via Web Browser (Any Device)
1. In PlatformIO, build the firmware binary:
   ```bash
   pio run
   ```
   The compiled `.bin` file will be generated at `.pio/build/esp32-c3-devkitm-1/firmware.bin`.
2. Open `http://192.168.4.1` on your connected browser.
3. Scroll to the **OTA Firmware Update** section at the bottom, select the `.bin` file, and click **Update Firmware**.

### Method 2: Via PlatformIO Wireless Push
Connect your computer to the `my bot` Wi-Fi network, switch to the `[env:esp32-c3-ota]` environment in `platformio.ini`, and run:
```bash
pio run -e esp32-c3-ota -t upload
```

---

## 📂 Project Structure

```
.
├── .gitignore               # Ignored files (.pio, build artifacts, etc.)
├── LICENSE                  # MIT open-source license
├── README.md                # Documentation & setup guide
├── min_spiffs.csv           # Dual 1.9MB OTA partition table
├── platformio.ini           # PlatformIO build configuration
└── src
    └── main.cpp             # Complete firmware & vector animation engine
```

---

## 📜 License

This project is open-source under the [MIT License](LICENSE). Feel free to use, modify, and build upon it!

---

<div align="center">
Crafted with ❤️ for the maker & embedded community.
</div>
