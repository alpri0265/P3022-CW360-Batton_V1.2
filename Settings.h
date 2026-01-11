#ifndef SETTINGS_H
#define SETTINGS_H

#include <Arduino.h>
#include <EEPROM.h>

// ---------------- Settings in EEPROM ----------------
struct Settings {
  uint16_t zero100;  // 0..35999 (0.01°)
  uint16_t calMin;   // ADC raw min (0..1023)
  uint16_t calMax;   // ADC raw max (0..1023)
  uint8_t  flags;    // bit0 invert
  uint8_t  crc;      // simple XOR crc
};

// Global settings instance
extern Settings S;

// Settings functions
uint8_t simple_crc(const Settings& s);
void saveSettings();
void loadSettings();

// Settings action functions
void doSetValue(uint16_t raw100, uint16_t target100);
void doSetZero(uint16_t raw100);
void doCalMin(uint16_t adc);
void doCalMax(uint16_t adc);
void doInvertToggle();

#endif // SETTINGS_H
