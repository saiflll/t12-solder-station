# T12 Solder Station - ESP32 Edition

T12 Soldering Station firmware for ESP32. Includes a Go Gin backend and a web dashboard for remote temp + power monitoring (optimize the logging yourself :3).

Built this to make my station a bit "smarter" without the bloated over-engineering, suited for the budget-conscious folks.

---

### 🛠 Main Features
- **RTOS Powered**: Heater control, inputs, and telemetry running on their own dedicated tasks.
- **Power Analytics**: Real-time wattage monitoring via a simple voltage divider.
- **Remote Dashboard**: Minimalist UI to keep an eye on tip temp, power, and system status.
- **Smart Logic**: Ambient temp sensing via DS18B20 to dynamically adjust auto-boost duration.

---

### 🛠 Hardware & Components
Standard T12 circuit using thermocouple feedback through an op-amp.

| Item | Spec | Purpose |
| :--- | :--- | :--- |
| MCU | ESP32 DevKit V1 | Main Brain & WiFi |
| Display | OLED SSD1306 | Local UI |
| Op-Amp | LM358 / SGM | **Tip Temperature Feedback (Handle)** |
| Sensor | DS18B20 | **Ambient Temp (Boost duration logic)** |
| Resistor R1 | 10k Ohm | Divider (High Side) |
| Resistor R2 | 1.5k Ohm | Divider (Low Side) - Max 25V |
| MOSFET | IRLZ44N | Heater PWM Driver |
| Input | Rotary EC11 | Controller |

---

### 📊 Power & Temp Logic
- **Tip Temperature**: Read via LM358 amplifier hooked up to GPIO 33.
- **Ambient & Boost**: DS18B20 reads room temp. If it's a cold start, the system automatically stretches the Boost duration for faster heat-up.
- **V-Input**: Monitored via GPIO 32 for wattage calculation:
  `V_in = (ADC / 4095) * 3.3 * (11500 / 1500)`

---

###  > Deployment

1. **Config**: Edit `include/config.h`, put in your WiFi SSID/Pass & Server IP. Adjust according to your "core beliefs".
2. **Flash**: Upload via PlatformIO. You could use Arduino IDE too, but you'll have to mess with it a bit :v
3. **Server**: Run Docker in the project folder:
   ```bash
   docker-compose up -d
   ```
4. **Monitor**: Open `http://localhost:3000` in your browser.

---

### ⚠️ Disclaimer
Use with common sense. High currents and high heat involved. A single slip-up in wiring might send your ESP32 or MOSFET to hardware heaven.

--- regards, reversed current :3

**saiflll**
