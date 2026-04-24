#include <tr246.h>
#include <Streaming.h>

// Global objects and variables
tr246 board;  // Main board instance
boolean displayCont = false;  // Flag to control continuous display
unsigned long prevTime = 0;   // Stores the last time measurements were printed

// Function prototypes
void readRHT1();              // Reads and prints RHT1 sensor data
void readENS21x();            // Reads and prints ENS21x sensor data
void buttonActivity();        // Handles button presses and LED feedback

void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    while (!Serial) ;  // Wait for serial port to connect (for USB boards)
    delay(100);        // Small delay for stability

    // Print build info for debugging and traceability
    Serial.print("\n\n");
    Serial.println(__FILE__);  // Source file name
    Serial.print("Compiled:\t");
    Serial.print(__TIME__);    // Compilation time
    Serial.print("\t");
    Serial.println(__DATE__);  // Compilation date

    // Initialize hardware and signal HSS is on
    board.init();
    board.WS_LED(128, 0, 0, 0);  // Red LED indicates HSS is active
}

void loop() {
    buttonActivity();   // Check for button presses
    runCommunication(); // Handle communication tasks (not shown)

    // Print sensor data at regular intervals if display is enabled
    if (displayCont && (millis() - prevTime) > board.MeasurementPause()) {
        // Print time, temperature, humidity, and absolute humidity for both sensors
        Serial.print(millis() / 1000.0f ,3);
        Serial << "\t\t"  // Time in seconds
               << board.RHT1.temperature() << "\t"   // RHT1 temperature
               << board.RHT1.humidity() << "\t"      // RHT1 humidity
               << computeAbsHumidity(board.RHT1.temperature(), board.RHT1.humidity()) << "\t\t" // RHT1 absolute humidity
               << board.ENS21x.temperature() << "\t" // ENS21x temperature
               << board.ENS21x.humidity() << "\t"    // ENS21x humidity
               << computeAbsHumidity(board.ENS21x.temperature(), board.ENS21x.humidity()) << "\n"; // ENS21x absolute humidity

        prevTime = millis();  // Update last print time
        board.flashLED(YELLOW, 2);  // Flash yellow LED to indicate measurement
    }
}

// Reads and prints RHT1 sensor data
void readRHT1() {
    Serial.print("\t\tRHT1  T: ");
    Serial.print(board.RHT1.temperature());
    Serial.print("\t | RH: ");
    Serial.print(board.RHT1.humidity());
}

// Reads and prints ENS21x sensor data
void readENS21x() {
    Serial.print("\t\tENS210  T: ");
    Serial.print(board.ENS21x.temperature());
    Serial.print("\t | RH: ");
    Serial.print(board.ENS21x.humidity());
}

/**
 * Handles button presses and updates LED/state accordingly.
 * Button A: Toggles continuous display.
 * Button B: Forces immediate re-measurement.
 * Button C: Toggles power and updates LED.
 */
void buttonActivity() {
    // Button A: Toggle continuous display
    if (board.buttonA_Status.changed) {
        board.buttonA_Status.changed = false;
        if (board.buttonA_Status.pressed) {
            board.flashLED(BLUE, 100);  // Flash blue LED
            displayCont = !displayCont; // Toggle display state
            board.WS_LED(0, 0, displayCont ? 128 : 0, 2);  // Update LED to reflect state
        }
    }

    // Button B: Force re-measurement
    if (board.buttonB_Status.changed) {
        board.buttonB_Status.changed = false;
        if (board.buttonB_Status.pressed) {
            board.flashLED(RED, 100);  // Flash red LED
            readRHT1();               // Read and print RHT1 data
            Serial.print("\n");
            readENS21x();              // Read and print ENS21x data
            Serial.print("\n");
        }
    }

    // Button C: Toggle power and update LED
    if (board.buttonC_Status.changed) {
        board.buttonC_Status.changed = false;
        if (board.buttonC_Status.pressed) {
            board.flashLED(GREEN, 100);  // Flash green LED
            board.powerSwitch(!board.powerSwitch());
            // Update LED: red if power is on, green if off
            board.WS_LED(board.powerSwitch() ? 128 : 0, board.powerSwitch() ? 0 : 128, 0, 0);
        }
    }
}

// Interrupt service routine for button state changes
ISR(PCINT2_vect) {
    board.handleButtonInterrupt();  // Handle button interrupt
}