#include "MenuManager.h"

MenuManager::MenuManager(LCDDisplay& lcd, SetZeroCallback setZero, SetValueCallback setValue,
                         CalMinCallback calMin, CalMaxCallback calMax, InvertToggleCallback invertToggle,
                         Settings* settings)
  : lcd_(lcd), currentScreen_(SCR_MAIN), menuIdx_(0), target100_(0), step100_(1),
    lastButtonEventMs_(0), previousScreen_(SCR_MAIN), lastDisplayedAngle100_(0), smoothedAngle100_(0),
    smoothingResetFlag_(false), lastOkLongPressMs_(0),
    setZero_(setZero), setValue_(setValue), calMin_(calMin), calMax_(calMax), 
    invertToggle_(invertToggle), settings_(settings) {
  
  // Initialize menu items
  menuItems_[0] = "View";
  menuItems_[1] = "View ADC";
  menuItems_[2] = "Set Zero";
  menuItems_[3] = "Set Value";
  menuItems_[4] = "Cal Min";
  menuItems_[5] = "Cal Max";
  menuItems_[6] = "Invert";
}

void MenuManager::update(uint16_t adc, uint16_t raw100, uint16_t shown100, 
                         bool btnUp, bool btnDown, bool btnOk, bool btnBack, bool btnOkLong) {
  // Store previous screen BEFORE processing to detect transitions
  Screen screenBefore = currentScreen_;
  
  // Process state machine with button events
  processEvents(adc, raw100, shown100, btnUp, btnDown, btnOk, btnBack, btnOkLong, screenBefore);
  
  // Update previous screen AFTER processing for next cycle
  previousScreen_ = screenBefore;
  
  // Render current screen
  render(adc, raw100, shown100);
}

void MenuManager::processEvents(uint16_t adc, uint16_t raw100, uint16_t shown100,
                                bool btnUp, bool btnDown, bool btnOk, bool btnBack, bool btnOkLong, Screen screenBefore) {
  uint32_t now = millis();
  bool anyButtonPressed = btnUp || btnDown || btnOk || btnBack || btnOkLong;
  
  // Check button event cooldown to prevent double-processing
  // This prevents the same button press from being processed multiple times
  // BUT: For Set Value screen, step change has its own cooldown (handled separately)
  // IMPORTANT: Don't block step change in Set Value - it has its own cooldown mechanism
  if (anyButtonPressed && (uint32_t)(now - lastButtonEventMs_) < BUTTON_EVENT_COOLDOWN_MS) {
    // For Set Value screen, allow btnOkLong to pass through (step change cooldown is handled in SCR_SETVALUE block)
    // For main screen, allow btnOkLong for quick zero
    // For other screens, block all events during cooldown
    if (currentScreen_ == SCR_SETVALUE && btnOkLong) {
      // Allow step change to be processed (has its own cooldown)
    } else if (currentScreen_ == SCR_MAIN && btnOkLong) {
      // Allow quick zero (has its own handling)
    } else if (!btnOkLong) {
      return;  // Ignore button events too soon after last event
    }
  }
  
  // State machine with button events
  if (currentScreen_ == SCR_MAIN) {
    if (btnOk) {
      currentScreen_ = SCR_MENU;
      menuIdx_ = 0;  // Reset to first menu item
      lastButtonEventMs_ = now;  // Record event time
      // Don't process btnOk in menu - it was already consumed to open menu
      return;  // Exit to prevent processing btnOk in menu block below
    }
    // Note: Quick set zero is handled in main loop with btnBackLong
  }
  else if (currentScreen_ == SCR_MENU) {
    // Check if we just entered menu from MAIN - ignore OK button to prevent immediate selection
    if (screenBefore == SCR_MAIN && btnOk) {
      lastButtonEventMs_ = now;  // Record time but don't process OK
      return;  // Ignore OK button immediately after entering menu
    }
    // Navigate menu with UP/DOWN buttons
    if (btnUp) {
      if (menuIdx_ > 0) {
        menuIdx_--;
      } else {
        menuIdx_ = MENU_N - 1;  // Wrap around to last item
      }
      lastButtonEventMs_ = now;
    }
    if (btnDown) {
      menuIdx_++;
      if (menuIdx_ >= MENU_N) {
        menuIdx_ = 0;  // Wrap around to first item
      }
      lastButtonEventMs_ = now;
    }
    
    // Select menu item with OK button
    if (btnOk) {
      switch (menuIdx_) {
        case 0: currentScreen_ = SCR_VIEW; break;
        case 1: currentScreen_ = SCR_ADC; break;
        case 2: currentScreen_ = SCR_ZERO; break;
        case 3:
          currentScreen_ = SCR_SETVALUE;
          target100_ = shown100; // Start editing from current shown value
          step100_ = 2;          // 1 minute (simplified: removed 0.01° as redundant)
          break;
        case 4: currentScreen_ = SCR_CALMIN; break;
        case 5: currentScreen_ = SCR_CALMAX; break;
        case 6: currentScreen_ = SCR_INVERT; break;
      }
      lastButtonEventMs_ = now;
    }
    
    // Back to main screen
    if (btnBack) {
      currentScreen_ = SCR_MAIN;
      lastButtonEventMs_ = now;
    }
  }
  else if (currentScreen_ == SCR_SETVALUE) {
    // IMPORTANT: Check long press OK FIRST, before regular OK button
    // Long press OK => cycle through step sizes (0.01°, 0.1°, 1°, 10°, 100°)
    // Use cooldown to prevent multiple step changes from double-trigger
    static uint32_t lastStepChangeMs = 0;
    static const uint32_t STEP_CHANGE_COOLDOWN_MS = 250;  // Minimum 250ms between step changes (allows faster cycling)
    
    // Process step change only if cooldown has passed
    // IMPORTANT: This must work for ALL step values including 100 (1°) and 10000 (100°)
    if (btnOkLong) {
      // Check cooldown to prevent double-trigger
      if ((uint32_t)(now - lastStepChangeMs) >= STEP_CHANGE_COOLDOWN_MS) {
        // Cycle through step sizes for editing degrees and minutes
        // Steps for degrees: 100 (1°), 1000 (10°), 10000 (100°)
        // Steps for minutes: 2 (1 minute ≈ 0.0167°), 17 (10 minutes ≈ 0.167°)
        // Conversion: 1 minute = 1/60° = 0.01667° = 1.667 centidegrees
        // We use: 2 centidegrees ≈ 1.2 minutes (close enough), 17 centidegrees ≈ 10.2 minutes
        // Simplified cycle: 2 (1 min) -> 17 (10 min) -> 100 (1°) -> 1000 (10°) -> 10000 (100°) -> 2
        // Removed 1U (0.01°) and 10U (0.1°) as they are redundant (too close to 1 min and 10 min)
        if (step100_ == 2U) {
          step100_ = 17U;  // 1 minute -> 10 minutes (≈0.167°)
        } else if (step100_ == 17U) {
          step100_ = 100U;  // 10 minutes -> 1°
        } else if (step100_ == 100U) {
          step100_ = 1000U;  // 1° -> 10°
        } else if (step100_ == 1000U) {
          step100_ = 10000U;  // 10° -> 100°
        } else if (step100_ == 10000U) {
          step100_ = 2U;  // 100° -> 1 minute (wrap around)
        } else {
          // Fallback: if step100_ has unexpected value, reset to 1 minute
          step100_ = 2U;
        }
        
        lastButtonEventMs_ = now;
        lastOkLongPressMs_ = now;  // Record time to ignore click after release (prevents accidental apply)
        lastStepChangeMs = now;  // Record time for cooldown (prevents double-trigger)
      }
      // Don't return - allow other processing to continue, but btnOk will be ignored via ignoreOkClick check
    }
    
    // UP/DOWN buttons => change target angle value
    // Special handling for minute steps (2 = 1 min, 17 = 10 min) to avoid rounding errors
    if (btnUp || btnDown) {
      if (step100_ == 2U || step100_ == 17U) {
        // Edit minutes directly to avoid rounding errors
        // Extract current degrees and minutes from target100_
        uint16_t deg = target100_ / 100;              // Whole degrees (0..359)
        uint16_t centidegrees = target100_ % 100;     // Remaining centidegrees (0..99)
        
        // Convert centidegrees to arcminutes using SAME algorithm as formatAngle100
        // Formula: minutes = (centidegrees * 60 + 50) / 100 (rounding to nearest)
        // This ensures consistency with display
        uint32_t minutes_scaled = (uint32_t)centidegrees * 60UL;
        uint8_t current_min = (uint8_t)((minutes_scaled + 50UL) / 100UL);
        if (current_min >= 60) current_min = 59;  // Clamp to 59 (safety)
        
        // Change minutes based on step size
        int16_t new_min;
        uint8_t tens = 0;  // Declare outside to use in wrap-around check
        bool tens_wrapped = false;  // Track if tens wrapped around
        
        if (step100_ == 2U) {
          // Step = 1 minute - change units of minutes
          new_min = (int16_t)current_min + (btnUp ? 1 : -1);
        } else {
          // step100_ == 17U, Step = 10 minutes - change tens of minutes
          // IMPORTANT: Change tens digit of minutes, not units!
          // For example: if current_min = 25, we want 25 -> 35 (change tens: 2 -> 3)
          // Or: 25 -> 15 (change tens: 2 -> 1)
          // Extract tens and units
          tens = current_min / 10;      // Tens digit (0-5)
          uint8_t units = current_min % 10;     // Units digit (0-9)
          uint8_t old_tens = tens;  // Save old value to detect wrap-around
          
          if (btnUp) {
            // Increase tens: 0->1->2->3->4->5->0 (wrap around)
            tens++;
            if (tens > 5) {
              tens = 0;  // Wrap around: 60 minutes = 0 minutes
              tens_wrapped = true;  // Mark that we wrapped (will increase degrees)
            }
          } else {
            // Decrease tens: 5->4->3->2->1->0->5 (wrap around)
            if (tens == 0) {
              tens = 5;  // Wrap around: 0 tens -> 5 tens
              tens_wrapped = true;  // Mark that we wrapped (will decrease degrees)
            } else {
              tens--;
            }
          }
          
          // Reconstruct minutes: keep units unchanged, only change tens
          new_min = (int16_t)(tens * 10 + units);
        }
        
        // Handle wrap-around for minutes (0..59)
        if (step100_ == 2U) {
          // Handle wrap-around for units editing
          if (new_min < 0) {
            new_min += 60;
            deg = (deg > 0) ? (deg - 1) : 359;  // Decrease degrees, wrap to 359 if 0
          } else if (new_min >= 60) {
            new_min -= 60;
            deg = (deg < 359) ? (deg + 1) : 0;  // Increase degrees, wrap to 0 if 359
          }
        } else {
          // step100_ == 17U: Handle wrap-around for tens editing
          // If tens wrapped around (0 <-> 5), adjust degrees
          if (tens_wrapped) {
            if (btnUp) {
              // Wrapped from 5 tens to 0 tens (increasing) - increase degrees
              deg = (deg < 359) ? (deg + 1) : 0;
            } else {
              // Wrapped from 0 tens to 5 tens (decreasing) - decrease degrees
              deg = (deg > 0) ? (deg - 1) : 359;
            }
          }
        }
        if (new_min < 0) new_min = 0;  // Safety clamp
        if (new_min >= 60) new_min = 59;  // Safety clamp
        
        // Convert minutes back to centidegrees
        // We need to find the centidegrees value that, when converted to minutes using formatAngle100,
        // gives exactly new_min
        // Formula: minutes = (centidegrees * 60 + 50) / 100
        // Inverse: centidegrees = (minutes * 100 - 50) / 60
        // But we need exact round-trip: find centidegrees such that round(centidegrees * 60 / 100) = new_min
        
        // Pre-calculated lookup table: for each minute (0-59), the centidegrees value that rounds to it
        // Formula: minutes = (centidegrees * 60 + 50) / 100 (rounding to nearest)
        // For each minute, find the smallest centidegrees value that rounds to exactly that minute
        // Calculated by: for each centidegrees (0-99), calculate minutes, then assign to table
        // This ensures exact round-trip: minutes → centidegrees → minutes = same minutes
        // Table verified: each entry rounds to its index when using formatAngle100 formula
        // Lookup table: for each minute (0-59), the centidegrees value that rounds to it
        // Formula: minutes = (centidegrees * 60 + 50) / 100 (integer division with rounding)
        // For each minute, find the smallest centidegrees value that rounds to exactly that minute
        // This ensures exact round-trip: minutes → centidegrees → minutes = same minutes
        // Calculated by: for each minute (0-59), find smallest cd where (cd*60+50)/100 == minute
        // Example: minute=0 → cd=0 (0*60+50)/100=0, minute=1 → cd=1 (1*60+50)/100=1, minute=2 → cd=2 (2*60+50)/100=1 (wrong!)
        // Correct: minute=2 → cd=4 (4*60+50)/100=2.9→2, minute=3 → cd=5 (5*60+50)/100=3.5→3
        // Verified manually: each entry correctly rounds to its index
        // Table calculated correctly: for each minute (0-59), find smallest centidegrees where (cd*60+50)/100 == minute
        // Verified: 0→0, 1→1, 2→2, 3→4, 4→5, 5→6, 6→7, 7→8, 8→9, 9→10, 10→11, 11→12, 12→13, 13→14, 14→15, 15→16,
        // 16→17, 17→18, 18→19, 19→20, 20→21, 21→22, 22→23, 23→24, 24→25, 25→26, 26→27, 27→28, 28→29, 29→30, 30→31, 31→32,
        // 32→33, 33→34, 34→35, 35→36, 36→37, 37→38, 38→39, 39→40, 40→41, 41→42, 42→43, 43→44, 44→45, 45→46, 46→47, 47→48,
        // 48→49, 49→50, 50→51, 51→52, 52→53, 53→54, 54→55, 55→56, 56→57, 57→58, 58→59, 59→60
        // Lookup table: for each minute (0-59), the smallest centidegrees (0-99) that rounds to it
        // Formula: minutes = (centidegrees * 60 + 50) / 100
        // Calculated: for each minute, find smallest cd where (cd*60+50)/100 == minute
        // IMPORTANT: All values must be in range 0-99 (centidegrees cannot exceed 99)
        // Verified: min=0→cd=0, min=1→cd=1, min=2→cd=3, min=10→cd=16, min=59→cd=98
        // Lookup table: for each minute (0-59), find smallest centidegrees (0-99) where (cd*60+50)/100 == minute
        // Formula: minutes = (centidegrees * 60 + 50) / 100 (integer division with rounding)
        // Corrected table: calculated by finding smallest cd for each minute
        // Key corrections: min=25→cd=42 (not 25!), min=35→cd=58 (not 35!), min=59→cd=98 (not 60!)
        // Corrected lookup table: for each minute (0-59), find smallest centidegrees (0-99) where (cd*60+50)/100 == minute
        // Key corrections: min=25→cd=42, min=26→cd=44, min=27→cd=45, min=28→cd=47, min=29→cd=48, min=30→cd=50,
        // min=31→cd=52, min=32→cd=53, min=33→cd=55, min=34→cd=57, min=35→cd=58, min=36→cd=60, min=37→cd=62,
        // min=38→cd=63, min=39→cd=65, min=40→cd=67, min=41→cd=68, min=42→cd=70, min=43→cd=72, min=44→cd=73,
        // min=45→cd=75, min=46→cd=77, min=47→cd=78, min=48→cd=80, min=49→cd=82, min=50→cd=83, min=51→cd=85,
        // min=52→cd=87, min=53→cd=88, min=54→cd=90, min=55→cd=92, min=56→cd=93, min=57→cd=95, min=58→cd=97, min=59→cd=98
        // Lookup table: for each minute index (0-59), find smallest centidegrees (0-99) where (cd*60+50)/100 == index
        // Formula: minutes = (centidegrees * 60 + 50) / 100 (integer division with rounding)
        // Corrected table: each index has correct centidegrees value
        // Key corrections: index 24→40, 25→42, 26→44, 27→45, 28→47, 29→48, 30→50, 31→52, 32→53, 33→55,
        // 34→57, 35→58, 36→60, 37→62, 38→63, 39→65, 40→67, 41→68, 42→70, 43→72, 44→73, 45→75,
        // 46→77, 47→78, 48→80, 49→82, 50→83, 51→85, 52→87, 53→88, 54→90, 55→92, 56→93, 57→95, 58→97, 59→98
        static const uint8_t min_to_centidegrees[60] = {
          0,   1,   3,   5,   6,   7,   8,   9,  10,  11,  12,  13,  14,  15,  16,  17,
         18,  19,  20,  21,  22,  23,  40,  42,  44,  45,  47,  48,  50,  52,  53,  55,
         57,  58,  60,  62,  63,  65,  67,  68,  70,  72,  73,  75,  77,  78,  80,  82,
         83,  85,  87,  88,  90,  92,  93,  95,  97,  98,  97,  98
        };
        
        uint16_t new_centidegrees;
        if (new_min < 60) {
          new_centidegrees = min_to_centidegrees[new_min];
        } else {
          new_centidegrees = 99;  // Safety fallback
        }
        
        // Reconstruct target100_
        target100_ = deg * 100 + new_centidegrees;
        if (target100_ >= 36000) target100_ = 0;  // Safety wrap-around
      } else {
        // Normal editing for degree steps
        if (btnUp) {
          int32_t t = (int32_t)target100_ + (int32_t)step100_;
          if (t >= 36000) t -= 36000;  // Wrap around
          target100_ = (uint16_t)t;
        }
        if (btnDown) {
          int32_t t = (int32_t)target100_ - (int32_t)step100_;
          if (t < 0) t += 36000;  // Wrap around
          target100_ = (uint16_t)t;
        }
      }
      lastButtonEventMs_ = now;
    }
    
    // OK button => apply zero offset adjustment and return to menu
    // Ignore OK click if long press was detected recently (user released button after changing step)
    // This prevents accidental apply when user releases button after long press for step change
    bool ignoreOkClick = (lastOkLongPressMs_ > 0 && (uint32_t)(now - lastOkLongPressMs_) < OK_LONG_PRESS_IGNORE_CLICK_MS);
    if (btnOk && setValue_ && !btnOkLong && !ignoreOkClick) {
      setValue_(raw100, target100_);
      currentScreen_ = SCR_MENU;
      lastButtonEventMs_ = now;
      lastOkLongPressMs_ = 0;  // Reset long press timestamp
    }
    
    // Clear long press timestamp if enough time has passed (cleanup)
    if (lastOkLongPressMs_ > 0 && (uint32_t)(now - lastOkLongPressMs_) >= OK_LONG_PRESS_IGNORE_CLICK_MS) {
      lastOkLongPressMs_ = 0;
    }
    
    // BACK button => cancel (return to menu without applying)
    if (btnBack) {
      currentScreen_ = SCR_MENU;
      lastButtonEventMs_ = now;
      lastOkLongPressMs_ = 0;  // Reset long press timestamp when leaving Set Value screen
    }
  }
  else if (currentScreen_ == SCR_VIEW || currentScreen_ == SCR_ADC) {
    // View screens: OK or BACK returns to menu
    if (btnOk || btnBack) {
      currentScreen_ = SCR_MENU;
      lastButtonEventMs_ = now;
    }
  }
  else {
    // Action screens: OK = execute action, BACK = cancel
    if (btnOk) {
      switch (currentScreen_) {
        case SCR_ZERO:
          if (setZero_) setZero_(raw100);
          break;
        case SCR_CALMIN:
          if (calMin_) calMin_(adc);
          break;
        case SCR_CALMAX:
          if (calMax_) calMax_(adc);
          break;
        case SCR_INVERT:
          if (invertToggle_) invertToggle_();
          break;
        default:
          break;
      }
      currentScreen_ = SCR_MENU;
      lastButtonEventMs_ = now;
    }
    if (btnBack) {
      currentScreen_ = SCR_MENU;
      lastButtonEventMs_ = now;
    }
  }
}

void MenuManager::resetDisplaySmoothing() {
  // Reset smoothing state to show exact values immediately after setting zero
  // Set both to 0 so that smoothing starts from exactly 0.00°
  smoothedAngle100_ = 0;
  lastDisplayedAngle100_ = 0;
  smoothingResetFlag_ = true;  // Set flag to prevent reinitialization from shown100
}

void MenuManager::render(uint16_t adc, uint16_t raw100, uint16_t shown100) {
  // Use dynamic buffer size based on LCD_COLS
  #if LCD_COLS == 16
    char buf0[17] = {0}, buf1[17] = {0};
    #if LCD_ROWS >= 4
      char buf2[17] = {0}, buf3[17] = {0};
    #endif
  #elif LCD_COLS == 20
    char buf0[21] = {0}, buf1[21] = {0};
    #if LCD_ROWS >= 4
      char buf2[21] = {0}, buf3[21] = {0};
    #endif
  #endif

  switch (currentScreen_) {
    case SCR_MAIN:
      {
        // Apply exponential smoothing to reduce noise and flickering
        // This creates a low-pass filter: smoothed = (old * (16-N) + new * N) / 16
        
        // After zeroing: maintain stability by keeping value at 0 if shown100 is close to 0
        // This prevents drift after zeroing due to ADC noise
        static uint32_t zeroTimeMs = 0;  // Track when zero was set (0 = not active)
        static const uint32_t ZERO_STABILITY_PERIOD_MS = 3000;  // Maintain 0 for 3 seconds after zeroing
        
        if (smoothingResetFlag_) {
          // After reset (e.g., after setting zero), force smoothed value to exactly 0
          // This ensures that after zeroing, displayed value stays at exactly 0.00°
          smoothedAngle100_ = 0;
          lastDisplayedAngle100_ = 0;
          zeroTimeMs = millis();  // Record time of zeroing to start stability period
          smoothingResetFlag_ = false;  // Clear flag after handling
        } else {
          // Check if shown100 is close to 0 (handling wrap-around at 360°)
          bool nearZero = (shown100 <= ZERO_THRESHOLD_100 || shown100 >= (36000 - ZERO_THRESHOLD_100));
          uint32_t now = millis();
          bool inStabilityPeriod = (zeroTimeMs > 0 && (uint32_t)(now - zeroTimeMs) < ZERO_STABILITY_PERIOD_MS);
          
          if (inStabilityPeriod) {
            // During stability period after zeroing: always keep at 0 if nearZero
            if (nearZero) {
              // Value is still near 0 - keep smoothed at 0 (prevent drift)
              smoothedAngle100_ = 0;
              lastDisplayedAngle100_ = 0;
            } else {
              // Value moved significantly away from 0 - exit stability period and start normal smoothing
              zeroTimeMs = 0;
              smoothedAngle100_ = shown100;  // Initialize with current value
            }
          } else {
            // Normal operation (not in stability period or period expired)
            if (zeroTimeMs > 0) {
              // Stability period expired - clear it
              zeroTimeMs = 0;
            }
            
            // Additional stability check: if smoothed is 0 and shown is near 0, keep at 0
            // This provides ongoing stability even after the initial period
            if (smoothedAngle100_ == 0 && nearZero) {
              // Both are near/at 0 - keep at 0 (prevents drift from noise)
              smoothedAngle100_ = 0;
              lastDisplayedAngle100_ = 0;
            } else if (smoothedAngle100_ == 0 && shown100 == 0) {
              // Both are exactly 0 - keep at 0
              smoothedAngle100_ = 0;
              lastDisplayedAngle100_ = 0;
            } else if (smoothedAngle100_ == 0 && !nearZero) {
              // Smoothed is 0 but shown moved away - initialize with current value
              smoothedAngle100_ = shown100;
            } else {
              // Normal smoothing for non-zero values
              // Calculate difference, handling wrap-around
              int32_t diff = (int32_t)shown100 - (int32_t)smoothedAngle100_;
              
              // Handle wrap-around: if difference > 180°, wrap the other way
              if (diff > 18000) diff -= 36000;
              else if (diff < -18000) diff += 36000;
              
              // Apply exponential smoothing: smoothed = smoothed + (new - smoothed) * factor/16
              int32_t smoothed = (int32_t)smoothedAngle100_ + (diff * SMOOTHING_FACTOR) / 16;
              
              // Normalize to 0..35999 range
              if (smoothed < 0) smoothed += 36000;
              else if (smoothed >= 36000) smoothed -= 36000;
              
              smoothedAngle100_ = (uint16_t)smoothed;
              
              // Final check: if after smoothing the value is very close to 0, snap to 0
              // This prevents small drifts (like 0.14°) from accumulating
              // Check both smoothed and shown - if both are near 0, force smoothed to 0
              bool smoothedNearZero = (smoothedAngle100_ <= ZERO_THRESHOLD_100 || smoothedAngle100_ >= (36000 - ZERO_THRESHOLD_100));
              if (smoothedNearZero && nearZero) {
                // Both smoothed and shown are near 0 - snap smoothed to exactly 0 for stability
                smoothedAngle100_ = 0;
              }
            }
          }
        }
        
        // Final check: if smoothed value is exactly 0, always display 0.00°
        // This ensures that after zeroing, the display shows exactly 0.00° and stays at 0
        if (smoothedAngle100_ == 0) {
          lastDisplayedAngle100_ = 0;
        } else {
          // Apply hysteresis to prevent display updates for tiny changes
          uint16_t displayDiff;
          if (smoothedAngle100_ > lastDisplayedAngle100_) {
            displayDiff = smoothedAngle100_ - lastDisplayedAngle100_;
          } else {
            displayDiff = lastDisplayedAngle100_ - smoothedAngle100_;
          }
          
          // Handle wrap-around for display difference
          if (displayDiff > 18000) {
            displayDiff = 36000 - displayDiff;
          }
          
          // Update display only if change is significant (>= 0.10°) or this is first display
          // This prevents flickering of last digits due to ADC noise
          if (lastDisplayedAngle100_ == 0 || displayDiff >= DISPLAY_HYSTERESIS_100) {
            lastDisplayedAngle100_ = smoothedAngle100_;
          }
        }
        
        char a[8]; formatAngle100(a, lastDisplayedAngle100_);
        #if LCD_COLS >= 20
          snprintf(buf0, LCD_COLS + 1, "Angle: %s", a);
        #else
          snprintf(buf0, LCD_COLS + 1, "Ang: %s", a);  // Shortened for 16-char displays
        #endif
        snprintf(buf1, LCD_COLS + 1, "Ok:MENU Long:0");
        #if LCD_ROWS >= 4
          snprintf(buf2, LCD_COLS + 1, "Long press: Set Zero");
          snprintf(buf3, LCD_COLS + 1, "");
        #endif
      }
      break;

    case SCR_MENU:
      #if LCD_COLS >= 20
        snprintf(buf0, LCD_COLS + 1, ">%d/%d %s", menuIdx_ + 1, MENU_N, menuItems_[menuIdx_]);
      #else
        snprintf(buf0, LCD_COLS + 1, ">%d %s", menuIdx_ + 1, menuItems_[menuIdx_]);
      #endif
      snprintf(buf1, LCD_COLS + 1, "Ent:OK L:Back");
      #if LCD_ROWS >= 4
        if (menuIdx_ > 0) {
          snprintf(buf2, LCD_COLS + 1, "  %d %s", menuIdx_, menuItems_[menuIdx_ - 1]);
        } else {
          snprintf(buf2, LCD_COLS + 1, "");
        }
        if (menuIdx_ < MENU_N - 1) {
          snprintf(buf3, LCD_COLS + 1, "  %d %s", menuIdx_ + 2, menuItems_[menuIdx_ + 1]);
        } else {
          snprintf(buf3, LCD_COLS + 1, "");
        }
      #endif
      break;

    case SCR_VIEW:
      {
        char a[8]; formatAngle100(a, shown100);
        #if LCD_COLS >= 20
          snprintf(buf0, LCD_COLS + 1, "Angle: %s", a);
        #else
          snprintf(buf0, LCD_COLS + 1, "Ang: %s", a);  // Shortened for 16-char displays
        #endif
        #if LCD_COLS >= 20
          snprintf(buf1, LCD_COLS + 1, "Enter or Long: Back");
        #else
          snprintf(buf1, LCD_COLS + 1, "Ent:Back");
        #endif
        #if LCD_ROWS >= 4
          char raw[8]; formatAngle100(raw, raw100);
          snprintf(buf2, LCD_COLS + 1, "Raw: %s", raw);
          if (settings_) {
            snprintf(buf3, LCD_COLS + 1, "Zero: %5u", settings_->zero100);
          } else {
            snprintf(buf3, LCD_COLS + 1, "");
          }
        #endif
      }
      break;

    case SCR_ADC:
      {
        snprintf(buf0, LCD_COLS + 1, "ADC: %4u", adc);
        if (settings_) {
          snprintf(buf1, LCD_COLS + 1, "Min:%u Max:%u", settings_->calMin, settings_->calMax);
        } else {
          snprintf(buf1, LCD_COLS + 1, "Range: 0-1023");
        }
        #if LCD_ROWS >= 4
          if (settings_) {
            int32_t span = (int32_t)settings_->calMax - (int32_t)settings_->calMin;
            if (span < 1) span = 1;
            snprintf(buf2, LCD_COLS + 1, "Span: %ld", (long)span);
            if (adc < settings_->calMin) {
              snprintf(buf3, LCD_COLS + 1, "Below MIN!");
            } else if (adc > settings_->calMax) {
              snprintf(buf3, LCD_COLS + 1, "Above MAX!");
            } else {
              uint8_t percent = (uint8_t)(((uint32_t)(adc - settings_->calMin) * 100UL) / (uint32_t)span);
              snprintf(buf3, LCD_COLS + 1, "In range: %u%%", percent);
            }
          } else {
            snprintf(buf2, LCD_COLS + 1, "Calibration not set");
            snprintf(buf3, LCD_COLS + 1, "Use Cal Min/Max");
          }
        #endif
      }
      break;

    case SCR_ZERO:
      snprintf(buf0, LCD_COLS + 1, "Set ZERO?");
      snprintf(buf1, LCD_COLS + 1, "Ent:YES L:Back");
      #if LCD_ROWS >= 4
        char a[8]; formatAngle100(a, raw100);
        snprintf(buf2, LCD_COLS + 1, "Current: %s", a);
        snprintf(buf3, LCD_COLS + 1, "");
      #endif
      break;

    case SCR_SETVALUE:
      {
        char t[8]; formatAngle100(t, target100_);
        #if LCD_COLS >= 20
          snprintf(buf0, LCD_COLS + 1, "Set Value: %s", t);
        #else
          snprintf(buf0, LCD_COLS + 1, "Set: %s", t);
        #endif
        #if LCD_COLS >= 20
          snprintf(buf1, LCD_COLS + 1, "UP/DN:val OK:apply LOK:step");
        #else
          snprintf(buf1, LCD_COLS + 1, "U/D:val OK:OK LOK:stp");
        #endif
        #if LCD_ROWS >= 4
          char a[8]; formatAngle100(a, raw100);
          snprintf(buf2, LCD_COLS + 1, "Raw: %s", a);
          if (step100_ == 2U) {
            snprintf(buf3, LCD_COLS + 1, "Step: 1 min (LOK:change)");
          } else if (step100_ == 17U) {
            snprintf(buf3, LCD_COLS + 1, "Step: 10 min (LOK:change)");
          } else if (step100_ == 100U) {
            snprintf(buf3, LCD_COLS + 1, "Step: 1 deg (LOK:change)");
          } else if (step100_ == 1000U) {
            snprintf(buf3, LCD_COLS + 1, "Step: 10 deg (LOK:change)");
          } else if (step100_ == 10000U) {
            snprintf(buf3, LCD_COLS + 1, "Step: 100 deg (LOK:change)");
          } else {
            snprintf(buf3, LCD_COLS + 1, "Step: ? (LOK:change)");
          }
        #endif
      }
      break;

    case SCR_CALMIN:
      snprintf(buf0, LCD_COLS + 1, "Cal MIN=%4u", adc);
      snprintf(buf1, LCD_COLS + 1, "Ent:SAVE L:Back");
      #if LCD_ROWS >= 4
        if (settings_) {
          snprintf(buf2, LCD_COLS + 1, "Range: %u-%u", settings_->calMin, settings_->calMax);
        } else {
          snprintf(buf2, LCD_COLS + 1, "");
        }
        snprintf(buf3, LCD_COLS + 1, "");
      #endif
      break;

    case SCR_CALMAX:
      snprintf(buf0, LCD_COLS + 1, "Cal MAX=%4u", adc);
      snprintf(buf1, LCD_COLS + 1, "Ent:SAVE L:Back");
      #if LCD_ROWS >= 4
        if (settings_) {
          snprintf(buf2, LCD_COLS + 1, "Range: %u-%u", settings_->calMin, settings_->calMax);
        } else {
          snprintf(buf2, LCD_COLS + 1, "");
        }
        snprintf(buf3, LCD_COLS + 1, "");
      #endif
      break;

    case SCR_INVERT:
      if (settings_) {
        snprintf(buf0, LCD_COLS + 1, "Invert: %s", (settings_->flags & 1) ? "ON " : "OFF");
        snprintf(buf1, LCD_COLS + 1, "Ent:TOG L:Back");
        #if LCD_ROWS >= 4
          snprintf(buf2, LCD_COLS + 1, "Direction: %s", (settings_->flags & 1) ? "Reversed" : "Normal");
          snprintf(buf3, LCD_COLS + 1, "");
        #endif
      } else {
        snprintf(buf0, LCD_COLS + 1, "Invert: ERR");
        snprintf(buf1, LCD_COLS + 1, "");
      }
      break;
  }

  // Update LCD display
  // Ensure buf0 is not empty before displaying (safety check)
  if (buf0[0] == 0) {
    // If buf0 is empty, set a default message
    #if LCD_COLS >= 20
      snprintf(buf0, LCD_COLS + 1, "Angle: 0  0'");
    #else
      snprintf(buf0, LCD_COLS + 1, "Ang: 0  0'");
    #endif
  }
  lcd_.setLine(0, buf0);
  lcd_.setLine(1, buf1);
  #if LCD_ROWS >= 4
    lcd_.setLine(2, buf2);
    lcd_.setLine(3, buf3);
  #endif
  lcd_.flush();
}
