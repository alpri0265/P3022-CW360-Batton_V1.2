#ifndef LCDDISPLAY_H
#define LCDDISPLAY_H

#include <Arduino.h>
#include <string.h>
#include "Config.h"

#if defined(LCD_INTERFACE_I2C)
  #include <LiquidCrystal_I2C.h>
  // Forward declaration - global lcd object will be defined in main .ino
  extern LiquidCrystal_I2C lcd;
#elif defined(LCD_INTERFACE_PARALLEL_4BIT)
  #include <LiquidCrystal.h>
  // Forward declaration - global lcd object will be defined in main .ino
  extern LiquidCrystal lcd;
#endif

// ---------------- LCD Display Class ----------------
class LCDDisplay {
public:
  // Constructor - takes reference to global LCD object
  #if defined(LCD_INTERFACE_I2C)
    LCDDisplay(LiquidCrystal_I2C& lcdRef);
  #elif defined(LCD_INTERFACE_PARALLEL_4BIT)
    LCDDisplay(LiquidCrystal& lcdRef);
  #endif

  // Initialize LCD display
  void begin();

  // Show startup message
  void showStartup();

  // Set line text (buffered, doesn't update display immediately)
  void setLine(uint8_t row, const char* s);

  // Update display with buffered lines (minimal redraw)
  void flush();

  // Clear display and buffers
  void clear();

  // Get LCD object reference for direct access if needed
  #if defined(LCD_INTERFACE_I2C)
    LiquidCrystal_I2C& getLCD();
  #elif defined(LCD_INTERFACE_PARALLEL_4BIT)
    LiquidCrystal& getLCD();
  #endif

private:
  bool initialized_;
  uint8_t lineSize_;
  
  // Line buffers (static allocation)
  #if LCD_COLS == 16
    char lines_[LCD_ROWS][17];
    char prevLines_[LCD_ROWS][17];
  #elif LCD_COLS == 20
    char lines_[LCD_ROWS][21];
    char prevLines_[LCD_ROWS][21];
  #endif
  
  // Reference to global LCD object
  #if defined(LCD_INTERFACE_I2C)
    LiquidCrystal_I2C& lcd;
  #elif defined(LCD_INTERFACE_PARALLEL_4BIT)
    LiquidCrystal& lcd;
  #endif
};

#endif // LCDDISPLAY_H
