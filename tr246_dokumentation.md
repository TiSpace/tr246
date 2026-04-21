# tr246 – Bibliotheks-Dokumentation

Dokumentation der `tr246`-Klasse für das Arduino-Board **tr246**.  
Die Bibliothek steuert die On-Board-Peripherie: RGB-LED, WS2812-Streifen, Taster, ENS21x-Digitalsensor, analoger RHT1-Sensor, OLED-Display sowie EEPROM-gestützte Einstellungen.

---

## Inhaltsverzeichnis

1. [Einbindung & Initialisierung](#1-einbindung--initialisierung)
2. [Enums & Typen](#2-enums--typen)
3. [Diskrete RGB-LED](#3-diskrete-rgb-led)
4. [WS2812 Adressierbare LEDs](#4-ws2812-adressierbare-leds)
5. [Taster (Polling)](#5-taster-polling)
6. [Taster (Interrupt-gesteuert)](#6-taster-interrupt-gesteuert)
7. [ENS21x Digitaler Temperatur-/Feuchtigkeitssensor](#7-ens21x-digitaler-temperatur-feuchtigkeitssensor)
8. [RHT1 Analoger Temperatur-/Feuchtigkeitssensor](#8-rht1-analoger-temperatur-feuchtigkeitssensor)
9. [OLED-Display](#9-oled-display)
10. [Messpause (EEPROM-gespeichert)](#10-messpause-eeprom-gespeichert)
11. [Hilfsfunktionen (global)](#11-hilfsfunktionen-global)
12. [Pin-Belegung & Konstanten](#12-pin-belegung--konstanten)

---

## 1. Einbindung & Initialisierung

```cpp
#include "tr246.h"

tr246 board;          // Standardmäßig wird Wire (I2C0) verwendet
// alternativ:
tr246 board(Wire1);   // Zweiter I2C-Bus

void setup() {
    board.init();     // Muss als Erstes in setup() aufgerufen werden
}
```

### `void init()`

Initialisiert die gesamte On-Board-Peripherie in dieser festen Reihenfolge:

| Schritt | Was passiert |
|---------|--------------|
| 1. GPIO | LED-Pins als Ausgang, Taster als Eingang mit Pull-up, `PIN_PWR_SW` HIGH |
| 2. I2C | Bus wird gestartet |
| 3. Interrupts | Pin-Change-Interrupts für D4–D6 (Taster) werden aktiviert |
| 4. WS2812 | FastLED registriert den Streifen, Helligkeit = 50, alle LEDs aus |
| 5. OLED | Display-Start, zeigt 800 ms lang Compile-Zeit/-Datum |
| 6. ENS21x | Dauermessmodus wird gestartet |
| 7. ADC | Referenzspannung wird auf `EXTERNAL` (3,3 V am AREF-Pin) umgestellt |
| 8. RHT1 | Versorgungsspannung 3,3 V wird intern gespeichert |
| 9. EEPROM | Gespeicherte Messpause wird geladen |

> **Hinweis:** `init()` blockiert für ca. 800 ms wegen der OLED-Startanzeige.

---

## 2. Enums & Typen

### `colourLED` – Farbauswahl für die diskrete RGB-LED

Bitmuster: **bit 2 = GREEN | bit 1 = RED | bit 0 = BLUE**

| Wert     | Farbe               | Bitmuster |
|----------|---------------------|-----------|
| `OFF`    | Aus                 | 000       |
| `BLUE`   | Blau                | 001       |
| `RED`    | Rot                 | 010       |
| `GREEN`  | Grün                | 100       |
| `WHITE`  | Weiß (alle an)      | 111       |
| `PURPLE` | Lila (Rot + Blau)   | 011       |
| `YELLOW` | Gelb (Rot + Grün)   | 110       |
| `CYAN`   | Cyan (Grün + Blau)  | 101       |
| `BLINK`  | Reserviert          | –         |

---

### `numberButton` – Taster-Bezeichner

| Wert      | Bitmask | Pin  |
|-----------|---------|------|
| `buttonA` | `1`     | D4   |
| `buttonB` | `2`     | D5   |
| `buttonC` | `4`     | D6   |

---

### `buttonStatus` – Zustandsstruktur (pro Taster)

Wird von `handleButtonInterrupt()` aktualisiert und kann im Sketch ausgelesen werden.

| Feld        | Typ       | Beschreibung |
|-------------|-----------|--------------|
| `pressed`   | `boolean` | `false` = gedrückt (active-low), `true` = losgelassen |
| `changed`   | `boolean` | `true` für genau einen ISR-Durchlauf nach einem Zustandswechsel |
| `pressTime` | `float`   | Zeitstempel des letzten akzeptierten Flankenwechsels in ms |

```cpp
if (!board.buttonA_Status.pressed && board.buttonA_Status.changed) {
    // Taster A wurde soeben gedrückt
}
```

---

## 3. Diskrete RGB-LED

Die diskrete LED ist **common-anode (active-low)**: `LOW` = Kanal an, `HIGH` = Kanal aus.

---

### `void LED(colourLED LED_colour)`

Setzt die Farbe der diskreten RGB-LED sofort.

| Parameter    | Typ         | Beschreibung                  |
|--------------|-------------|-------------------------------|
| `LED_colour` | `colourLED` | Gewünschte Farbe aus dem Enum |

```cpp
board.LED(GREEN);    // Grün einschalten
board.LED(YELLOW);   // Gelb (Rot + Grün)
board.LED(OFF);      // Ausschalten
```

---

### `void flashLED(colourLED LED_colour, uint8_t _duration)`

Schaltet die LED für eine definierte Zeit ein und danach wieder aus. **Blockierend.**

| Parameter    | Typ         | Beschreibung                         |
|--------------|-------------|--------------------------------------|
| `LED_colour` | `colourLED` | Farbe des Blitzes                    |
| `_duration`  | `uint8_t`   | Leuchtdauer in Millisekunden (0–255) |

```cpp
board.flashLED(RED, 100);   // 100 ms rot aufleuchten
board.flashLED(BLUE, 50);   // 50 ms blau aufleuchten
```

> **Hinweis:** Die Funktion ruft intern `delay()` auf und blockiert daher den gesamten Programmablauf für die angegebene Dauer.

---

## 4. WS2812 Adressierbare LEDs

Das Board verfügt über **3 WS2812-LEDs** (Index 0–2), gesteuert über FastLED.  
Globale Helligkeit beim Start: **50 von 255**.

---

### `void WS_LED(uint8_t r, uint8_t g, uint8_t b, uint8_t index = 0)`

Setzt eine einzelne WS2812-LED auf eine RGB-Farbe und überträgt den Wert sofort.

| Parameter | Typ       | Beschreibung                             |
|-----------|-----------|------------------------------------------|
| `r`       | `uint8_t` | Rotanteil (0–255)                        |
| `g`       | `uint8_t` | Grünanteil (0–255)                       |
| `b`       | `uint8_t` | Blauanteil (0–255)                       |
| `index`   | `uint8_t` | LED-Index im Streifen (0–2), Standard: 0 |

Ungültige Index-Werte (≥ `NUM_LEDS`) werden stillschweigend ignoriert.

```cpp
board.WS_LED(255, 0, 0);        // LED 0 → Rot
board.WS_LED(0, 255, 0, 1);     // LED 1 → Grün
board.WS_LED(0, 0, 255, 2);     // LED 2 → Blau
board.WS_LED(255, 100, 0, 0);   // LED 0 → Orange
```

---

### `void WS_clear()`

Schaltet alle WS2812-LEDs aus (setzt alle auf Schwarz) und überträgt sofort.

```cpp
board.WS_clear();
```

---

## 5. Taster (Polling)

Alle drei Taster sind **active-low** mit internem Pull-up: nicht gedrückt = HIGH, gedrückt = LOW.

---

### `uint8_t readAllButton()`

Liest alle drei Taster gleichzeitig und gibt das Ergebnis als Bitmaske zurück.

**Rückgabe:** `uint8_t` — `bit0 = buttonA`, `bit1 = buttonB`, `bit2 = buttonC`  
Ein gesetztes Bit bedeutet: dieser Taster ist **gedrückt**.

| Rückgabewert | Bedeutung                    |
|--------------|------------------------------|
| `0`          | Kein Taster gedrückt         |
| `1`          | Nur buttonA gedrückt         |
| `3`          | buttonA und buttonB gedrückt |
| `7`          | Alle drei Taster gedrückt    |

```cpp
uint8_t state = board.readAllButton();
if (state & buttonA) { /* A ist gedrückt */ }
if (state & buttonC) { /* C ist gedrückt */ }
```

---

### `uint8_t readButton(numberButton myButton)`

Prüft, ob ein bestimmter Taster gerade gedrückt ist.

| Parameter  | Typ            | Beschreibung        |
|------------|----------------|---------------------|
| `myButton` | `numberButton` | Zu prüfender Taster |

**Rückgabe:** `1` = gedrückt, `0` = losgelassen

```cpp
if (board.readButton(buttonB) == 1) {
    // Taster B ist aktuell gedrückt
}
```

---

### `void waitButtonPress(numberButton myButton)`

**Blockiert** den Programmablauf, bis der angegebene Taster gedrückt wird.

| Parameter  | Typ            | Beschreibung             |
|------------|----------------|--------------------------|
| `myButton` | `numberButton` | Auf diesen Taster warten |

```cpp
board.waitButtonPress(buttonA);
// Ab hier: buttonA wurde gedrückt
```

---

### `void waitButtonPressAndRelease(numberButton myButton)`

**Blockiert** den Programmablauf, bis ein vollständiger Drück-und-Loslass-Zyklus abgeschlossen ist. Zwischen Drücken und Loslassen liegt ein 50 ms Debounce-Delay.

| Parameter  | Typ            | Beschreibung             |
|------------|----------------|--------------------------|
| `myButton` | `numberButton` | Auf diesen Taster warten |

```cpp
board.waitButtonPressAndRelease(buttonC);
// Ab hier: buttonC wurde vollständig gedrückt und losgelassen
```

---

## 6. Taster (Interrupt-gesteuert)

Für nicht-blockierende Anwendungen können die Taster per Pin-Change-Interrupt ausgewertet werden. `init()` aktiviert die Interrupts automatisch — die ISR-Funktion muss aber im Sketch selbst definiert werden.

---

### `void handleButtonInterrupt()`

Muss aus der ISR (`ISR(PCINT2_vect)`) aufgerufen werden. Aktualisiert die drei öffentlichen `buttonX_Status`-Strukturen mit Debouncing (Fenster: 80 ms = `debounceTime`).

```cpp
tr246 board;

ISR(PCINT2_vect) {
    board.handleButtonInterrupt();
}

void loop() {
    if (!board.buttonA_Status.pressed && board.buttonA_Status.changed) {
        // Flanke: buttonA gedrückt
        board.LED(RED);
    }
}
```

**Öffentliche Statusstrukturen:**

| Objekt           | Beschreibung        |
|------------------|---------------------|
| `buttonA_Status` | Status von Taster A |
| `buttonB_Status` | Status von Taster B |
| `buttonC_Status` | Status von Taster C |

Felder jeder Struktur: siehe Abschnitt [2 – buttonStatus](#buttonstatus--zustandsstruktur-pro-taster).

---

## 7. ENS21x Digitaler Temperatur-/Feuchtigkeitssensor

Der ENS21x ist fest an I2C-Adresse `0x42` verdrahtet.  
Der Zugriff erfolgt über das öffentliche Member-Objekt `board.ENS21x`.  
`init()` kümmert sich um Initialisierung — **kein manueller Aufruf nötig**.

| Register | Adresse | Funktion |
|----------|---------|----------|
| SENS_RUN | `0x21`  | Dauermessung starten (bit1=T, bit0=RH) |
| T_VAL    | `0x30`  | Roher Temperaturwert (16-bit, little-endian) |
| H_VAL    | `0x33`  | Roher Feuchtigkeitswert (16-bit, little-endian) |

---

### `float board.ENS21x.temperature()`

Liest den aktuellen Temperaturwert vom Sensor.

**Rückgabe:** Temperatur in **°C** (`float`), oder `NAN` wenn der Sensor nicht antwortet.  
**Formel:** `T [°C] = raw / 64 − 273,15`

```cpp
float t = board.ENS21x.temperature();
if (!isnan(t)) {
    Serial.print("Temperatur: ");
    Serial.print(t, 1);
    Serial.println(" °C");
}
```

---

### `float board.ENS21x.humidity()`

Liest den aktuellen Feuchtigkeitswert vom Sensor.

**Rückgabe:** Relative Luftfeuchte in **%RH** (`float`), oder `NAN` wenn der Sensor nicht antwortet.  
**Formel:** `RH [%] = raw / 512`

```cpp
float rh = board.ENS21x.humidity();
if (!isnan(rh)) {
    Serial.print("Feuchte: ");
    Serial.print(rh, 1);
    Serial.println(" %RH");
}
```

> **Hinweis:** Beide Funktionen geben `NAN` zurück, wenn der Sensor nicht antwortet. Immer mit `isnan()` prüfen, bevor der Wert weiterverarbeitet wird.

---

## 8. RHT1 Analoger Temperatur-/Feuchtigkeitssensor

Externer analoger Sensor, angeschlossen an `A6` (Feuchte) und `A7` (Temperatur).  
Der ADC arbeitet gegen eine externe 3,3-V-Referenz am AREF-Pin — wird von `init()` automatisch konfiguriert.  
Der Zugriff erfolgt über das öffentliche Member-Objekt `board.RHT1`.

| Kanal      | Pin | Formel |
|------------|-----|--------|
| Feuchte    | A6  | `RH [%RH] = −36,48 + 185,19 × (ADC / 1023)` |
| Temperatur | A7  | `T [°C] = −66,875 + 218,75 × (ADC / 1023)` |

---

### `float board.RHT1.temperature()`

Liest den analogen Temperaturkanal und konvertiert den Wert.

**Rückgabe:** Temperatur in **°C** (`float`)

```cpp
float t = board.RHT1.temperature();
Serial.print("RHT1 Temp: ");
Serial.println(t, 1);
```

---

### `float board.RHT1.humidity()`

Liest den analogen Feuchtigkeitskanal und konvertiert den Wert.

**Rückgabe:** Relative Luftfeuchte in **%RH** (`float`)

```cpp
float rh = board.RHT1.humidity();
Serial.print("RHT1 Feuchte: ");
Serial.println(rh, 1);
```

> **Hinweis:** Die VDD-Terme kürzen sich in der Formel heraus — das Ergebnis ist unabhängig von der tatsächlichen Versorgungsspannung, solange ADC-Referenz und Sensorversorgung dieselbe Quelle nutzen.

---

## 9. OLED-Display

Das Display ist ein **SSD1306 128×64** an I2C-Adresse `0x3C`.  
Es ist als öffentliches Member direkt zugänglich:

```cpp
board.display   // Typ: U8X8_SSD1306_128X64_NONAME_HW_I2C
```

`init()` initialisiert das Display und gibt kurz Compile-Datum und -Uhrzeit aus. Danach kann es frei beschrieben werden:

```cpp
board.display.clear();
board.display.setCursor(0, 0);   // (Spalte, Zeile in Zeichen-Einheiten)
board.display.print("Hallo!");

board.display.setCursor(0, 2);
board.display.print(board.ENS21x.temperature(), 1);
board.display.print(" C");
```

**Voreingestellte Schriftart:** `u8x8_font_8x13_1x2_r`  
Jede Zeile belegt 2 Zeichen-Zeilen → nutzbare Cursorpositionen: **0, 2, 4, 6**

Vollständige API: [U8x8-Dokumentation](https://github.com/olikraus/u8g2/wiki/u8x8reference)

---

## 10. Messpause (EEPROM-gespeichert)

Die Messpause (Intervall zwischen Sensormessungen) wird als 16-Bit-Wert im EEPROM gespeichert (little-endian, Adressen 31–32). Beim Start lädt `init()` den gespeicherten Wert automatisch. Werte über 50 000 ms (z. B. bei frisch beschriebenem EEPROM = `0xFFFF`) werden auf 1 000 ms zurückgesetzt.

---

### `uint16_t setMeasurementPause(uint16_t _pause)`

Setzt die Messpause und schreibt den Wert dauerhaft ins EEPROM.

| Parameter | Typ        | Beschreibung                         |
|-----------|------------|--------------------------------------|
| `_pause`  | `uint16_t` | Intervall in Millisekunden (0–50000) |

**Rückgabe:** Der gespeicherte Wert (`uint16_t`)

```cpp
board.setMeasurementPause(2000);   // Intervall auf 2 Sekunden setzen
```

---

### `uint16_t getMeasurementPause()`

Gibt die beim Start aus dem EEPROM geladene Messpause zurück.

**Rückgabe:** Intervall in Millisekunden (`uint16_t`)

```cpp
uint16_t pause = board.getMeasurementPause();
Serial.print("Messpause: ");
Serial.print(pause);
Serial.println(" ms");
```

**Typisches Verwendungsmuster im Sketch:**

```cpp
unsigned long lastMeasurement = 0;

void loop() {
    if (millis() - lastMeasurement >= board.getMeasurementPause()) {
        lastMeasurement = millis();
        // Messung durchführen …
    }
}
```

---

## 11. Hilfsfunktionen (global)

Diese Funktionen sind **nicht Teil der Klasse** und stehen nach dem `#include "tr246.h"` direkt zur Verfügung.

---

### `double computeAbsHumidity(double celsius, double humidity)`

Berechnet die absolute Luftfeuchte aus Temperatur und relativer Feuchte.

| Parameter  | Typ      | Beschreibung                    |
|------------|----------|---------------------------------|
| `celsius`  | `double` | Temperatur in °C                |
| `humidity` | `double` | Relative Feuchte in %RH (0–100) |

**Rückgabe:** Absolute Feuchte in **g/m³** (`double`)

```cpp
double absH = computeAbsHumidity(
    board.ENS21x.temperature(),
    board.ENS21x.humidity()
);
Serial.print("Abs. Feuchte: ");
Serial.print(absH, 2);
Serial.println(" g/m³");
```

---

### `double computeDewPoint2(double celsius, double humidity)`

Berechnet den Taupunkt nach der Tetens-Näherungsformel (Sonntag-Variante).

| Parameter  | Typ      | Beschreibung                    |
|------------|----------|---------------------------------|
| `celsius`  | `double` | Temperatur in °C                |
| `humidity` | `double` | Relative Feuchte in %RH (0–100) |

**Rückgabe:** Taupunkt in **°C** (`double`)

```cpp
double td = computeDewPoint2(
    board.ENS21x.temperature(),
    board.ENS21x.humidity()
);
Serial.print("Taupunkt: ");
Serial.print(td, 1);
Serial.println(" °C");
```

---

## 12. Pin-Belegung & Konstanten

### Pin-Belegung

| Konstante       | Pin | Funktion                         |
|-----------------|-----|----------------------------------|
| `PIN_LED_BLUE`  | A0  | Blaukanal der diskreten RGB-LED  |
| `PIN_LED_RED`   | A1  | Rotkanal der diskreten RGB-LED   |
| `PIN_LED_GREEN` | A2  | Grünkanal der diskreten RGB-LED  |
| `PIN_BUTTON1`   | D4  | Taster A (active-low, Pull-up)   |
| `PIN_BUTTON2`   | D5  | Taster B (active-low, Pull-up)   |
| `PIN_BUTTON3`   | D6  | Taster C (active-low, Pull-up)   |
| `PIN_DIN`       | D9  | Data-In WS2812-Streifen          |
| `PIN_PWR_SW`    | D8  | Versorgungsschalter (HIGH = an)  |
| `PIN_RHT1_RH`   | A6  | Analogeingang Feuchte (RHT1)     |
| `PIN_RHT1_T`    | A7  | Analogeingang Temperatur (RHT1)  |

### Weitere Konstanten

| Konstante              | Wert   | Bedeutung                                                         |
|------------------------|--------|-------------------------------------------------------------------|
| `LED_ON`               | `0`    | Active-low: Pin LOW → LED leuchtet                                |
| `LED_OFF`              | `1`    | Pin HIGH → LED aus                                                |
| `debounceTime`         | `80`   | Entprellzeit in ms                                                |
| `NUM_LEDS`             | `3`    | Anzahl WS2812-LEDs                                                |
| `ADDR_ENS21x`          | `0x42` | I2C-Adresse des ENS21x-Sensors                                    |
| `EEPROM_ADDRESS_PAUSE` | `31`   | EEPROM-Startadresse der Messpause (Low-Byte = 31, High-Byte = 32) |

---

*Dokumentation generiert auf Basis von `tr246.h` und `tr246.cpp`.*
