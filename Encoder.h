#ifndef ENCODER_H
#define ENCODER_H

#include <Arduino.h>

// ---------------- Encoder Class (Interrupt-based) ----------------
// Encoder rotation is handled by external interrupts (INT0/INT1) for precise, real-time detection.
// Both encoder pins (A and B) use CHANGE interrupt mode to catch all quadrature transitions.
// Button handling uses polling in update() method (called periodically).
// 
// Hardware requirements:
// - PIN_ENC_A and PIN_ENC_B must be interrupt-capable pins:
//   - Arduino Uno/Nano: Pin 2 (INT0), Pin 3 (INT1)
//   - Arduino Micro: Pin 2 (INT1), Pin 3 (INT0)
//   digitalPinToInterrupt() automatically maps pins to correct interrupt numbers.
//
// Performance:
// - ISR executes in <10 microseconds, very low CPU overhead
// - No missed encoder steps even at high rotation speeds
// - Button debouncing handled in main loop (10ms polling is sufficient)
class Encoder {
public:
  struct State {
    int16_t delta;      // Accumulated detents (+/-)
    bool click;         // Short click event
    bool longClick;     // Long press event (>600ms)
    
    State() : delta(0), click(false), longClick(false) {}
  };

  Encoder(uint8_t pinA, uint8_t pinB, uint8_t pinSW);
  
  // Initialize encoder pins and attach interrupts
  void begin();
  
  // Update button state (call periodically, e.g., every 10ms)
  // Encoder rotation is handled by interrupts, this only processes button
  void update();
  
  // Get current state (and optionally reset flags)
  State getState(bool resetFlags = true);
  
  // Consume (subtract) specific amount from delta, used for partial processing
  void consumeDelta(int16_t amount);
  
  // Reset only button flags (click/longClick), leave delta unchanged
  void resetButtonFlags();

private:
  uint8_t pinA_, pinB_, pinSW_;
  State state_;
  
  // Encoder rotation state (volatile because modified in ISR)
  volatile uint8_t prevAB_;      // Previous AB state (from ISR)
  volatile int16_t isrDelta_;    // Delta accumulated in ISR (atomic counter)
  
  // Button state (not volatile, modified only in update())
  bool swPrevUp_;
  bool swLastUp_;
  uint32_t swLastChangeMs_;
  uint32_t swDownMs_;
  bool longFired_;
  
  // Static instance pointer for ISR callbacks
  static Encoder* instance_;
  
  // ISR callback functions (must be static)
  static void isrA();
  static void isrB();
  
  // Quadrature decode function (called from ISR)
  void handleInterrupt();
};

#endif // ENCODER_H
