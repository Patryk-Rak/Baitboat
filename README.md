<p align="center">
  <a href="" rel="noopener">
 <img width=200px height=200px src="https://i.imgur.com/6wj0hh6.jpg" alt="Project logo"></a>
</p>

<h3 align="center">Baitboat Software</h3>

<div align="center">

[![Status](https://img.shields.io/badge/status-active-success.svg)]()
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](/LICENSE)

</div>

---

<p align="center"> Firmware for a remote controlled fishing baitboat built with two ESP32 microcontrollers.
    <br>
</p>

## 📝 Table of Contents

- [About](#about)
- [Getting Started](#getting_started)
- [Deployment](#deployment)
- [Usage](#usage)
- [Built Using](#built_using)
- [Contributing](../CONTRIBUTING.md)
- [Authors](#authors)
- [Acknowledgments](#acknowledgement)

## 🧐 About <a name = "about"></a>

This project is the embedded software for a DIY fishing baitboat. The system uses two ESP32 DevKitC V4 boards communicating wirelessly via the ESP-NOW protocol. One ESP32 acts as the handheld controller — it reads input from a Nintendo Wii Nunchuk joystick, displays real-time telemetry on a 2.4" ILI9341 TFT touchscreen using LVGL, and transmits directional commands to the boat. The second ESP32 sits on the boat, receives those commands, and drives two DC motors through a DRI0041 (L298N) motor driver for differential steering, as well as a SG90 servo for bait release.

The controller features a graphical UI with a loading screen, connection status monitoring, and live speed bar indicators for each direction. Motor control on the boat side includes soft start/stop for smooth acceleration and a minimum PWM threshold to reliably overcome motor inertia.

## 🏁 Getting Started <a name = "getting_started"></a>

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes. See [deployment](#deployment) for notes on how to deploy the project on a live system.

### Prerequisites

What things you need to install the software and how to install them.

```
Give examples
```

### Installing

A step by step series of examples that tell you how to get a development env running.

Say what the step will be

```
Give the example
```

And repeat

```
until finished
```

End with an example of getting some data out of the system or using it for a little demo.

## 🔧 Running the tests <a name = "tests"></a>

Explain how to run the automated tests for this system.

### Break down into end to end tests

Explain what these tests test and why

```
Give an example
```

### And coding style tests

Explain what these tests test and why

```
Give an example
```

## 🎈 Usage <a name="usage"></a>

This project is for personal use only. I do not take any responsibility for damages, injuries, loss of equipment, or any other issues that may arise from using this software or hardware. Use at your own risk.

## 🚀 Deployment <a name = "deployment"></a>

This project is currently under active development and is still unoptimised. Expect breaking changes and incomplete features.

## ⛏️ Built Using <a name = "built_using"></a>

### Hardware
- [ESP32 DevKitC V4](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/hw-reference/esp32/get-started-devkitc.html) - Microcontroller (x2)
- [ILI9341 2.4" TFT LCD](https://www.adafruit.com/product/2478) - Touchscreen Display
- [XPT2046](https://www.buydisplay.com/download/ic/XPT2046.pdf) - Touch Controller
- [DRI0041 / L298N](https://wiki.dfrobot.com/Dual_H-Bridge_Motor_Driver-DRI0041_SKU__DRI0041) - DC Motor Driver
- [SG90](https://www.towerpro.com.tw/product/sg90-7/) - Servo Motor
- [Nintendo Wii Nunchuk](https://www.nintendo.com/) - Joystick Controller

### Software & Frameworks
- [PlatformIO](https://platformio.org/) - Build System & IDE
- [Arduino Framework](https://www.arduino.cc/) - ESP32 Runtime Framework
- [ESP-NOW](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/network/esp_now.html) - Wireless Communication Protocol

### Libraries
- [LVGL](https://lvgl.io/) - Graphics Library for Embedded UI
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) - TFT Display Driver
- [XPT2046_Touchscreen](https://github.com/PaulStoffregen/XPT2046_Touchscreen) - Touch Input Driver
- [Nintendo Extension Ctrl](https://github.com/dmadison/NintendoExtensionCtrl) - Nunchuk I2C Communication
- [ESP32Servo](https://github.com/madhephaestus/ESP32Servo) - Servo Control for ESP32
- [Unity](https://github.com/ThrowTheSwitch/Unity) - Unit Testing Framework

## ✍️ Authors <a name = "authors"></a>

- [@Dan Timefly](https://github.com/Patryk-Rak) - Idea & Development

## 🎉 Acknowledgements <a name = "acknowledgement"></a>

- Hat tip to anyone whose code was used
- Inspiration
- References
