#include "LCDDisplay.h"
#include <Wire.h>  // For I2C interface

#if defined(LCD_INTERFACE_I2C)
LCDDisplay::LCDDisplay(LiquidCrystal_I2C& lcdRef) : initialized_(false), lineSize_(LCD_COLS + 1), lcd(lcdRef) {
#elif defined(LCD_INTERFACE_PARALLEL_4BIT)
LCDDisplay::LCDDisplay(LiquidCrystal& lcdRef) : initialized_(false), lineSize_(LCD_COLS + 1), lcd(lcdRef) {
#endif
  // Allocate static buffers for all rows
  // Using static allocation instead of dynamic to avoid memory fragmentation
}

void LCDDisplay::begin() {
  if (initialized_) return;
  
  // Initialize I2C bus (only needed for I2C interface)
  #if defined(LCD_INTERFACE_I2C)
    Wire.begin(); // Uno/Nano: A4/A5, Micro: D2/D3 (handled automatically)
    delay(100); // Wait for I2C to stabilize
  #endif
  
  // Initialize LCD based on interface type
  #if defined(LCD_INTERFACE_I2C)
    lcd.init();  // I2C LCD uses init()
    lcd.backlight();  // Turn on backlight
  #elif defined(LCD_INTERFACE_PARALLEL_4BIT)
    lcd.begin(LCD_COLS, LCD_ROWS);  // 4-bit parallel LCD uses begin()
    delay(50); // Wait for LCD to stabilize after initialization
  #endif
  
  // Initialize line buffers
  for (uint8_t i = 0; i < LCD_ROWS; i++) {
    memset(lines_[i], 0, lineSize_);
    memset(prevLines_[i], 0, lineSize_);
  }
  
  initialized_ = true;
}

void LCDDisplay::showStartup() {
  if (!initialized_) begin();
  
  lcd.setCursor(0, 0); 
  lcd.print("P3022 Sensor");
  lcd.setCursor(0, 1);
  #if defined(__AVR_ATmega32U4__)
    lcd.print("Micro 32U4");
  #elif defined(__AVR_ATmega328P__) || defined(__AVR_ATmega328__)
    lcd.print("Uno/Nano 328");
  #else
    lcd.print("Initializing...");
  #endif
  #if LCD_ROWS >= 4
    lcd.setCursor(0, 2);
    #if defined(LCD_INTERFACE_I2C)
      lcd.print("I2C ");
    #elif defined(LCD_INTERFACE_PARALLEL_4BIT)
      lcd.print("4-bit ");
    #endif
    #if defined(LCD_TYPE_1602)
      lcd.print("1602");
    #elif defined(LCD_TYPE_2004)
      lcd.print("2004");
    #endif
    lcd.setCursor(0, 3);
    lcd.print("Ready...");
  #endif
  delay(800);
  
  // Clear and show ready message
  clear();
  setLine(0, "Ready");
  setLine(1, "Press for menu");
  #if LCD_ROWS >= 4
    setLine(2, "Long: Set Zero");
    setLine(3, "");
  #endif
  flush();
  delay(400);
}

void LCDDisplay::setLine(uint8_t row, const char* s) {
  if (row >= LCD_ROWS || !initialized_) return;
  
  char* dst = lines_[row];
  
  // Copy string and pad with spaces
  for (uint8_t i = 0; i < LCD_COLS; i++) {
    char c = s[i];
    if (c == 0) {
      dst[i] = ' ';
      for (uint8_t j = i + 1; j < LCD_COLS; j++) dst[j] = ' ';
      break;
    }
    dst[i] = c;
  }
  dst[LCD_COLS] = 0;
}

void LCDDisplay::flush() {
  if (!initialized_) return;
  
  // Update only changed lines for better performance
  for (uint8_t row = 0; row < LCD_ROWS; row++) {
    if (strcmp(prevLines_[row], lines_[row]) != 0) {
      lcd.setCursor(0, row);
      lcd.print(lines_[row]);
      memcpy(prevLines_[row], lines_[row], lineSize_);
    }
  }
}

void LCDDisplay::clear() {
  if (!initialized_) return;
  lcd.clear();
  for (uint8_t i = 0; i < LCD_ROWS; i++) {
    memset(lines_[i], 0, lineSize_);
    memset(prevLines_[i], 0, lineSize_);
  }
}

#if defined(LCD_INTERFACE_I2C)
LiquidCrystal_I2C& LCDDisplay::getLCD() {
  return lcd;
}
#elif defined(LCD_INTERFACE_PARALLEL_4BIT)
LiquidCrystal& LCDDisplay::getLCD() {
  return lcd;
}
#endif
