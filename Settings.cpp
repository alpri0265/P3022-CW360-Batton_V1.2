#include "Settings.h"

Settings S;

uint8_t simple_crc(const Settings& s) {
  const uint8_t* p = (const uint8_t*)&s;
  uint8_t c = 0;
  for (size_t i = 0; i < sizeof(Settings) - 1; i++) c ^= p[i];
  return c;
}

void saveSettings() {
  S.crc = simple_crc(S);
  EEPROM.put(0, S);
}

void loadSettings() {
  EEPROM.get(0, S);
  
  // Validate loaded settings
  bool bad =
    (S.crc != simple_crc(S)) ||      // CRC mismatch = corrupted data
    (S.calMin >= S.calMax) ||         // Invalid calibration range
    (S.calMax > 1023) ||              // ADC max out of range
    (S.zero100 >= 36000);             // Zero offset out of range

  if (bad) {
    // Load defaults if validation failed (first run or corrupted EEPROM)
    S.zero100 = 0;
    S.calMin  = 0;
    S.calMax  = 1023;
    S.flags   = 0;
    saveSettings();
  }
}

void doSetValue(uint16_t raw100, uint16_t target100) {
  int32_t z = (int32_t)raw100 - (int32_t)target100;
  // Normalize to 0..35999 range
  if (z < 0) z += 36000;
  else if (z >= 36000) z -= 36000;
  S.zero100 = (uint16_t)z;
  saveSettings();
}

void doSetZero(uint16_t raw100) {
  // Set zero offset so that the current raw angle becomes the new zero point
  // This means: displayed_angle = raw100 - zero100
  // To make displayed_angle = 0, we set: zero100 = raw100
  S.zero100 = raw100;
  saveSettings();
}

void doCalMin(uint16_t adc) {
  // Set calibration minimum, ensure it's less than max
  S.calMin = adc;
  if (S.calMin >= S.calMax) {
    S.calMax = S.calMin + 1;
    if (S.calMax > 1023) S.calMax = 1023;
  }
  saveSettings();
}

void doCalMax(uint16_t adc) {
  // Set calibration maximum, ensure it's greater than min
  S.calMax = adc;
  if (S.calMax <= S.calMin) {
    S.calMin = (S.calMax > 0) ? (S.calMax - 1) : 0;
  }
  saveSettings();
}

void doInvertToggle() {
  S.flags ^= 0x01;
  saveSettings();
}
