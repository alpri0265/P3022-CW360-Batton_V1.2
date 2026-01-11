#ifndef SENSOR_H
#define SENSOR_H

#include <Arduino.h>
#include "Config.h"
#include "Settings.h"

// ---------------- ADC / angle math ----------------
// Read ADC with averaging for better stability
// Compatible with all AVR boards (Uno/Nano/Micro have same ADC resolution: 10-bit = 0-1023)
uint16_t readAdcAvg16();

// Convert ADC value to angle (centidegrees: 0..35999)
// Applies calibration and optional inversion
uint16_t adcToAngle100(uint16_t adc);

// Apply zero offset to angle
uint16_t applyZero100(uint16_t angle100);

#endif // SENSOR_H
