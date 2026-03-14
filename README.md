# ESP32 Proximity Information Display (Prox)

A smart, connected desktop display built on the ESP-IDF framework for the ESP32 (specifically developed for the ESP32-S3-DevKit-C1). The project uses a 128x128 SSD1327 OLED display to show real-time, location-aware data fetched over WiFi.

Currently, the display features three interactive pages toggled via a hardware button:
1. **Home Page (Default)**: Shows big, clear local time (12-hour format with seconds) and date, automatically offset based on your selected `menuconfig` timezone (with DST support).
   <br>![Home Page](img/PAGE_HOME.png)
2. **Weather Page**: Shows live weather (temperature, humidity, feels-like, pressure, wind, sunrise/sunset, conditions) via the OpenWeatherMap API.
   <br>![Weather Page](img/PAGE_OPENWEATHER.png)
3. **Adhan Page**: Shows daily Islamic prayer timings (Imsak, Fajr, Dhuhr, Asr, Maghrib, Isha) formatted in 12-hour AM/PM, fetched seamlessly from the AlAdhan API based on device coordinates.
   <br>![Adhan Page](img/PAGE_PRAYERTIMES.png)

![image](https://img.shields.io/badge/ESP--IDF-v5.x-blue.svg)
![image](https://img.shields.io/badge/Hardware-ESP32--S3-orange.svg)
![image](https://img.shields.io/badge/Display-SSD1327-green.svg)

---

## ⚡ Features

- **FreeRTOS Multi-Tasking Architecture:** Dedicated tasks for API fetching (HTTPS) and display rendering ensure a highly responsive UI while heavy network requests happen in the background.
- **`Kconfig` Integration:** All hard-coded values (WiFi credentials, API keys, GPS coordinates, GPIO pin mapping) have been moved to `menuconfig` for easy zero-code configuration.
- **Hardware Debouncing:** Snappy, reliable page-tossing using the ESP32's BOOT button.
- **Custom Components:** Includes modular components for WiFi management, OpenWeatherMap, AlAdhan, and a custom `espu8g2` HAL wrapper for the [U8g2 graphics library](https://github.com/olikraus/u8g2).

---

## 🛠 Hardware Requirements

- **Microcontroller**: ESP32 / ESP32-S3 (Tested on ESP32-S3-DevKit-C1).
- **Display**: 1.5 inch 128x128 OLED Display Module (SSD1327) using 4-Wire SPI.

### Default Pinout (ESP32-S3)

Pins can be re-assigned in `menuconfig`.

| ESP32 Pin | SSD1327 Pin | Description |
|-----------|-------------|-------------|
| `GPIO 11` | `MOSI` / `D1` | SPI Data |
| `GPIO 12` | `CLK` / `D0` | SPI Clock |
| `GPIO 10` | `CS` | Chip Select |
| `GPIO 4` | `DC` | Data / Command |
| `GPIO 5` | `RES` | Reset |
| `GPIO 0` | - | Boot button (Page Toggle) |

---

## 🚀 Setup & Installation

### 1. Prerequisites
Ensure you have the [ESP-IDF framework](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/get-started/index.html) installed (v5.0 or later recommended). 

### 2. Clone the Repository
```bash
git clone https://github.com/minthura/prox.git
cd prox
```

### 3. Configure the Project
Run the ESP-IDF configuration menu to set your WiFi, GPS coordinates, and API keys.

```bash
idf.py menuconfig
```

Navigate to the **Prox Configuration** menu:

1. **WiFi SSID** & **WiFi Password**: Enter your local WiFi network details.
2. **Timezone**: Select your local timezone from the list (e.g. SGT, WIB, EST) to automatically offset the NTP-synced clock.
3. **OpenWeatherMap API Key**: Enter your free [OpenWeatherMap API Key](https://openweathermap.org/api).
4. **Latitude & Longitude**: Enter your exact decimal GPS coordinates (e.g. `1.3264482`, `103.9283213`). 
5. **Hardware Pins**: Adjust SPI and Button pins if your wiring differs from the default table above.

Save (press `S`) and exit (press `Q`).

### 4. Build and Flash
Compile the code and flash it to your connected ESP32 board. Replace `COM3` (Windows) or `/dev/ttyUSB0` (Linux/Mac) with your device's actual serial port.

```bash
idf.py -p COM3 build flash monitor
```

---

## 🏗 Software Architecture

- **`app_main()`**: Initialises the SSD1327 hardware, connects to WiFi, syncs NTP time, and spins up the FreeRTOS tasks.
- **`weather_task`**: Wakes every 30 minutes to fetch updated weather data. Yields to FreeRTOS while waiting.
- **`adhan_task`**: Wakes every 6 hours to fetch fresh prayer timings for the current day.
- **`display_task`**: High-priority task running every 100ms. Polls the BOOT button for presses (debounced) and commands the `u8g2` engine to draw the currently active page.
- **Mutexing**: A global `SemaphoreHandle_t` protects the shared state variables (`s_weather`, `s_timings`, `s_page`) so incomplete data is never rendered to the screen by the display task.

## 📦 Dependencies

- **[U8g2](https://github.com/olikraus/u8g2)**: Monochrome display graphics library. Included as an ESP-IDF component.
- **cJSON**: Used internally by the OpenWeatherMap and Adhan components to parse the API response bodies. (Built-in to ESP-IDF). 
- **esp_http_client**: Standard ESP-IDF HTTP/HTTPS client.

---

## 📄 License
This project is open-source under the MIT License.
