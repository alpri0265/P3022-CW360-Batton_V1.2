#include "Sensor.h"

extern Settings S;

uint16_t readAdcAvg16() {
  uint32_t acc = 0;
  // Increased averaging from 16 to 64 samples for maximum stability and reduced noise
  // This provides better noise reduction at the cost of slightly longer read time (~6.4ms)
  for (uint8_t i = 0; i < 64; i++) {
    acc += analogRead(PIN_ANGLE);
    // Small delay between reads for better stability and ADC settling
    // ADC conversion takes ~100us, delay ensures stable readings
    if (i < 63) delayMicroseconds(100);
  }
  return (uint16_t)(acc >> 6); // Divide by 64 => 0..1023
}

uint16_t adcToAngle100(uint16_t adc) {
  int32_t a = adc;

  // clamp by calibration
  if (a < (int32_t)S.calMin) a = S.calMin;
  if (a > (int32_t)S.calMax) a = S.calMax;

  int32_t span = (int32_t)S.calMax - (int32_t)S.calMin;
  if (span < 1) span = 1;

  // 0..35999 (0.01°). If calc reaches 36000, wrap to 0.
  int32_t ang100 = ((a - (int32_t)S.calMin) * 36000L) / span;
  if (ang100 >= 36000) ang100 = 0;

  // optional invert
  if (S.flags & 0x01) {
    ang100 = 36000 - ang100;
    if (ang100 >= 36000) ang100 = 0;
  }
  return (uint16_t)ang100;
}

uint16_t applyZero100(uint16_t angle100) {
  // Apply zero offset with proper wrap-around
  int32_t a = (int32_t)angle100 - (int32_t)S.zero100;
  // Normalize to 0..35999 range using modulo arithmetic
  if (a < 0) a += 36000;
  else if (a >= 36000) a -= 36000;
  return (uint16_t)a;
}
