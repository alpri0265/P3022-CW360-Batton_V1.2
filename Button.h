#ifndef BUTTON_H
#define BUTTON_H

#include <Arduino.h>

// ---------------- Button Class (Simple button with debouncing and long press) ----------------
class Button {
public:
  Button(uint8_t pin);
  
  // Initialize button pin
  void begin();
  
  // Update button state (call periodically, e.g., every 10ms)
  void update();
  
  // Get button events (automatically reset after reading)
  bool wasPressed();      // Short press (click)
  bool wasLongPressed();  // Long press (>600ms)
  
  // Check if button is currently held down (including long press state)
  bool isPressed() const { return state_ == BTN_DOWN || state_ == BTN_LONG_PRESS; }

private:
  uint8_t pin_;
  
  // Button state machine
  enum State : uint8_t {
    BTN_UP = 0,          // Button not pressed
    BTN_DEBOUNCE_DOWN,   // Button pressed, debouncing
    BTN_DOWN,            // Button confirmed pressed
    BTN_DEBOUNCE_UP,     // Button released, debouncing
    BTN_LONG_PRESS       // Long press detected
  };
  
  State state_;
  uint32_t lastChangeMs_;
  uint32_t pressStartMs_;
  bool clickEvent_;
  bool longPressEvent_;
  bool hadLongPress_;  // Track if long press occurred in this press cycle (prevents click after long press)
  
  static const uint16_t DEBOUNCE_MS = 25;      // Debounce time
  static const uint16_t LONG_PRESS_MS = 600;   // Long press threshold
};

#endif // BUTTON_H
