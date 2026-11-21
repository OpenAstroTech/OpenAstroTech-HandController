#pragma once
#include "Constants.hpp"

#define DEBUG_SERIAL_PORT Serial

// Guards allow build flags (e.g. the native test env) to override these.
#ifndef DEBUG_LEVEL
#define DEBUG_LEVEL (DEBUG_NONE)
#endif

#ifndef INFO_DISPLAY
#define INFO_DISPLAY 1
#endif

// Rotary encoder with push button (CLK, DT, SW) - the only on-device input,
// driving the menu. Placeholder pins until the hardware is wired; GPIO 16/17
// (USB host D+/D-) and 18 (VBUS enable) must stay reserved for the mount port.
#ifndef ROTARY_ENCODER_CLK_PIN
#define ROTARY_ENCODER_CLK_PIN 19
#endif
#ifndef ROTARY_ENCODER_DT_PIN
#define ROTARY_ENCODER_DT_PIN 20
#endif
#ifndef ROTARY_ENCODER_SW_PIN
#define ROTARY_ENCODER_SW_PIN 21
#endif

// SSD1306 OLED on I²C, board-default SDA/SCL pins.
#ifndef DISPLAY_SSD1306_I2C_ADDRESS
#define DISPLAY_SSD1306_I2C_ADDRESS 0x3C
#endif
