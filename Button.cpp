#include "Button.h"

Button::Button(uint8_t pin) 
  : pin_(pin), state_(BTN_UP), lastChangeMs_(0), pressStartMs_(0),
    clickEvent_(false), longPressEvent_(false), hadLongPress_(false) {
}

void Button::begin() {
  pinMode(pin_, INPUT_PULLUP);
  state_ = BTN_UP;
  lastChangeMs_ = millis();
  pressStartMs_ = 0;
  clickEvent_ = false;
  longPressEvent_ = false;
  hadLongPress_ = false;
}

void Button::update() {
  bool pressed = !digitalRead(pin_);  // LOW = pressed (pullup)
  uint32_t now = millis();
  
  switch (state_) {
    case BTN_UP:
      if (pressed) {
        state_ = BTN_DEBOUNCE_DOWN;
        lastChangeMs_ = now;
      }
      break;
      
    case BTN_DEBOUNCE_DOWN:
      if (pressed) {
        if ((uint32_t)(now - lastChangeMs_) >= DEBOUNCE_MS) {
          state_ = BTN_DOWN;
          pressStartMs_ = now;
          clickEvent_ = false;
          longPressEvent_ = false;
          hadLongPress_ = false;  // Reset long press flag for new press cycle
        }
      } else {
        // False press, back to UP
        state_ = BTN_UP;
        hadLongPress_ = false;
      }
      break;
      
    case BTN_DOWN:
      if (!pressed) {
        state_ = BTN_DEBOUNCE_UP;
        lastChangeMs_ = now;
      } else {
        // Check for long press
        if (!hadLongPress_ && (uint32_t)(now - pressStartMs_) >= LONG_PRESS_MS) {
          state_ = BTN_LONG_PRESS;
          longPressEvent_ = true;
          hadLongPress_ = true;  // Mark that long press occurred
          clickEvent_ = false;  // Prevent click event if long press detected
        }
      }
      break;
      
    case BTN_DEBOUNCE_UP:
      if (!pressed) {
        if ((uint32_t)(now - lastChangeMs_) >= DEBOUNCE_MS) {
          // Button released - fire click event only if no long press occurred in this cycle
          if (!hadLongPress_) {
            clickEvent_ = true;
          }
          // Reset flags when fully released
          state_ = BTN_UP;
          hadLongPress_ = false;
        }
      } else {
        // Still pressed, back to DOWN
        state_ = BTN_DOWN;
      }
      break;
      
    case BTN_LONG_PRESS:
      if (!pressed) {
        state_ = BTN_DEBOUNCE_UP;
        lastChangeMs_ = now;
        // hadLongPress_ remains true to prevent click event when fully released
      }
      // Stay in LONG_PRESS state while button is held
      break;
  }
}

bool Button::wasPressed() {
  if (clickEvent_) {
    clickEvent_ = false;
    return true;
  }
  return false;
}

bool Button::wasLongPressed() {
  if (longPressEvent_) {
    longPressEvent_ = false;
    // Reset state if we're in LONG_PRESS
    if (state_ == BTN_LONG_PRESS) {
      state_ = BTN_UP;
    }
    return true;
  }
  return false;
}
