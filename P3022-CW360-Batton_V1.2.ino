/*
  Arduino Uno/Nano/Micro Compatible
  P3022 Angle Sensor with LCD Display and Button Menu Navigation
  Version: Button_V1.1 (Button-based menu control)
  
  Compatible boards:
  - Arduino Uno (ATmega328P) - Full support
  - Arduino Nano (ATmega328P) - Full support  
  - Arduino Micro (ATmega32U4) - Full support
  
         Hardware:
         - LCD Display (1602 or 2004) via I2C or 4-bit parallel
         - 4 Buttons for menu navigation (UP, DOWN, OK, BACK)
         - P3022-V1-CW360 analog angle sensor
         
         Features:
         - Reads P3022 analog output on A0 (ADC averaged for stability)
         - Calibration MIN/MAX (stores to EEPROM with CRC validation)
         - Zero offset (Set Zero) stores to EEPROM
         - Set Value: set displayed angle to arbitrary target (e.g. 70.42°) by adjusting zero offset
         - Invert direction (stores to EEPROM)
         - Menu controlled by buttons:
             UP: navigate up / increase value in Set Value
             DOWN: navigate down / decrease value in Set Value
             OK: select / confirm / apply
             BACK: cancel / back to previous screen / quick Set Zero (long press on main screen)
         - LCD redraw without frequent lcd.clear() to reduce flicker
         - Optimized polling rates for stable operation

  Libraries required:
  - For I2C: LiquidCrystal_I2C (by Frank de Brabander or similar)
  - For 4-bit: LiquidCrystal (built-in Arduino library)
  - EEPROM (built-in Arduino library)
  - Wire (built-in Arduino library, only for I2C mode)

  LCD Configuration (set below):
  - LCD_TYPE: 1602 (16x2) or 2004 (20x4)
  - LCD_INTERFACE: I2C or PARALLEL_4BIT
  
  Wiring I2C:
    - Uno/Nano: SDA=A4, SCL=A5
    - Micro: SDA=D2, SCL=D3 (handled automatically by Wire library)
    - VCC=5V, GND=GND
    - I2C address: 0x27 or 0x3F (set LCD_I2C_ADDR below)
  
  Wiring 4-bit Parallel (only if LCD_INTERFACE == PARALLEL_4BIT):
    - RS=12, Enable=11, D4=7, D5=6, D6=5, D7=8
    - RW=GND (read/write always low)
    - VCC=5V, GND=GND, V0=potentiometer (contrast)
    - Can customize pins by changing PIN_LCD_* below
  
  P3022 Sensor:
    - OUT=A0 (analog input)
    - VCC=5V, GND=GND
  
  Buttons for menu navigation:
    - UP=D2, DOWN=D3, OK=D4, BACK=D9 (all buttons to GND)
    - Note: D5 is used by LCD (PIN_LCD_D6), so BACK is on D9
    - Internal INPUT_PULLUP enabled (no external resistors needed)
    - Long press on BACK (on main screen) = quick Set Zero

  Notes:
  - Code automatically adapts to board type (detected at compile time)
  - All settings are stored in EEPROM with CRC protection
  - Works with 16MHz Uno/Nano and 16MHz Micro (both 5V)
*/

// Include configuration first (defines LCD_TYPE, LCD_INTERFACE, etc.)
#include "Config.h"

// Include LCD library based on interface type (must be before LCDDisplay.h)
#if defined(LCD_INTERFACE_I2C)
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
  // Global LCD object (will be used by LCDDisplay class)
  LiquidCrystal_I2C lcd(LCD_I2C_ADDR, LCD_COLS, LCD_ROWS);
#elif defined(LCD_INTERFACE_PARALLEL_4BIT)
  #include <LiquidCrystal.h>
  // Global LCD object (will be used by LCDDisplay class)
  LiquidCrystal lcd(PIN_LCD_RS, PIN_LCD_EN, PIN_LCD_D4, PIN_LCD_D5, PIN_LCD_D6, PIN_LCD_D7);
#endif

// Include all module headers (order matters for dependencies)
#include "Settings.h"
#include "Sensor.h"
#include "Button.h"
#include "LCDDisplay.h"  // Requires lcd object defined above
#include "Utils.h"
#include "MenuManager.h"  // Requires LCDDisplay and Utils
#include <string.h>  // For memcpy in LCDDisplay

// ---------------- Global Instances ----------------
// Global button instances
Button btnUp(PIN_BTN_UP);
Button btnDown(PIN_BTN_DOWN);
Button btnOk(PIN_BTN_OK);
Button btnBack(PIN_BTN_BACK);

// Global LCD display instance (references global lcd object)
LCDDisplay lcdDisplay(lcd);

// Global menu manager instance (will be initialized in setup)
MenuManager* menuManager = nullptr;

// Callback functions for menu actions (wrappers for settings functions)
void menuSetZero(uint16_t raw100) {
  doSetZero(raw100);
}

void menuSetValue(uint16_t raw100, uint16_t target100) {
  doSetValue(raw100, target100);
}

void menuCalMin(uint16_t adc) {
  doCalMin(adc);
}

void menuCalMax(uint16_t adc) {
  doCalMax(adc);
}

void menuInvertToggle() {
  doInvertToggle();
}

       // ---------------- Timing Variables ----------------
       // Timing constants are defined in Config.h
       uint32_t lastButtonTick = 0;
       uint32_t lastUiTick  = 0;

void setup() {
  // Configure ADC reference
  // DEFAULT = AVcc (5V for Uno/Nano/Micro)
  // For 3.3V boards, use INTERNAL or EXTERNAL
  analogReference(DEFAULT);

  // Load settings from EEPROM (or defaults if first run)
  loadSettings();

  // Initialize buttons (configure pins and read initial state)
  btnUp.begin();
  btnDown.begin();
  btnOk.begin();
  btnBack.begin();

  // Initialize LCD display and show startup message
  lcdDisplay.showStartup();

  // Initialize menu manager
  static MenuManager menu(lcdDisplay, menuSetZero, menuSetValue, 
                          menuCalMin, menuCalMax, menuInvertToggle, &S);
  menuManager = &menu;
}

void loop() {
  uint32_t now = millis();

  // Button processing (debouncing and long press detection)
  if ((uint32_t)(now - lastButtonTick) >= BUTTON_TICK_MS) {
    lastButtonTick = now;
    btnUp.update();
    btnDown.update();
    btnOk.update();
    btnBack.update();
  }

  // UI tick (10ms = 100Hz update rate for smooth display)
  if ((uint32_t)(now - lastUiTick) >= UI_TICK_MS) {
    lastUiTick = now;

    uint16_t adc    = readAdcAvg16();           // Read averaged ADC value (0..1023)
    uint16_t raw100 = adcToAngle100(adc);       // Convert to angle (0..35999, calibrated, invert applied, no zero offset)
    uint16_t shown  = applyZero100(raw100);     // Apply zero offset to get displayed angle

    // Update menu with button events and sensor data
    if (menuManager) {
      // IMPORTANT: Check long press BEFORE wasPressed() because wasLongPressed() can only be called once per event
      bool btnOkLong = btnOk.wasLongPressed();  // Long press OK for step size change in Set Value
      bool btnBackLong = btnBack.wasLongPressed();  // Long press for quick set zero on main screen
      
      // Alternative method for OK long press detection (for step size change in Set Value)
      // Change step size on RELEASE after long press, not during hold (more intuitive for user)
      static uint32_t btnOkPressStart = 0;
      static bool btnOkWasLongHeld = false;  // Flag: button was held >600ms during this press cycle
      static bool btnOkPendingStepChange = false;  // Flag: step change should happen on release
      static bool btnOkPreviousPinState = false;  // Track previous pin state to detect release transition
      static bool btnOkStepChangeFired = false;  // Flag: step change was already fired for this press cycle (prevent double-trigger)
      
      bool btnOkPinPressed = !digitalRead(PIN_BTN_OK);
      bool btnOkJustReleased = (btnOkPreviousPinState && !btnOkPinPressed);  // Detect release transition (true only once per release)
      
      if (btnOkPinPressed && btnOkPressStart == 0) {
        // Button just pressed - start timer and reset all flags
        btnOkPressStart = now;
        btnOkWasLongHeld = false;
        btnOkPendingStepChange = false;
        btnOkStepChangeFired = false;  // Reset flag for new press cycle
      } else if (btnOkPinPressed && btnOkPressStart > 0) {
        // Button still pressed - check if held long enough (>600ms)
        if (!btnOkWasLongHeld && (uint32_t)(now - btnOkPressStart) >= 600) {
          btnOkWasLongHeld = true;
          // Mark that step change should happen on RELEASE (only if in Set Value screen)
          if (menuManager && menuManager->getCurrentScreen() == MenuManager::SCR_SETVALUE) {
            btnOkPendingStepChange = true;
          }
        }
      } else if (!btnOkPinPressed && btnOkPressStart > 0) {
        // Button released - reset timer for next press cycle
        btnOkPressStart = 0;
      }
      
      // Update previous pin state for next cycle (for release detection)
      btnOkPreviousPinState = btnOkPinPressed;
      
      // For Set Value screen: use ONLY manual detection (on release) to prevent double-trigger
      // Button class wasLongPressed() fires during hold, manual timer fires on release
      // Using both causes double step change (skipping one step size)
      // Solution: ignore Button class wasLongPressed() for Set Value screen, use only manual timer
      // IMPORTANT: btnOkStepChangeFired prevents double-trigger - step change fires only once per release
      bool btnOkLongOnRelease = (btnOkJustReleased && btnOkPendingStepChange && btnOkWasLongHeld && 
                                 !btnOkStepChangeFired &&
                                 menuManager && menuManager->getCurrentScreen() == MenuManager::SCR_SETVALUE);
      
      // Mark that step change was fired (prevents double-trigger in same or next cycle)
      if (btnOkLongOnRelease) {
        btnOkStepChangeFired = true;
        // Clear flags after firing to prevent re-triggering
        btnOkPendingStepChange = false;
        btnOkWasLongHeld = false;
      }
      
      // Combine methods: for Set Value screen use only manual timer, for other screens use Button class
      bool btnOkLongCombined;
      if (menuManager && menuManager->getCurrentScreen() == MenuManager::SCR_SETVALUE) {
        // Set Value: use ONLY manual timer (on release) to prevent double-trigger and give user control
        btnOkLongCombined = btnOkLongOnRelease;
      } else {
        // Other screens: use Button class wasLongPressed() (normal behavior)
        btnOkLongCombined = btnOkLong;
      }
      
      // Quick set zero from main screen (long press BACK) - handle IMMEDIATELY
      // Track button hold time manually using direct pin read (more reliable than Button class)
      static uint32_t btnBackPressStart = 0;
      static bool btnBackLongProcessed = false;  // Prevent multiple zero operations per press
      
      // Read pin directly to detect physical press state (LOW = pressed with pullup)
      // This bypasses Button class state machine which might not work correctly
      bool btnBackPinPressed = !digitalRead(PIN_BTN_BACK);
      
      if (btnBackPinPressed && btnBackPressStart == 0) {
        // Button just pressed - start timer
        btnBackPressStart = now;
        btnBackLongProcessed = false;
      } else if (!btnBackPinPressed) {
        // Button released - reset timer and flag
        btnBackPressStart = 0;
        btnBackLongProcessed = false;
      }
      
      // Check if button has been held for long time (>600ms)
      bool btnBackLongHeld = (btnBackPressStart > 0 && (uint32_t)(now - btnBackPressStart) >= 600);
      
      // Check if we're on main screen and long press was detected (either wasLongPressed() or manual timer)
      if (menuManager->getCurrentScreen() == MenuManager::SCR_MAIN && 
          (btnBackLong || (btnBackLongHeld && !btnBackLongProcessed))) {
        // To make current displayed value (shown) become exactly 0.00°:
        // shown = raw100 - zero100, so zero100 = raw100 - shown
        // If we want shown = 0, we need: zero100 = raw100 - 0 = raw100
        // So we set zero100 to current raw100 value
        menuSetZero(raw100);  // Set zero offset to current raw angle (makes displayed angle = 0.00°)
        
        // Recalculate shown value after setting zero (should be exactly 0)
        shown = applyZero100(raw100);  // Recalculate: shown = raw100 - zero100 = raw100 - raw100 = 0
        
        // Reset display smoothing IMMEDIATELY to show exact 0.00° and prevent drift
        // This must be done BEFORE update() call to prevent smoothing from "recovering" old value
        menuManager->resetDisplaySmoothing();
        
        // Mark as processed to prevent multiple zero operations during same press
        if (btnBackLongHeld) {
          btnBackLongProcessed = true;
        }
      }
      
      // Get button events (automatically reset after reading)
      bool btnUpEvent = btnUp.wasPressed();
      bool btnDownEvent = btnDown.wasPressed();
      bool btnOkEvent = btnOk.wasPressed();
      bool btnBackEvent = btnBack.wasPressed();
      
      // If BACK long press was used for zeroing, don't trigger back action
      if ((btnBackLong || btnBackLongHeld) && menuManager->getCurrentScreen() == MenuManager::SCR_MAIN) {
        btnBackEvent = false;  // Prevent back action when using long press for zeroing
      }
      
      // If OK long press is detected (for step size change in Set Value), don't trigger OK action
      // This prevents accidental apply when user releases button after long press for step change
      if (btnOkLongCombined && menuManager->getCurrentScreen() == MenuManager::SCR_SETVALUE) {
        btnOkEvent = false;  // Prevent OK action when using long press for step size change
      }
      
      // Update menu with button events
      menuManager->update(adc, raw100, shown, btnUpEvent, btnDownEvent, btnOkEvent, btnBackEvent, btnOkLongCombined);
    }
  }
}
