# arduino-air-quality-monitor

Proyek: IoT Air Quality Monitor menggunakan ESP8266 (NodeMCU/Wemos), MQ-135, DHT22, dan OLED SSD1306.
Menampilkan data di OLED dan menyediakan **Web UI** + **/data JSON** via WiFi Access Point (ESP8266).

## Fitur
- MQ-135 (gas & polusi) → pembacaan analog
- DHT22 → suhu & kelembaban
- OLED 128x64 (I2C) menampilkan metrik
- ESP8266 membuat WiFi AP, halaman web (http://192.168.4.1/) untuk melihat data
- Endpoint JSON: `/data`

## Hardware
- NodeMCU (ESP8266) atau Wemos D1 mini
- MQ-135 sensor module (dilengkapi load resistor & potentiometer sensitivitas)
- DHT22 sensor
- OLED SSD1306 I2C 128x64
- Kabel jumper, breadboard, 5V power supply (USB)
- (Optional) Resistor, header, enclosure

## Wiring (NodeMCU)
- MQ-135 VCC -> 5V (or 3.3V jika modul mendukung) ; GND -> GND ; OUT -> A0
- DHT22 VCC -> 3.3V ; GND -> GND ; DATA -> D2 (GPIO4)
- OLED VCC -> 3.3V ; GND -> GND ; SDA -> D2 ; SCL -> D1
- NodeMCU A0 is the only analog input (no extra wiring required)

> Catatan: MQ-135 bekerja lebih stabil dengan 5V tapi pengukuran analog pada NodeMCU A0 harus sesuai (papan NodeMCU memiliki pembagi untuk A0). Periksa modul MQ-135 dan board Anda.

## Kalibrasi MQ-135
1. Biarkan MQ-135 menyala dan hangat selama minimal 24 jam untuk stabilisasi.
2. Di lingkungan bersih, catat nilai `mq_raw` lewat Serial Monitor — ini baseline.
3. Sesuaikan fungsi `computeGasIndex()` untuk menghitung Ro dan menggunakan kurva gas jika ingin hasil kuantitatif (ppm). Panduan kurva ada di datasheet MQ-135.

## Upload
1. Install libraries: Adafruit SSD1306, Adafruit GFX, DHT sensor library
2. Pilih board: NodeMCU 1.0 (ESP-12E)
3. Upload `arduino-air-quality-monitor.ino`
4. Setelah boot, sambungkan smartphone ke SSID `AQMonitor_xxxx` (atau sesuai serial output) lalu buka `http://192.168.4.1`

## Pengembangan lanjutan
- Kirim data ke smartphone via MQTT / ThingSpeak
- Implementasi HTTPS / captive portal untuk konfigurasi WiFi (WIFI manager)
- Tambahkan microSD logging, baterai, power management
