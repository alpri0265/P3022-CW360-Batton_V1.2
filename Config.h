#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ---------------- Board Detection ----------------
// Auto-detect board type for compatibility
#if defined(__AVR_ATmega32U4__)
  #define BOARD_TYPE "Micro (32U4)"
#elif defined(__AVR_ATmega328P__) || defined(__AVR_ATmega328__)
  #define BOARD_TYPE "Uno/Nano (328P)"
#else
  #define BOARD_TYPE "Unknown"
#endif

// ---------------- LCD Configuration ----------------
// Choose LCD type: 1602 (16x2) or 2004 (20x4)
#define LCD_TYPE_1602
// #define LCD_TYPE_2004

// Choose interface: I2C or PARALLEL_4BIT
// #define LCD_INTERFACE_I2C
#define LCD_INTERFACE_PARALLEL_4BIT

#if defined(LCD_TYPE_1602)
  #define LCD_COLS 16
  #define LCD_ROWS 2
#elif defined(LCD_TYPE_2004)
  #define LCD_COLS 20
  #define LCD_ROWS 4
#else
  #error "Please define LCD_TYPE_1602 or LCD_TYPE_2004"
#endif

// I2C address for I2C interface
//#define LCD_I2C_ADDR 0x27  // Change to 0x3F if needed

// Pin definitions for 4-bit parallel interface
#define PIN_LCD_RS 12
#define PIN_LCD_EN 11
#define PIN_LCD_D4 7
#define PIN_LCD_D5 6
#define PIN_LCD_D6 5
#define PIN_LCD_D7 8

// ---------------- Pin Definitions ----------------
// Button pins for menu navigation (all buttons use INPUT_PULLUP - connect to GND when pressed)
// These pins are compatible across Uno, Nano, and Micro
// Note: D5 is used by LCD (PIN_LCD_D6), so BACK button is on D9
static const uint8_t PIN_BTN_UP   = 2;   // Button UP (move menu up)
static const uint8_t PIN_BTN_DOWN = 3;   // Button DOWN (move menu down)
static const uint8_t PIN_BTN_OK   = 4;   // Button OK (select/confirm)
static const uint8_t PIN_BTN_BACK = 9;   // Button BACK (cancel/back) - D5 занят LCD, використано D9

static const uint8_t PIN_ANGLE  = A0;  // Analog input for P3022 sensor

// ---------------- Timing Constants ----------------
static const uint16_t BUTTON_TICK_MS = 10;   // Button processing: 10ms (debouncing and long press detection)
static const uint16_t UI_TICK_MS = 20;       // UI update: 20ms = 50Hz (reduced from 10ms to reduce flickering)

// Note: Encoder functionality removed in Button_V1.1 branch - replaced with button navigation

#endif // CONFIG_H
