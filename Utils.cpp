#include "Utils.h"

void formatAngle100(char* out, uint16_t a100) {
  // Convert from centidegrees (0..35999) to degrees and minutes
  // Format: "359°59'" (degrees and arcminutes)
  // Using byte 0xDF (223) which is degree symbol (°) in HD44780 LCD character ROM
  uint16_t deg = a100 / 100;              // Whole degrees (0..359)
  uint16_t centidegrees = a100 % 100;     // Remaining centidegrees (0..99)
  
  // Convert centidegrees to arcminutes: 1 centidegree = 0.01° = 0.6 arcminutes
  // Formula: minutes = centidegrees * 60 / 100
  // For stable display: use proper rounding with consistent algorithm
  // Standard rounding: (value * 60 + 50) / 100 rounds to nearest integer
  // Examples: 0→0, 1→1, 2→1, 3→2, 83→50, 84→50, 99→59
  uint32_t minutes_scaled = (uint32_t)centidegrees * 60UL;  // Avoid overflow: max 99 * 60 = 5940
  // Round to nearest: add 50 before dividing by 100
  // This ensures: 0.0-0.49 → 0, 0.5-1.49 → 1, 1.5-2.49 → 2, etc.
  uint8_t min = (uint8_t)((minutes_scaled + 50UL) / 100UL);
  if (min >= 60) min = 59;  // Clamp to 59 (safety check - shouldn't happen with centidegrees <= 99)
  
  // Format: "359°59'" (7 chars: 3 digits + degree symbol + 2 digits + apostrophe)
  sprintf(out, "%3u%c%02u'", deg, (char)0xDF, min);
  out[8] = 0;  // Ensure null terminator (safety)
}
