#include <tr246.h>

/* ================================================================================
   tr246::init
   Initialise all on-board peripherals.  Call once from Arduino setup().

   Initialisation sequence:
     1. GPIO    — discrete RGB LED outputs, button inputs, power-switch output
     2. I2C     — start the bus
     3. PCINT   — enable pin-change interrupts for buttons on D4–D6 (PORTD)
     4. WS2812  — register strip with FastLED, set brightness, clear all pixels
     5. OLED    — configure display, show build timestamp for 800 ms
     6. ENS21x  — start continuous T/RH measurement
     7. ADC     — switch reference to external 3.3 V at AREF pin
     8. RHT1    — store VDD for ADC scaling
     9. EEPROM  — restore persisted user settings
   ================================================================================*/
void tr246::init() {

    // ── 1. GPIO ───────────────────────────────────────────────────────────────────

    // Discrete RGB LED channels — common-anode, active-low
    pinMode(PIN_LED_GREEN, OUTPUT);
    pinMode(PIN_LED_RED,   OUTPUT);
    pinMode(PIN_LED_BLUE,  OUTPUT);

    // Tactile buttons — active-low with internal pull-up resistors
    pinMode(PIN_BUTTON1, INPUT_PULLUP);
    pinMode(PIN_BUTTON2, INPUT_PULLUP);
    pinMode(PIN_BUTTON3, INPUT_PULLUP);

    // Power-switch output — drive HIGH to keep the board powered
    pinMode(PIN_PWR_SW, OUTPUT);
    powerSwitch(true);

    // ── 2. I2C ────────────────────────────────────────────────────────────────────
    _wire.begin();
    LED(OFF);  // Ensure the discrete RGB LED is off at startup

    // ── 3. Pin-change interrupts for buttons (ATmega328P) ─────────────────────────
    // PCIE2  — enables PCINT on PORTD
    // PCMSK2 — selects PD4 (D4), PD5 (D5), PD6 (D6) → PCINT20/21/22
    PCICR  |= (1 << PCIE2);
    PCMSK2  = (1 << PCINT20) | (1 << PCINT21) | (1 << PCINT22);

    // ── 4. WS2812 addressable LED strip ───────────────────────────────────────────
    FastLED.addLeds<WS2812, PIN_DIN, GRB>(_leds, NUM_LEDS);
    FastLED.setBrightness(50);  // Global brightness limit (0–255)
    WS_clear();                 // Ensure all pixels are off at startup

    // ── 5. OLED display ───────────────────────────────────────────────────────────
    display.begin();
    display.setI2CAddress(0x3C << 1);       // SSD1306 default address, shifted for U8x8
    display.setFlipMode(0);                 // Normal (non-flipped) orientation
    display.setFont(u8x8_font_8x13_1x2_r); // 8×13 px font, 1×2 tile characters

    // Show compile-time build timestamp on startup
    display.setCursor(1, 0);
    display.print(__TIME__);  // e.g. "12:34:56"
    display.setCursor(1, 2);
    display.print(__DATE__);  // e.g. "Apr 20 2026"
    delay(800);
    display.clear();

    display.setCursor(0, 0);
    display.print("Arduino tr246");

    // ── 6. On-board ENS21x digital T/RH sensor ────────────────────────────────────
    ENS21x.begin(&_wire);  // Bind the shared I2C bus to the nested driver
    ENS21x.init();         // Start continuous temperature and humidity measurement

    // ── 7. ADC reference ──────────────────────────────────────────────────────────
    // Use the external voltage at the AREF pin (3.3 V) as the ADC reference.
    // Must be configured before the first analogRead() call on the RHT1 sensor.
    analogReference(EXTERNAL);

    // ── 8. Analogue RHT1 T/RH sensor ─────────────────────────────────────────────
    RHT1.init();  // Default VDD = 3.3 V — matches the external ADC reference

    // ── 9. EEPROM ─────────────────────────────────────────────────────────────────
    loadParameter();  // Restore measurement pause and other persisted settings
}


/* ================================================================================
   tr246::LED
   Set the discrete common-anode RGB LED to a colour preset.

   The colourLED enum encodes channels as a 3-bit bitmask:
     bit 2 = GREEN | bit 1 = RED | bit 0 = BLUE
   Writing LOW drives the corresponding channel on (active-low logic).

   @param LED_colour  Colour preset from the colourLED enum.
   ================================================================================*/
void tr246::LED(colourLED LED_colour) {
    digitalWrite(PIN_LED_GREEN, 1 - ((LED_colour & GREEN) >> 2));
    digitalWrite(PIN_LED_RED,   1 - ((LED_colour & RED)   >> 1));
    digitalWrite(PIN_LED_BLUE,  1 - ((LED_colour & BLUE)  >> 0));
}


/* ================================================================================
   tr246::flashLED
   Turn the discrete RGB LED on for a fixed duration, then switch it off.
   Useful for short visual acknowledgements (e.g. measurement taken).

   @param LED_colour  Colour to display during the flash.
   @param _duration   On-time in milliseconds.
   ================================================================================*/
void tr246::flashLED(colourLED LED_colour, uint16_t _duration) {
    LED(LED_colour);
    delay(_duration);
    LED(OFF);
}


/* ================================================================================
   WS2812 Addressable LED Strip
   ---------------------------------------------------------------------------------
   The strip carries NUM_LEDS WS2812 pixels managed by FastLED.
   FastLED.show() transfers the pixel buffer (_leds[]) to the hardware.

   Usage:
     board.WS_LED(255, 0,   0,   0);  // Red   on LED 0
     board.WS_LED(0,   255, 0,   2);  // Green on LED 2
     board.WS_clear();                // All LEDs off
   ================================================================================*/

/* ────────────────────────────────────────────────────────────────────────────────
   tr246::WS_LED
   Set a single WS2812 LED to the specified RGB colour and commit the update.

   @param r      Red   channel (0–255)
   @param g      Green channel (0–255)
   @param b      Blue  channel (0–255)
   @param index  LED index in the strip (0 … NUM_LEDS-1).
                 Out-of-range values are silently ignored to prevent buffer overruns.
   ──────────────────────────────────────────────────────────────────────────────*/
void tr246::WS_LED(uint8_t r, uint8_t g, uint8_t b, uint8_t index) {
    if (index >= NUM_LEDS) return;  // Guard: ignore invalid index
    _leds[index] = CRGB(r, g, b);
    FastLED.show();
}


/* ────────────────────────────────────────────────────────────────────────────────
   tr246::WS_clear
   Fill the pixel buffer with black and push it — turns off all WS2812 LEDs.
   ──────────────────────────────────────────────────────────────────────────────*/
void tr246::WS_clear() {
    fill_solid(_leds, NUM_LEDS, CRGB::Black);
    FastLED.show();
}


/* ================================================================================
   tr246::powerSwitch (getter)
   Read the current state of the power-switch output pin.

   @return true if the power switch is currently driven HIGH (on), false otherwise.
   ================================================================================*/
bool tr246::powerSwitch() {
    _powerSwitch = digitalRead(PIN_PWR_SW);
    return _powerSwitch;
}


/* ================================================================================
   tr246::powerSwitch (setter)
   Drive the power-switch output pin to the requested state and cache the value.

   @param state  true = switch on (HIGH), false = switch off (LOW).
   @return The state that was applied.
   ================================================================================*/
bool tr246::powerSwitch(bool state) {
    _powerSwitch = state;
    digitalWrite(PIN_PWR_SW, _powerSwitch);
    return _powerSwitch;
}


/* ================================================================================
   ENS_t — On-board ENS21x Digital Temperature / Humidity Sensor
   ---------------------------------------------------------------------------------
   Communication: I2C at address ADDR_ENS21x (0x42).

   Register map:
     0x21  SENS_RUN — enable/disable continuous measurement (bit1 = T, bit0 = RH)
     0x30  T_VAL   — 16-bit raw temperature result (little-endian)
     0x33  H_VAL   — 16-bit raw humidity result    (little-endian)

   Conversion formulas (ENS21x datasheet §6.4):
     Temperature [°C]  = raw / 64 − 273.15
     Humidity    [%RH] = raw / 512
   ================================================================================*/

/* ────────────────────────────────────────────────────────────────────────────────
   ENS_t::init
   Write the SENS_RUN register to start continuous measurement for both
   temperature and humidity channels.  Called internally by tr246::init().
   ──────────────────────────────────────────────────────────────────────────────*/
void tr246::ENS_t::init() {
    _wire->beginTransmission(ADDR_ENS21x);
    _wire->write(0x21);  // Register: SENS_RUN
    _wire->write(0x03);  // bit1 = T enable, bit0 = RH enable
    _wire->write(0x03);  // Start measurement on both channels
    _wire->endTransmission();
}


/* ────────────────────────────────────────────────────────────────────────────────
   ENS_t::temperature
   Read the raw 16-bit value from register T_VAL and convert to degrees Celsius.

   @return Temperature in °C, or NAN if the sensor does not respond.
   ──────────────────────────────────────────────────────────────────────────────*/
float tr246::ENS_t::temperature() {
    uint8_t data[2];

    _wire->beginTransmission(ADDR_ENS21x);
    _wire->write(0x30);             // Point to T_VAL register
    _wire->endTransmission(false);  // Repeated-start: keep bus active for the read

    _wire->requestFrom(ADDR_ENS21x, 2);
    if (_wire->available() == 2) {
        data[0] = _wire->read();  // Low byte
        data[1] = _wire->read();  // High byte
        _wire->endTransmission();

        // Reassemble little-endian uint16 and apply datasheet conversion
        return (float)((data[1] << 8) | data[0]) / 64.0f - 273.15f;
    }

    return NAN;  // Sensor not responding
}


/* ────────────────────────────────────────────────────────────────────────────────
   ENS_t::humidity
   Read the raw 16-bit value from register H_VAL and convert to %RH.

   @return Relative humidity in %RH, or NAN if the sensor does not respond.
   ──────────────────────────────────────────────────────────────────────────────*/
float tr246::ENS_t::humidity() {
    uint8_t data[2];

    _wire->beginTransmission(ADDR_ENS21x);
    _wire->write(0x33);             // Point to H_VAL register
    _wire->endTransmission(false);  // Repeated-start: keep bus active for the read

    _wire->requestFrom(ADDR_ENS21x, 2);
    if (_wire->available() == 2) {
        data[0] = _wire->read();  // Low byte
        data[1] = _wire->read();  // High byte
        _wire->endTransmission();

        // Cast to uint16_t before shifting to ensure unsigned arithmetic
        uint16_t raw = (uint16_t)((data[1] << 8) | data[0]);
        return raw / 512.0f;
    }

    return NAN;  // Sensor not responding
}


/* ================================================================================
   RHT1_t — Analogue Capacitive Temperature / Humidity Sensor
   ---------------------------------------------------------------------------------
   The RHT1 sensor produces analogue voltages proportional to temperature and
   humidity.  Both channels are read with analogRead() against the external
   3.3 V reference configured in tr246::init().

   Transfer functions (derived from the sensor datasheet):
     Humidity    [%RH] = −19.7 / 0.54  +  (100 / 0.54) × (ADC / 1023)
                       ≈ −36.48  +  185.19 × (ADC / 1023)
     Temperature [°C]  = −66.875 + 218.75 × (ADC / 1023)

   The VDD terms cancel in the ratiometric ADC measurement
   (ADC × VDD / 1023 / VDD = ADC / 1023), so the formulas are independent
   of supply voltage and _VDD is not used in the conversion.
   ================================================================================*/

/* ────────────────────────────────────────────────────────────────────────────────
   RHT1_t::init
   Store the sensor supply voltage for reference.  Called by tr246::init().

   @param VDD  Supply voltage in volts (default 3.3 V).
   ──────────────────────────────────────────────────────────────────────────────*/
void tr246::RHT1_t::init(float VDD) {
    _VDD = VDD;
}


/* ────────────────────────────────────────────────────────────────────────────────
   RHT1_t::humidity
   Read the analogue humidity output and convert it to %RH.

   @return Relative humidity in %RH.
   ──────────────────────────────────────────────────────────────────────────────*/
float tr246::RHT1_t::humidity() {
    // Ratiometric ADC: VDD terms cancel, result depends only on ADC / 1023
    return (-19.7f / 0.54f) + (100.0f / 0.54f) * (analogRead(PIN_RHT1_RH) / 1023.0f);
}


/* ────────────────────────────────────────────────────────────────────────────────
   RHT1_t::temperature
   Read the analogue temperature output and convert it to degrees Celsius.

   @return Temperature in °C.
   ──────────────────────────────────────────────────────────────────────────────*/
float tr246::RHT1_t::temperature() {
    // Ratiometric ADC: VDD terms cancel, result depends only on ADC / 1023
    return -66.875f + 218.75f * (analogRead(PIN_RHT1_T) / 1023.0f);
}


/* ================================================================================
   Button Methods
   ---------------------------------------------------------------------------------
   All three buttons are wired active-low with internal pull-up resistors:
     digitalRead() = 1 → button released
     digitalRead() = 0 → button pressed

   readAllButton() inverts this polarity and packs the three states into a bitmask:
     bit 0 = buttonA | bit 1 = buttonB | bit 2 = buttonC
   ================================================================================*/

/* ────────────────────────────────────────────────────────────────────────────────
   tr246::readAllButton
   Sample all three buttons simultaneously.

   @return Bitmask of currently pressed buttons (set bit = pressed).
   ──────────────────────────────────────────────────────────────────────────────*/
uint8_t tr246::readAllButton() {
    // digitalRead returns 1 (released) or 0 (pressed).
    // Multiply by the bitmask weight, sum, then subtract from 7 to invert polarity.
    uint8_t state = digitalRead(PIN_BUTTON1) * buttonA
                  + digitalRead(PIN_BUTTON2) * buttonB
                  + digitalRead(PIN_BUTTON3) * buttonC;
    return 7 - state;
}


/* ────────────────────────────────────────────────────────────────────────────────
   tr246::readButton
   Check whether a specific button is currently pressed.

   @param myButton  Button to query (buttonA, buttonB, or buttonC).
   @return 1 if pressed, 0 if released.
   ──────────────────────────────────────────────────────────────────────────────*/
uint8_t tr246::readButton(numberButton myButton) {
    return (readAllButton() & myButton) ? 1 : 0;
}


/* ────────────────────────────────────────────────────────────────────────────────
   tr246::waitButtonPress
   Block until the specified button is pressed.

   @param myButton  Button to wait for.
   ──────────────────────────────────────────────────────────────────────────────*/
void tr246::waitButtonPress(numberButton myButton) {
    while (readButton(myButton) == 0) {}
}


/* ────────────────────────────────────────────────────────────────────────────────
   tr246::waitButtonPressAndRelease
   Block until a complete press-and-release cycle is detected.
   A 50 ms pause between press and release provides additional debounce margin.

   @param myButton  Button to wait for.
   ──────────────────────────────────────────────────────────────────────────────*/
void tr246::waitButtonPressAndRelease(numberButton myButton) {
    while (readButton(myButton) == 0) {}  // Wait for press
    delay(50);                            // Debounce pause
    while (readButton(myButton) == 1) {}  // Wait for release
}


/* ================================================================================
   tr246::handleButtonInterrupt
   Update the three buttonX_Status structs from within the pin-change ISR.

   For each button, compare the current hardware level against the last recorded
   state.  If they differ and the debounce window (debounceTime ms) has elapsed
   since the last accepted edge, accept the transition and update the struct.
   If the levels match, clear the 'changed' flag so the main loop sees it for
   exactly one ISR pass after each valid transition.

 
   Hardware: buttons on D4–D6 (PORTD); PCINT enabled via PCIE2 / PCMSK2.
   ================================================================================*/
void tr246::handleButtonInterrupt() {

    // ── Button A (D4 / PD4 / PCINT20) ────────────────────────────────────────────
    if (buttonA_Status.pressed != digitalRead(PIN_BUTTON1)) {
        if ((millis() - buttonA_Status.pressTime) > debounceTime) {
            buttonA_Status.changed   = true;
            buttonA_Status.pressTime = millis();
            buttonA_Status.pressed   = digitalRead(PIN_BUTTON1);
        }
    } else {
        buttonA_Status.changed = false;
    }

    // ── Button B (D5 / PD5 / PCINT21) ────────────────────────────────────────────
    if (buttonB_Status.pressed != digitalRead(PIN_BUTTON2)) {
        if ((millis() - buttonB_Status.pressTime) > debounceTime) {
            buttonB_Status.changed   = true;
            buttonB_Status.pressTime = millis();
            buttonB_Status.pressed   = digitalRead(PIN_BUTTON2);
        }
    } else {
        buttonB_Status.changed = false;
    }

    // ── Button C (D6 / PD6 / PCINT22) ────────────────────────────────────────────
    if (buttonC_Status.pressed != digitalRead(PIN_BUTTON3)) {
        if ((millis() - buttonC_Status.pressTime) > debounceTime) {
            buttonC_Status.changed   = true;
            buttonC_Status.pressTime = millis();
            buttonC_Status.pressed   = digitalRead(PIN_BUTTON3);
        }
    } else {
        buttonC_Status.changed = false;
    }
}


/* ================================================================================
   Measurement Pause — EEPROM-backed interval setting
   ---------------------------------------------------------------------------------
   The measurement interval is stored as a 16-bit little-endian value in two
   consecutive EEPROM bytes starting at EEPROM_ADDRESS_PAUSE.
   ================================================================================*/

/* ────────────────────────────────────────────────────────────────────────────────
   tr246::MeasurementPause (getter)
   @return Current measurement interval in milliseconds.
   ──────────────────────────────────────────────────────────────────────────────*/
uint16_t tr246::MeasurementPause() {
    return _measurementPause;
}


/* ────────────────────────────────────────────────────────────────────────────────
   tr246::MeasurementPause (setter)
   Set the measurement interval and persist it to EEPROM (16-bit, little-endian).

   @param _pause  Interval in milliseconds.
   @return The value that was stored.
   ──────────────────────────────────────────────────────────────────────────────*/
uint16_t tr246::MeasurementPause(uint16_t _pause) {
    _measurementPause = _pause;
    EEPROM.write(EEPROM_ADDRESS_PAUSE,     lowByte(_measurementPause));
    EEPROM.write(EEPROM_ADDRESS_PAUSE + 1, highByte(_measurementPause));
    return _measurementPause;
}


/* ================================================================================
   tr246::loadParameter
   Load user settings from EEPROM into RAM.  Called once by init().

   Sanity check: an uninitialised EEPROM (fresh chip, all 0xFF bytes) would yield
   65535 ms, which is impractically long.  Values above 50000 ms are reset to a
   safe default of 1000 ms.
   ================================================================================*/
void tr246::loadParameter() {
    // Reconstruct the 16-bit measurement pause from two consecutive EEPROM bytes
    _measurementPause = (uint16_t)EEPROM.read(EEPROM_ADDRESS_PAUSE)
                      | ((uint16_t)EEPROM.read(EEPROM_ADDRESS_PAUSE + 1) << 8);

    // Clamp to a reasonable range — guards against uninitialised EEPROM (0xFFFF)
    if (_measurementPause > 50000) _measurementPause = 1000;
}


/* ================================================================================
   computeAbsHumidity
   Calculate absolute humidity from temperature and relative humidity.

   Uses the Magnus approximation for saturation vapour pressure combined with
   the ideal gas law.  Result in g/m³.

   @param celsius   Dry-bulb temperature in °C.
   @param humidity  Relative humidity in %RH (0–100).
   @return Absolute humidity in g/m³.
   ================================================================================*/
double computeAbsHumidity(double celsius, double humidity) {
    return 6.112 * exp((17.67 * celsius) / (celsius + 243.5))
           * 2.1674 * humidity / (273.15 + celsius);
}


/* ================================================================================
   computeDewPoint
   Calculate the dew-point temperature using the August–Roche–Magnus formula
   (Sonntag 1990 coefficients).

   @param celsius   Dry-bulb temperature in °C.
   @param humidity  Relative humidity in %RH (0–100).
   @return Dew-point temperature in °C.
   ================================================================================*/
double computeDewPoint(double celsius, double humidity) {
    double RATIO = 373.15 / (273.15 + celsius);

    double SUM = -7.90298 * (RATIO - 1);
    SUM += 5.02808  * log10(RATIO);
    SUM += -1.3816e-7 * (pow(10, (11.344  * (1 - 1 / RATIO))) - 1);
    SUM +=  8.1328e-3 * (pow(10, (-3.49149 * (RATIO - 1)))    - 1);
    SUM += log10(1013.246);

    double VP = pow(10, SUM - 3) * humidity;
    double T  = log(VP / 0.61078);
    return (241.88 * T) / (17.558 - T);
}
