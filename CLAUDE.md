# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a PlatformIO-based baitboat control system using two ESP32 microcontrollers communicating via ESP-NOW. The system consists of:

- **Controller** (`controller_driver.cpp`): A handheld controller with a TFT touchscreen display, Nintendo Nunchuk input, and LVGL-based UI
- **Boat** (`boat_driver.cpp`): The remote boat receiver that controls DC motors (via L298N/DRI0041) and a servo (SG90)

Communication between the two devices uses ESP-NOW protocol with a packed struct containing directional values (up/down/left/right) and a trigger button state.

## Build and Upload Commands

This project uses PlatformIO with two environments defined in `platformio.ini`:

### Controller Environment
```bash
# Build controller firmware
pio run -e controller

# Upload to controller (COM4)
pio run -e controller -t upload

# Monitor serial output
pio device monitor -p COM4 -b 115200
```

### Boat Environment
```bash
# Build boat firmware
pio run -e boat

# Upload to boat (COM5)
pio run -e boat -t upload

# Monitor serial output
pio device monitor -p COM5 -b 115200
```

### Testing
```bash
# Run all tests
pio test

# Run specific test
pio test -f test_update_speed_values
```

### Build All Environments
```bash
# Build both environments
pio run
```

## Architecture

### Communication Protocol

The two ESP32s communicate using ESP-NOW with the following data structure:

```cpp
typedef struct __attribute__((packed)) {
    uint8_t up;      // 0-255
    uint8_t down;    // 0-255
    uint8_t left;    // 0-255
    uint8_t right;   // 0-255
    bool trigger;    // servo control
} struct_message;
```

**Controller MAC address receiver:** `{0xA8, 0x48, 0xFA, 0x6B, 0xB4, 0xAC}` (configured in `controller_driver.cpp`)

### Controller Architecture

The controller runs on ESP32 with:
- **Display**: TFT_eSPI-based 320x240 touchscreen
- **Input**: Nintendo Nunchuk connected via I2C (SDA: pin 22, SCL: pin 27)
- **UI Framework**: LVGL v9.x with custom UI files in `lib/ui/`
- **Touch Controller**: XPT2046 on VSPI

**Key components:**
- Three LVGL screens: Loading, Connect (when Nunchuk disconnected), and Menu (main control)
- Joystick values are mapped with deadzone handling (center: 128, deadzone: ±1)
- Speed bars display directional values in real-time
- Connection monitoring via LVGL timer (500ms interval)
- ESP-NOW transmission every 100ms

**Pin Configuration:**
- Touch: IRQ=36, MOSI=32, MISO=39, CLK=25, CS=33
- Nunchuk I2C: SDA=22, SCL=27

### Boat Architecture

The boat receiver controls:
- **DC Motors**: Two motors via DRI0041/L298N driver
  - Left motor: ENA=26, IN1=27, IN2=14
  - Right motor: ENB=12, IN3=13, IN4=15
- **Servo**: SG90 on pin 18 (controlled by trigger button)

**Motor Control Features:**
- Soft start/stop with gradual speed changes (SMOOTHING_STEP = 10)
- Minimum PWM threshold (MIN_PWM = 50) to overcome motor startup resistance
- PWM frequency: 5kHz, 8-bit resolution
- Differential steering: `baseSpeed ± turnAdjust`
- Speed update interval: 10ms

**Control Logic:**
```
baseSpeed = up - down              // Forward/backward
turnAdjust = (right - left) / 2    // Steering adjustment
leftMotor = baseSpeed + turnAdjust
rightMotor = baseSpeed - turnAdjust
```

### UI System (LVGL)

The UI is generated externally (likely SquareLine Studio) and located in `lib/ui/`. Key files:
- `ui.h/ui.c`: Main UI initialization
- `ui_Loading.h/c`, `ui_Connect.h/c`, `ui_Menu.h/c`: Screen definitions
- `ui_helpers.h/c`: LVGL helper functions and animations
- `ui_events.h`: Event handlers

**UI Objects referenced in code:**
- `ui_LoadingBar`: Progress bar during startup
- `ui_SpeedBarUp/Down/Left/Right`: Directional speed indicators
- `ui_BatteryText`: Battery/connection status text
- `ui_Unplugged`: Nunchuk disconnection animation

## Important Notes

### Build System
- The `build_src_filter` in `platformio.ini` determines which source file is compiled for each environment
- Controller builds only `controller_driver.cpp`
- Boat builds only `boat_driver.cpp`
- Both environments share libraries in `lib/`

### Serial Debugging
- Both devices output extensive serial debug information at 115200 baud
- Controller logs joystick values, connection status, and ESP-NOW send results
- Boat logs received data, motor speeds, and servo state

### ESP-NOW Configuration
- Uses WiFi in STA mode (no AP, no internet connectivity)
- Channel 0, no encryption
- One-way communication: Controller → Boat
- If MAC address needs to change, update `receiverAddress` in `controller_driver.cpp`

### Motor Calibration
- Adjust `MIN_PWM` in `boat_driver.cpp` if motors don't start reliably
- Modify `SMOOTHING_STEP` to control acceleration/deceleration rate
- Change `UPDATE_INTERVAL` for more/less responsive control

### Touchscreen Calibration
- Auto-calibration happens at runtime in `my_touch_read()`
- Initial values: `touch_min_x=400, touch_max_x=3600, touch_min_y=300, touch_max_y=3700`
- Calibration values update dynamically based on touch inputs

### Library Management
- Custom/modified libraries are in `lib/` (ESP32Servo, lvgl, Nintendo_Extension_Ctrl, TFT_eSPI, XPT2046_Touchscreen)
- LVGL configuration is in `lib/lv_conf.h`
- Don't modify `lib/ui/` files directly - they're regenerated from UI design tool
