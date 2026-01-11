#include "Encoder.h"

// Static instance pointer (initialized to nullptr)
Encoder* Encoder::instance_ = nullptr;

Encoder::Encoder(uint8_t pinA, uint8_t pinB, uint8_t pinSW) 
  : pinA_(pinA), pinB_(pinB), pinSW_(pinSW),
    prevAB_(0), isrDelta_(0),
    swPrevUp_(true), swLastUp_(true), swLastChangeMs_(0),
    swDownMs_(0), longFired_(false) {
  // State is initialized by default constructor
  // Set static instance pointer for ISR callbacks
  instance_ = this;
}

void Encoder::begin() {
  pinMode(pinA_, INPUT_PULLUP);
  pinMode(pinB_, INPUT_PULLUP);
  pinMode(pinSW_, INPUT_PULLUP);
  
  // Read initial state to avoid false first movement
  uint8_t a = digitalRead(pinA_) & 1;
  uint8_t b = digitalRead(pinB_) & 1;
  prevAB_ = (a << 1) | b;
  isrDelta_ = 0;
  
  swPrevUp_ = digitalRead(pinSW_);
  swLastUp_ = swPrevUp_;
  swLastChangeMs_ = millis();
  
  // Attach interrupts for encoder pins
  // Both pins use CHANGE mode to catch all transitions
  // Note: For Uno/Nano, pin 2 = INT0, pin 3 = INT1
  //       For Micro, pin 3 = INT0, pin 2 = INT1 (but both work)
  attachInterrupt(digitalPinToInterrupt(pinA_), isrA, CHANGE);
  attachInterrupt(digitalPinToInterrupt(pinB_), isrB, CHANGE);
}

// Static ISR callback for pin A
void Encoder::isrA() {
  if (instance_) {
    instance_->handleInterrupt();
  }
}

// Static ISR callback for pin B
void Encoder::isrB() {
  if (instance_) {
    instance_->handleInterrupt();
  }
}

// Quadrature decode (called from ISR - must be fast and use only volatile variables)
void Encoder::handleInterrupt() {
  // Read current state (in ISR, must be fast)
  uint8_t a = digitalRead(pinA_) & 1;
  uint8_t b = digitalRead(pinB_) & 1;
  uint8_t ab = (a << 1) | b;
  
  // Lookup table for quadrature decoding (Gray code sequence)
  // Index: (prevAB << 2) | currentAB (4 bits)
  // This table gives the direction: +1 for CW, -1 for CCW, 0 for invalid/no change
  static const int8_t abTable[16] = {
    0,  1, -1,  0,  // prev=00: 00->00=0, 00->01=+1, 00->10=-1, 00->11=0
   -1,  0,  0,  1,  // prev=01: 01->00=-1, 01->01=0, 01->10=0, 01->11=+1
    1,  0,  0, -1,  // prev=10: 10->00=+1, 10->01=0, 10->10=0, 10->11=-1
    0, -1,  1,  0   // prev=11: 11->00=0, 11->01=-1, 11->10=+1, 11->11=0
  };
  
  uint8_t idx = (prevAB_ << 2) | ab;
  int8_t step = abTable[idx];
  
  if (step != 0) {
    // Valid transition - accumulate delta in ISR
    isrDelta_ += step;
    prevAB_ = ab;  // Update previous state for next interrupt
  }
}

void Encoder::update() {
  static const uint16_t SW_DEBOUNCE_MS = 25;      // Button debounce time
  
  // Transfer delta from ISR to main state (atomic read)
  // Disable interrupts briefly to read volatile variable safely
  noInterrupts();
  int16_t delta = isrDelta_;
  isrDelta_ = 0;  // Reset ISR counter
  interrupts();
  
  // Add ISR delta to main state
  if (delta != 0) {
    state_.delta += delta;
  }

  // Button handling with debouncing and long press detection
  bool up = digitalRead(pinSW_); // true=not pressed (pullup), false=pressed
  uint32_t now = millis();

  // Track button state changes for debouncing
  if (up != swLastUp_) {
    swLastUp_ = up;
    swLastChangeMs_ = now;
  }

  // Apply debouncing: only accept state change after debounce period
  if ((uint32_t)(now - swLastChangeMs_) >= SW_DEBOUNCE_MS && up != swPrevUp_) {
    swPrevUp_ = up;
    if (!swPrevUp_) { // Button pressed
      swDownMs_ = now;
      longFired_ = false;
    } else { // Button released (after debounce)
      if (!longFired_) state_.click = true; // Only fire click if long press wasn't triggered
    }
  }
  
  // Long press detection: >600ms hold
  if (!swPrevUp_ && !longFired_ && (uint32_t)(now - swDownMs_) > 600) {
    state_.longClick = true;
    longFired_ = true; // Prevent click event when released
  }
}

Encoder::State Encoder::getState(bool resetFlags) {
  State s = state_;
  if (resetFlags) {
    state_.delta = 0;
    state_.click = false;
    state_.longClick = false;
  }
  return s;
}

void Encoder::consumeDelta(int16_t amount) {
  // Subtract consumed amount from delta
  // Works correctly for both positive and negative values
  if (amount > 0 && state_.delta > 0) {
    // Consume positive amount from positive delta
    if (state_.delta >= amount) {
      state_.delta -= amount;
    } else {
      state_.delta = 0;
    }
  } else if (amount < 0 && state_.delta < 0) {
    // Consume negative amount from negative delta
    // Both are negative. We want to subtract amount (which is negative, so we add its magnitude)
    // Check: |delta| >= |amount|, which for negatives means: -delta >= -amount, or delta <= amount
    if (state_.delta <= amount) {  // -24 <= -8 is true, -3 <= -8 is false
      state_.delta -= amount;  // -24 - (-8) = -16, -16 - (-8) = -8, -8 - (-8) = 0
    } else {
      // |delta| < |amount|, don't consume (partial detent)
      // state_.delta stays unchanged
    }
  }
  // If amount and delta have different signs, do nothing (shouldn't happen in normal use)
}

void Encoder::resetButtonFlags() {
  state_.click = false;
  state_.longClick = false;
}
