#ifndef TR246_H
#define TR246_H

#include <Wire.h>
#include <FastLED.h>  // Tested with version 3.10.3 — https://github.com/FastLED/FastLED
#include <U8x8lib.h>  // OLED text-only display library
#include <EEPROM.h>


// ── Pin Assignments ───────────────────────────────────────────────────────────────

#define PIN_LED_BLUE  A0  // Discrete RGB LED — blue  channel (active-low)
#define PIN_LED_RED   A1  // Discrete RGB LED — red   channel (active-low)
#define PIN_LED_GREEN A2  // Discrete RGB LED — green channel (active-low)

#define PIN_BUTTON1 4  // Tactile button A (active-low, internal pull-up)
#define PIN_BUTTON2 5  // Tactile button B (active-low, internal pull-up)
#define PIN_BUTTON3 6  // Tactile button C (active-low, internal pull-up)

#define PIN_DIN     9   // WS2812 data-in line
#define PIN_PWR_SW  8   // Power-switch output (HIGH = on)
#define PIN_RHT1_RH A6  // Analogue RHT1 sensor — humidity    output
#define PIN_RHT1_T  A7  // Analogue RHT1 sensor — temperature output


// ── I2C Device Addresses ──────────────────────────────────────────────────────────

#define ADDR_ENS21x 0x42  // ENS21x T/RH sensor I2C address


// ── Discrete LED Logic ────────────────────────────────────────────────────────────
// The discrete RGB LED is common-anode (active-low):
//   LED_ON  = 0  drives the pin LOW  → LED illuminated
//   LED_OFF = 1  drives the pin HIGH → LED off
#define LED_ON  0
#define LED_OFF 1


// ── Timing & Strip Size ───────────────────────────────────────────────────────────

// Button debounce window (ms): edges within this period after the last accepted
// edge are ignored.
#define debounceTime 80

#define NUM_LEDS 3  // Number of WS2812 LEDs on the strip


// ── EEPROM Layout ─────────────────────────────────────────────────────────────────

// Base address for the 16-bit measurement pause (little-endian):
//   byte 31 = low byte, byte 32 = high byte
#define EEPROM_ADDRESS_PAUSE 31


// ── Type Definitions ──────────────────────────────────────────────────────────────

/**
 * @brief Colour presets for the discrete common-anode RGB LED.
 *
 * Bit layout:  bit 2 = GREEN | bit 1 = RED | bit 0 = BLUE
 * Bits can be combined to produce mixed colours (e.g. RED | BLUE = PURPLE).
 */
typedef enum {
    OFF    = 0,  ///< All channels off
    BLUE   = 1,  ///< bit 0
    RED    = 2,  ///< bit 1
    GREEN  = 4,  ///< bit 2
    WHITE  = 7,  ///< All channels on  (RED | GREEN | BLUE)
    PURPLE = 3,  ///< RED   | BLUE
    YELLOW = 6,  ///< RED   | GREEN
    CYAN   = 5,  ///< GREEN | BLUE
    BLINK  = 8   ///< Reserved for future blink mode
} colourLED;

/**
 * @brief Bitmask identifiers for the three tactile buttons.
 *
 * Passed as arguments to readButton() / waitButtonPress() and used as bits
 * in the bitmask returned by readAllButton().
 */
typedef enum {
    buttonA = 1,  ///< bit 0
    buttonB = 2,  ///< bit 1
    buttonC = 4   ///< bit 2
} numberButton;

/**
 * @brief Per-button state maintained by the pin-change ISR.
 *
 * handleButtonInterrupt() updates these fields on every pin-change edge.
 * The main loop reads them to detect debounced transitions.
 */
typedef struct {
    float   pressTime = 0;      ///< Timestamp of the last accepted edge (ms)
    boolean pressed   = true;   ///< Debounced logical state (true = pressed)
    boolean changed   = false;  ///< True for exactly one pass after a transition
} buttonStatus;


// ── Free Functions ────────────────────────────────────────────────────────────────

/**
 * @brief Calculate absolute humidity from temperature and relative humidity.
 * @param celsius   Dry-bulb temperature in °C.
 * @param humidity  Relative humidity in %RH (0–100).
 * @return Absolute humidity in g/m³.
 */
double computeAbsHumidity(double celsius, double humidity);

/**
 * @brief Calculate the dew-point temperature (August–Roche–Magnus formula,
 *        Sonntag 1990 coefficients).
 * @param celsius   Dry-bulb temperature in °C.
 * @param humidity  Relative humidity in %RH (0–100).
 * @return Dew-point temperature in °C.
 */
double computeDewPoint2(double celsius, double humidity);


// ── tr246 Board Class ─────────────────────────────────────────────────────────────

class tr246 {
  public:

    /**
     * @brief Construct a tr246 board object.
     * @param wire  I2C bus instance (defaults to the global Wire object).
     */
    tr246(TwoWire &wire = Wire)
      : _wire(wire) {}

    /**
     * @brief Initialise all on-board peripherals.
     *
     * Must be called once from Arduino setup() before using any other method.
     * Configures GPIO, I2C, pin-change interrupts, FastLED, OLED, ENS21x,
     * RHT1, and EEPROM.  
     */
    void init();


    // ── ISR Integration ───────────────────────────────────────────────────────────

    /**
     * @brief Update button state structs from within the pin-change ISR.
     *

     */
    void handleButtonInterrupt();

    buttonStatus buttonA_Status;  ///< Debounced state of button A
    buttonStatus buttonB_Status;  ///< Debounced state of button B
    buttonStatus buttonC_Status;  ///< Debounced state of button C


    // ── Discrete RGB LED ──────────────────────────────────────────────────────────

    /**
     * @brief Set the discrete RGB LED to a colour preset.
     * @param LED_colour  Colour from the colourLED enum.
     */
    void LED(colourLED LED_colour);

    /**
     * @brief Flash the discrete RGB LED for a fixed duration, then turn it off.
     * @param LED_colour  Colour to flash.
     * @param _duration   On-time in milliseconds.
     */
    void flashLED(colourLED LED_colour, uint16_t _duration);


    // ── Button Polling ────────────────────────────────────────────────────────────

    /**
     * @brief Sample all three buttons and return their state as a bitmask.
     * @return Bitmask: bit0 = buttonA, bit1 = buttonB, bit2 = buttonC.
     *         A set bit means the corresponding button is currently pressed.
     */
    uint8_t readAllButton();

    /**
     * @brief Check whether a specific button is currently pressed.
     * @param myButton  Button to query (buttonA, buttonB, or buttonC).
     * @return 1 if pressed, 0 if released.
     */
    uint8_t readButton(numberButton myButton);

    /**
     * @brief Block until the specified button is pressed.
     * @param myButton  Button to wait for.
     */
    void waitButtonPress(numberButton myButton);

    /**
     * @brief Block until a complete press-and-release cycle is detected.
     *
     * A 50 ms pause between press and release provides additional debounce margin.
     *
     * @param myButton  Button to wait for.
     */
    void waitButtonPressAndRelease(numberButton myButton);


    // ── WS2812 Addressable LED Strip ──────────────────────────────────────────────

    /**
     * @brief Set a single WS2812 LED to an RGB colour and push the update.
     * @param r      Red   channel (0–255).
     * @param g      Green channel (0–255).
     * @param b      Blue  channel (0–255).
     * @param index  LED index in the strip (0 … NUM_LEDS-1); out-of-range ignored.
     */
    void WS_LED(uint8_t r, uint8_t g, uint8_t b, uint8_t index = 0);

    /**
     * @brief Turn off all WS2812 LEDs immediately.
     */
    void WS_clear();


    // ── Power Switch ──────────────────────────────────────────────────────────────

    /**
     * @brief Read the current state of the power-switch output.
     * @return true if the switch is currently driven HIGH (on).
     */
    bool powerSwitch();

    /**
     * @brief Drive the power-switch output to the requested state.
     * @param state  true = on (HIGH), false = off (LOW).
     * @return The state that was applied.
     */
    bool powerSwitch(bool state);

    bool _powerSwitch;  ///< Cached power-switch state


    // ── On-board ENS21x Digital Temperature / Humidity Sensor ────────────────────

    /**
     * @brief Nested driver for the ENS21x I2C temperature/humidity sensor.
     *
     * Access via the public member object:
     * @code
     *   float t = board.ENS21x.temperature();
     *   float h = board.ENS21x.humidity();
     * @endcode
     *
     * Register map:
     *   0x21  SENS_RUN — continuous measurement control
     *   0x30  T_VAL   — raw temperature result (16-bit, little-endian)
     *   0x33  H_VAL   — raw humidity result    (16-bit, little-endian)
     *
     * Conversion (ENS21x datasheet §6.4):
     *   T [°C]  = raw / 64 − 273.15
     *   RH [%]  = raw / 512
     */
    class ENS_t {
      public:
        /** @brief Bind the shared I2C bus pointer.  Called internally by tr246::init(). */
        void begin(TwoWire *wire) { _wire = wire; }

        /** @brief Start continuous T/RH measurement (writes SENS_RUN register). */
        void init();

        /** @brief Read temperature.       @return °C, or NAN if not responding. */
        float temperature();

        /** @brief Read relative humidity. @return %RH, or NAN if not responding. */
        float humidity();

      private:
        TwoWire *_wire = nullptr;  ///< Pointer to the shared I2C bus
    };

    ENS_t ENS21x;  ///< On-board ENS21x sensor — access via board.ENS21x.*


    // ── Analogue RHT1 Temperature / Humidity Sensor ───────────────────────────────

    /**
     * @brief Nested driver for the analogue RHT1 capacitive T/RH sensor.
     *
     * Access via the public member object:
     * @code
     *   float t = board.RHT1.temperature();
     *   float h = board.RHT1.humidity();
     * @endcode
     *
     * @note  The ADC reference must be set to EXTERNAL and the external reference
     *        must equal the VDD supplied to init() (default 3.3 V).
     *        tr246::init() handles this automatically.
     */
    class RHT1_t {
      public:
        /**
         * @brief Store the sensor supply voltage used for ADC scaling.
         * @param VDD  Supply voltage in volts (default 3.3 V).
         */
        void init(float VDD = 3.3);

        /** @brief Read temperature.       @return Temperature in °C. */
        float temperature();

        /** @brief Read relative humidity. @return Humidity in %RH.  */
        float humidity();

      private:
        float _VDD = 3.3;  ///< Sensor supply / ADC reference voltage (V)
    };

    RHT1_t RHT1;  ///< Analogue RHT1 sensor — access via board.RHT1.*


    // ── OLED Display ──────────────────────────────────────────────────────────────

    /** @brief SSD1306 128×64 px OLED, driven over hardware I2C. */
    U8X8_SSD1306_128X64_NONAME_HW_I2C display;


    // ── Measurement Pause (EEPROM-backed) ─────────────────────────────────────────

    /**
     * @brief Set the measurement interval and persist it to EEPROM.
     * @param _pause  Interval in milliseconds (0–50000).
     * @return The value that was stored.
     */
    uint16_t MeasurementPause(uint16_t _pause);

    /**
     * @brief Return the current measurement interval loaded from EEPROM at startup.
     * @return Interval in milliseconds.
     */
    uint16_t MeasurementPause();


  private:

    TwoWire &_wire;            ///< Reference to the shared I2C bus instance
    CRGB _leds[NUM_LEDS];      ///< FastLED pixel buffer for the WS2812 strip

    /** @brief Load persisted settings from EEPROM into RAM.  Called by init(). */
    void loadParameter();

    uint16_t _measurementPause;  ///< Measurement interval in milliseconds
};


#endif  // TR246_H
