# tr246 – Library Documentation

Documentation of the `tr246` class for the **tr246** Arduino board.  
The library controls the on-board peripherals: RGB LED, WS2812 strip, pushbuttons, ENS21x digital sensor, analog RHT1 sensor, OLED display, power switch, and EEPROM-backed settings.

---

## Table of Contents

1. [Inclusion &amp; Initialization](#1-inclusion--initialization)
2. [Enums &amp; Types](#2-enums--types)
3. [Discrete RGB LED](#3-discrete-rgb-led)
4. [WS2812 Addressable LEDs](#4-ws2812-addressable-leds)
5. [Pushbuttons (Polling)](#5-pushbuttons-polling)
6. [Pushbuttons (Interrupt-driven)](#6-pushbuttons-interrupt-driven)
7. [Power Switch](#7-power-switch)
8. [ENS21x Digital Temperature/Humidity Sensor](#8-ens21x-digital-temperaturehumidity-sensor)
9. [RHT1 Analog Temperature/Humidity Sensor](#9-rht1-analog-temperaturehumidity-sensor)
10. [OLED Display](#10-oled-display)
11. [Measurement Pause (EEPROM-stored)](#11-measurement-pause-eeprom-stored)
12. [Helper Functions (global)](#12-helper-functions-global)
13. [Pin Assignment &amp; Constants](#13-pin-assignment--constants)

---

## 1. Inclusion &amp; Initialization

```cpp
#include "tr246.h"

tr246 board;          // By default, Wire (I2C0) is used

void setup() {
    board.init();     // Must be called first in setup()
}
```

### `void init()`

Initializes the entire on-board peripherals in this fixed order:


| Step          | What happens                                                             |
| ------------- | ------------------------------------------------------------------------ |
| 1. GPIO       | LED pins as output, pushbuttons as input with pull-up, `PIN_PWR_SW` HIGH |
| 2. I2C        | Bus is started, discrete LED set to `OFF`                                |
| 3. Interrupts | Pin-change interrupts for D4–D6 (pushbuttons) are enabled                |
| 4. WS2812     | FastLED registers the strip, brightness = 50, all LEDs off               |
| 5. OLED       | Display start, shows compile time/date for 800 ms, then "Arduino tr246"  |
| 6. ENS21x     | Continuous measurement mode is started                                   |
| 7. ADC        | Reference voltage is switched to `EXTERNAL` (3.3 V at the AREF pin)      |
| 8. RHT1       | Supply voltage of 3.3 V is stored internally                             |
| 9. EEPROM     | Saved measurement pause is loaded                                        |


> **Note:** `init()` blocks for approx. 800 ms because of the OLED start screen.

---

## 2. Enums &amp; Types

### `colourLED` – Colour selection for the discrete RGB LED

Bit pattern: **bit 2 = GREEN | bit 1 = RED | bit 0 = BLUE**


| Value    | Colour               | Bit pattern |
| -------- | -------------------- | ----------- |
| `OFF`    | Off                  | 000         |
| `BLUE`   | Blue                 | 001         |
| `RED`    | Red                  | 010         |
| `GREEN`  | Green                | 100         |
| `WHITE`  | White (all on)       | 111         |
| `PURPLE` | Purple (red + blue)  | 011         |
| `YELLOW` | Yellow (red + green) | 110         |
| `CYAN`   | Cyan (green + blue)  | 101         |
| `BLINK`  | Reserved             | –           |


---

### `numberButton` – Pushbutton identifiers


| Value     | Bitmask | Pin |
| --------- | ------- | --- |
| `buttonA` | `1`     | D4  |
| `buttonB` | `2`     | D5  |
| `buttonC` | `4`     | D6  |


---

### `buttonStatus` – Status structure (per pushbutton)

Updated by `handleButtonInterrupt()` and can be read in the sketch.


| Field       | Type      | Initialization | Description                                                                                    |
| ----------- | --------- | -------------- | ---------------------------------------------------------------------------------------------- |
| `pressed`   | `boolean` | `true`         | Current logic level: `false` = pressed (active-low: LOW level), `true` = released (HIGH level) |
| `changed`   | `boolean` | `false`        | `true` for exactly one ISR pass after an accepted edge transition                              |
| `pressTime` | `float`   | `0`            | Timestamp of the last accepted edge transition in ms                                           |


```cpp
if (!board.buttonA_Status.pressed && board.buttonA_Status.changed) {
    // Pushbutton A was just pressed (falling edge accepted)
}
```

> **Note:** Since the pushbuttons are active-low, `pressed == false` corresponds to the physically pressed state.

---

## 3. Discrete RGB LED

The discrete LED is **common-anode (active-low)**: `LOW` = channel on, `HIGH` = channel off.

---

### `void LED(colourLED LED_colour)`

Sets the colour of the discrete RGB LED immediately.


| Parameter    | Type        | Description                  |
| ------------ | ----------- | ---------------------------- |
| `LED_colour` | `colourLED` | Desired colour from the enum |


```cpp
board.LED(GREEN);    // Turn green on
board.LED(YELLOW);   // Yellow (red + green)
board.LED(OFF);      // Turn off
```

---

### `void flashLED(colourLED LED_colour, uint16_t _duration)`

Turns the LED on for a defined time and then off again. **Blocking.**


| Parameter    | Type        | Description                       |
| ------------ | ----------- | --------------------------------- |
| `LED_colour` | `colourLED` | Colour of the flash               |
| `_duration`  | `uint16_t`  | On-time in milliseconds (0–65535) |


```cpp
board.flashLED(RED, 100);   // Flash red for 100 ms
board.flashLED(BLUE, 50);   // Flash blue for 50 ms
```

> **Note:** The function internally calls `delay()` and therefore blocks the entire program flow for the specified duration.

---

## 4. WS2812 Addressable LEDs

The board has **3 WS2812 LEDs** (index 0–2), controlled via FastLED.  
Global brightness at startup: **50 out of 255**.

---

### `void WS_LED(uint8_t r, uint8_t g, uint8_t b, uint8_t index = 0)`

Sets a single WS2812 LED to an RGB colour and transmits the value immediately.


| Parameter | Type      | Description                              |
| --------- | --------- | ---------------------------------------- |
| `r`       | `uint8_t` | Red component (0–255)                    |
| `g`       | `uint8_t` | Green component (0–255)                  |
| `b`       | `uint8_t` | Blue component (0–255)                   |
| `index`   | `uint8_t` | LED index in the strip (0–2), default: 0 |


Invalid index values (≥ `NUM_LEDS`) are silently ignored.

```cpp
board.WS_LED(255, 0, 0);        // LED 0 → red
board.WS_LED(0, 255, 0, 1);     // LED 1 → green
board.WS_LED(0, 0, 255, 2);     // LED 2 → blue
board.WS_LED(255, 100, 0, 0);   // LED 0 → orange
```

---

### `void WS_clear()`

Turns all WS2812 LEDs off (sets all to black) and transmits immediately.

```cpp
board.WS_clear();
```

---

## 5. Pushbuttons (Polling)

All three pushbuttons are **active-low** with internal pull-up: not pressed = HIGH, pressed = LOW.

---

### `uint8_t readAllButton()`

Reads all three pushbuttons simultaneously and returns the result as a bitmask.

**Return value:** `uint8_t` — `bit0 = buttonA`, `bit1 = buttonB`, `bit2 = buttonC`  
A set bit means: this pushbutton is **pressed**.


| Return value | Meaning                       |
| ------------ | ----------------------------- |
| `0`          | No pushbutton pressed         |
| `1`          | Only buttonA pressed          |
| `3`          | buttonA and buttonB pressed   |
| `7`          | All three pushbuttons pressed |


```cpp
uint8_t state = board.readAllButton();
if (state & buttonA) { /* A is pressed */ }
if (state & buttonC) { /* C is pressed */ }
```

---

### `uint8_t readButton(numberButton myButton)`

Checks whether a specific pushbutton is currently pressed.


| Parameter  | Type           | Description         |
| ---------- | -------------- | ------------------- |
| `myButton` | `numberButton` | Pushbutton to check |


**Return value:** `1` = pressed, `0` = released

```cpp
if (board.readButton(buttonB) == 1) {
    // Pushbutton B is currently pressed
}
```

---

### `void waitButtonPress(numberButton myButton)`

**Blocks** the program flow until the specified pushbutton is pressed.


| Parameter  | Type           | Description            |
| ---------- | -------------- | ---------------------- |
| `myButton` | `numberButton` | Pushbutton to wait for |


```cpp
board.waitButtonPress(buttonA);
// From here on: buttonA has been pressed
```

---

### `void waitButtonPressAndRelease(numberButton myButton)`

**Blocks** the program flow until a complete press-and-release cycle is completed. Between pressing and releasing there is a 50 ms debounce delay.


| Parameter  | Type           | Description            |
| ---------- | -------------- | ---------------------- |
| `myButton` | `numberButton` | Pushbutton to wait for |


```cpp
board.waitButtonPressAndRelease(buttonC);
// From here on: buttonC has been fully pressed and released
```

---

## 6. Pushbuttons (Interrupt-driven)

For non-blocking applications, the pushbuttons can be evaluated via pin-change interrupts. `init()` enables the interrupts automatically — but the ISR function must be defined in the sketch itself.

---

### `void handleButtonInterrupt()`

Must be called from the ISR (`ISR(PCINT2_vect)`). Updates the three public `buttonX_Status` structures with debouncing (window: 80 ms = `debounceTime`).

The following applies to each pushbutton: If the current hardware level differs from the last stored state **and** the debounce window has expired, the edge is accepted (`changed = true`, `pressed` and `pressTime` are updated). If level and state match, `changed` is reset to `false` — so `changed` is only set for exactly one ISR pass.

```cpp
tr246 board;

ISR(PCINT2_vect) {
    board.handleButtonInterrupt();
}

void loop() {
    if (!board.buttonA_Status.pressed && board.buttonA_Status.changed) {
        // Edge: buttonA pressed
        board.LED(RED);
    }
}
```

**Public status structures:**


| Object           | Description            |
| ---------------- | ---------------------- |
| `buttonA_Status` | Status of pushbutton A |
| `buttonB_Status` | Status of pushbutton B |
| `buttonC_Status` | Status of pushbutton C |


Fields of each structure: see section [2 – buttonStatus](#buttonstatus--status-structure-per-pushbutton).

---

## 7. Power Switch

The power switch output (`PIN_PWR_SW`, D8) maintains the board's power supply. `init()` automatically sets it HIGH.

---

### `bool powerSwitch()`

Reads the current state of the power switch output from the pin and returns it.

**Return value:** `true` if the output is HIGH (power on), `false` if LOW.

```cpp
bool state = board.powerSwitch();
```

---

### `bool powerSwitch(bool state)`

Sets the power switch output to the desired state and stores it internally.


| Parameter | Type   | Description                             |
| --------- | ------ | --------------------------------------- |
| `state`   | `bool` | `true` = on (HIGH), `false` = off (LOW) |


**Return value:** The state actually applied.

```cpp
board.powerSwitch(true);   // Turn power supply on
board.powerSwitch(false);  // Turn power supply off
```

> **Note:** Setting `false` shuts down the board's power supply — the sketch will not continue running afterwards.

---

## 8. ENS21x Digital Temperature/Humidity Sensor

The ENS21x is hard-wired to I2C address `0x42`.  
Access is via the public member object `board.ENS21x`.  
`init()` takes care of initialization — **no manual call needed**.


| Register  | Address | Function                                       |
| --------- | ------- | ---------------------------------------------- |
| SENS\_RUN | `0x21`  | Start continuous measurement (bit1=T, bit0=RH) |
| T\_VAL    | `0x30`  | Raw temperature value (16-bit, little-endian)  |
| H\_VAL    | `0x33`  | Raw humidity value (16-bit, little-endian)     |


---

### `float board.ENS21x.temperature()`

Reads the current temperature value from the sensor.

**Return value:** Temperature in **°C** (`float`), or `NAN` if the sensor does not respond.  
**Formula:** `T [°C] = raw / 64 − 273.15`

```cpp
float t = board.ENS21x.temperature();
if (!isnan(t)) {
    Serial.print("Temperature: ");
    Serial.print(t, 1);
    Serial.println(" °C");
}
```

---

### `float board.ENS21x.humidity()`

Reads the current humidity value from the sensor.

**Return value:** Relative humidity in **%RH** (`float`), or `NAN` if the sensor does not respond.  
**Formula:** `RH [%] = raw / 512`

```cpp
float rh = board.ENS21x.humidity();
if (!isnan(rh)) {
    Serial.print("Humidity: ");
    Serial.print(rh, 1);
    Serial.println(" %RH");
}
```

> **Note:** Both functions return `NAN` if the sensor does not respond. Always check with `isnan()` before further processing the value.

---

## 9. RHT1 Analog Temperature/Humidity Sensor

External analog sensor, connected to `A6` (humidity) and `A7` (temperature).  
The ADC operates against an external 3.3 V reference at the AREF pin — configured automatically by `init()`.  
Access is via the public member object `board.RHT1`.

Since the ADC measurement is ratiometric (VDD terms cancel out), the conversion formulas are independent of the actual supply voltage.


| Channel     | Pin | Formula                                     |
| ----------- | --- | ------------------------------------------- |
| Humidity    | A6  | `RH [%RH] = −36.48 + 185.19 × (ADC / 1023)` |
| Temperature | A7  | `T [°C] = −66.875 + 218.75 × (ADC / 1023)`  |


---

### `float board.RHT1.temperature()`

Reads the analog temperature channel and converts the value.

**Return value:** Temperature in **°C** (`float`)

```cpp
float t = board.RHT1.temperature();
Serial.print("RHT1 temp: ");
Serial.println(t, 1);
```

---

### `float board.RHT1.humidity()`

Reads the analog humidity channel and converts the value.

**Return value:** Relative humidity in **%RH** (`float`)

```cpp
float rh = board.RHT1.humidity();
Serial.print("RHT1 humidity: ");
Serial.println(rh, 1);
```

---

## 10. OLED Display

The display is an **SSD1306 128×64** at I2C address `0x3C`.  
It is directly accessible as a public member:

```cpp
board.display   // Type: U8X8_SSD1306_128X64_NONAME_HW_I2C
```

`init()` initializes the display and briefly shows the compile date and time (800 ms), then "Arduino tr246" is displayed. After that it can be written to freely:

```cpp
board.display.clear();
board.display.setCursor(0, 0);   // (column, row in character units)
board.display.print("Hello!");

board.display.setCursor(0, 2);
board.display.print(board.ENS21x.temperature(), 1);
board.display.print(" C");
```

**Preset font:** `u8x8_font_8x13_1x2_r`  
Each line occupies 2 character rows → usable cursor positions: **0, 2, 4, 6**

Full API: [U8x8 documentation](https://github.com/olikraus/u8g2/wiki/u8x8reference)

---

## 11. Measurement Pause (EEPROM-stored)

The measurement pause (interval between sensor measurements) is stored as a 16-bit value in EEPROM (little-endian, addresses 31–32). At startup, `init()` automatically loads the stored value. Values above 50,000 ms (e.g. with a fresh EEPROM = `0xFFFF`) are reset to 1,000 ms.

Getter and setter are implemented as **overloaded methods** of the same name `MeasurementPause()`.

---

### `uint16_t MeasurementPause(uint16_t _pause)` — Setter

Sets the measurement pause and permanently writes the value to EEPROM.


| Parameter | Type       | Description                        |
| --------- | ---------- | ---------------------------------- |
| `_pause`  | `uint16_t` | Interval in milliseconds (0–50000) |


**Return value:** The stored value (`uint16_t`)

```cpp
board.MeasurementPause(2000);   // Set interval to 2 seconds
```

---

### `uint16_t MeasurementPause()` — Getter

Returns the measurement pause currently held in RAM (loaded from EEPROM at startup).

**Return value:** Interval in milliseconds (`uint16_t`)

```cpp
uint16_t pause = board.MeasurementPause();
Serial.print("Measurement pause: ");
Serial.print(pause);
Serial.println(" ms");
```

**Typical usage pattern in a sketch:**

```cpp
unsigned long lastMeasurement = 0;

void loop() {
    if (millis() - lastMeasurement >= board.MeasurementPause()) {
        lastMeasurement = millis();
        // Perform measurement …
    }
}
```

---

## 12. Helper Functions (global)

These functions are **not part of the class** and are directly available after `#include "tr246.h"`.

---

### `double computeAbsHumidity(double celsius, double humidity)`

Calculates the absolute humidity from temperature and relative humidity.  
Uses the Magnus approximation for saturation vapour pressure combined with the ideal gas law.


| Parameter  | Type     | Description                      |
| ---------- | -------- | -------------------------------- |
| `celsius`  | `double` | Temperature in °C                |
| `humidity` | `double` | Relative humidity in %RH (0–100) |


**Return value:** Absolute humidity in **g/m³** (`double`)

```cpp
double absH = computeAbsHumidity(
    board.ENS21x.temperature(),
    board.ENS21x.humidity()
);
Serial.print("Abs. humidity: ");
Serial.print(absH, 2);
Serial.println(" g/m³");
```

---

### `double computeDewPoint2(double celsius, double humidity)`

Calculates the dew point using the **August–Roche–Magnus formula (Sonntag 1990)**.


| Parameter  | Type     | Description                      |
| ---------- | -------- | -------------------------------- |
| `celsius`  | `double` | Temperature in °C                |
| `humidity` | `double` | Relative humidity in %RH (0–100) |


**Return value:** Dew point in **°C** (`double`)

```cpp
double td = computeDewPoint2(
    board.ENS21x.temperature(),
    board.ENS21x.humidity()
);
Serial.print("Dew point: ");
Serial.print(td, 1);
Serial.println(" °C");
```

---

## 13. Pin Assignment &amp; Constants

### Pin Assignment


| Constant        | Pin | Function                              |
| --------------- | --- | ------------------------------------- |
| `PIN_LED_BLUE`  | A0  | Blue channel of the discrete RGB LED  |
| `PIN_LED_RED`   | A1  | Red channel of the discrete RGB LED   |
| `PIN_LED_GREEN` | A2  | Green channel of the discrete RGB LED |
| `PIN_BUTTON1`   | D4  | Pushbutton A (active-low, pull-up)    |
| `PIN_BUTTON2`   | D5  | Pushbutton B (active-low, pull-up)    |
| `PIN_BUTTON3`   | D6  | Pushbutton C (active-low, pull-up)    |
| `PIN_DIN`       | D9  | Data-in WS2812 strip                  |
| `PIN_PWR_SW`    | D8  | Power switch (HIGH = on)              |
| `PIN_RHT1_RH`   | A6  | Analog input humidity (RHT1)          |
| `PIN_RHT1_T`    | A7  | Analog input temperature (RHT1)       |


### Other Constants


| Constant               | Value  | Meaning                                                                       |
| ---------------------- | ------ | ----------------------------------------------------------------------------- |
| `LED_ON`               | `0`    | Active-low: pin LOW → LED on                                                  |
| `LED_OFF`              | `1`    | Pin HIGH → LED off                                                            |
| `debounceTime`         | `80`   | Debounce time in ms                                                           |
| `NUM_LEDS`             | `3`    | Number of WS2812 LEDs                                                         |
| `ADDR_ENS21x`          | `0x42` | I2C address of the ENS21x sensor                                              |
| `EEPROM_ADDRESS_PAUSE` | `31`   | EEPROM start address of the measurement pause (low byte = 31, high byte = 32) |


---

*Documentation generated based on `tr246.h` and `tr246.cpp`.*