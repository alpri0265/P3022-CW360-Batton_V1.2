#ifndef MENUMANAGER_H
#define MENUMANAGER_H

#include <Arduino.h>
#include "LCDDisplay.h"
#include "Settings.h"
#include "Utils.h"
#include "Config.h"

// ---------------- Menu Manager Class ----------------
class MenuManager {
public:
  enum Screen : uint8_t {
    SCR_MAIN = 0,
    SCR_MENU,
    SCR_VIEW,
    SCR_ADC,
    SCR_ZERO,
    SCR_SETVALUE,
    SCR_CALMIN,
    SCR_CALMAX,
    SCR_INVERT,
  };

  // Callback function types for settings actions
  typedef void (*SetZeroCallback)(uint16_t);
  typedef void (*SetValueCallback)(uint16_t, uint16_t);
  typedef void (*CalMinCallback)(uint16_t);
  typedef void (*CalMaxCallback)(uint16_t);
  typedef void (*InvertToggleCallback)();

  MenuManager(LCDDisplay& lcd, SetZeroCallback setZero, SetValueCallback setValue,
              CalMinCallback calMin, CalMaxCallback calMax, InvertToggleCallback invertToggle,
              Settings* settings);

  // Process UI events and update display
  // Button events: up, down, ok, back, okLong (okLong = long press on OK)
  void update(uint16_t adc, uint16_t raw100, uint16_t shown100, 
              bool btnUp, bool btnDown, bool btnOk, bool btnBack, bool btnOkLong = false);

         // Get current screen
         Screen getCurrentScreen() const { return currentScreen_; }
         
         // Get current menu index (for debug)
         uint8_t getMenuIndex() const { return menuIdx_; }
         
         // Reset display smoothing (call after setting zero to show exact value immediately)
         void resetDisplaySmoothing();

private:
  LCDDisplay& lcd_;
  Screen currentScreen_;
  uint8_t menuIdx_;
  
  // Set Value editor state
  uint16_t target100_; // 0..35999
  uint16_t step100_;   // 1=0.01°, 2=1min, 10=0.1°, 17=10min, 100=1°, 1000=10°, 10000=100°
  
  // Menu items
  static const uint8_t MENU_N = 2;
  const char* menuItems_[MENU_N];
  
  // Button event cooldown to prevent double-processing
  uint32_t lastButtonEventMs_;
  Screen previousScreen_;  // Track previous screen to detect transitions
  static const uint32_t BUTTON_EVENT_COOLDOWN_MS = 200;  // Minimum time between button events (200ms)
  
  uint32_t lastOkLongPressMs_;  // Track when OK long press happened (for ignoring click after long press)
  static const uint32_t OK_LONG_PRESS_IGNORE_CLICK_MS = 500;  // Ignore OK click for 500ms after long press
  
  // Display smoothing and hysteresis to prevent flickering of last digits
  uint16_t lastDisplayedAngle100_;  // Last displayed angle value (for hysteresis)
  uint16_t smoothedAngle100_;       // Smoothed angle value (exponential filter)
  bool smoothingResetFlag_;         // Flag to indicate smoothing was reset (don't reinitialize from shown100)
  static const uint16_t DISPLAY_HYSTERESIS_100 = 10;  // Minimum change to update display (0.10° = maximum stability)
  static const uint8_t SMOOTHING_FACTOR = 2;  // Smoothing factor: lower = more smoothing (1-15, 2 = very strong smoothing)
  static const uint16_t ZERO_THRESHOLD_100 = 20;  // If shown value is < 0.20° after reset, keep it at 0 (prevents 0.14° drift)
  
  // Callbacks for settings actions
  SetZeroCallback setZero_;
  SetValueCallback setValue_;
  CalMinCallback calMin_;
  CalMaxCallback calMax_;
  InvertToggleCallback invertToggle_;
  Settings* settings_;

  // Clamp value to range [lo, hi]
  static inline int16_t clampi16(int16_t v, int16_t lo, int16_t hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
  }

  // Process button events and update state machine
  // screenBefore is the screen state BEFORE processing (passed from update() to detect transitions)
  void processEvents(uint16_t adc, uint16_t raw100, uint16_t shown100,
                     bool btnUp, bool btnDown, bool btnOk, bool btnBack, bool btnOkLong, Screen screenBefore);

  // Render current screen to LCD
  void render(uint16_t adc, uint16_t raw100, uint16_t shown100);
};

#endif // MENUMANAGER_H
