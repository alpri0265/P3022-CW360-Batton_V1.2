#ifndef UTILS_H
#define UTILS_H

#include <Arduino.h>

// Format angle from centidegrees (0..35999) to string "359°59'" (degrees and arcminutes)
// Example: 35999 -> "359°59'", 12345 -> "123°27'", 1234 -> " 12°20'"
void formatAngle100(char* out, uint16_t a100);

#endif // UTILS_H
