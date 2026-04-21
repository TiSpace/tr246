/* *********************************************************************************
 * @brief  Serial communication handler
 * @param  none
 * @return void
 * @note   Checks for incoming serial data, parses the command character,
 *         and dispatches to the appropriate action.
 * *********************************************************************************/
void runCommunication() {

    if (Serial.available()) {
        // Read one complete line (terminated by newline) and strip surrounding whitespace
        String line = Serial.readStringUntil('\n');
        line.trim();

        // Dispatch on the first character of the received command
        switch (line.charAt(0)) {

            // ── 'p' : change the pause interval between measurements ──────────────
            case 'p':
                {
                    // Show the current pause value and request a new one
                    Serial.print(F("Pause between measurements: "));
                    Serial.print(board.getMeasurementPause());
                    Serial.print(F(" ms  New value? "));
                    board.setMeasurementPause(readIntegerFromSerial()) ;
                    Serial.print("\n");

                 
                    break;
                }

            // ── 'f' : cycle through measurement print formats ─────────────────────
            case 'f':
                // measurementPrint++;
                // if (measurementPrint > 1) measurementPrint = 0;  // Wrap back to format 0
                // EEPROM.write(EEPROM_ADDRESS_MEASPRT, measurementPrint);  // Save selection
                 break;

            // ── '?' : display the help menu ───────────────────────────────────────
            case '?':
              //  printMenue();
                break;
        }

        Serial.println();  // Blank line after any command for readability
    }
}


/* *********************************************************************************
 * @brief  Flush all pending bytes from the serial receive buffer
 * @param  none
 * @return void
 * @note   Call this to discard stale input, e.g. after a mode change.
 * *********************************************************************************/
void clearSerialBuffer() {
    while (Serial.available() > 0) {
        Serial.read();  // Read and discard each byte until the buffer is empty
    }
}


/* *********************************************************************************
 * @brief  Print the serial command menu to the console
 * @param  none
 * @return void
 * *********************************************************************************/
void printMenue() {
    Serial.println(F("\n#############################################"));
    Serial.println(F("\n\tMenu:"));
    Serial.println(F("\tf - toggle output format"));
    Serial.println(F("\tp - set pause between measurements"));
    Serial.println(F("\t? - show this menu"));
    Serial.println(F("\n#############################################\n"));
}


/* *********************************************************************************
 * @brief  Read a decimal integer from the serial input
 * @param  none
 * @return int   The parsed integer value, or -1 if no valid input was received.
 * @note   Blocks until a carriage-return character ('\r') is received.
 *         Non-digit characters are rejected and the partial input is cleared.
 * *********************************************************************************/
int readIntegerFromSerial() {
    String input = "";  // Accumulate digit characters here

    while (true) {
        if (Serial.available()) {
            char c = Serial.read();

            // Carriage return signals end-of-input
            if (c == '\r') {
                if (input.length() > 0) {
                    return input.toInt();  // Return the successfully parsed integer
                } else {
                    Serial.println("No input received.");
                    return 0;
                    //return -1;  // Error: nothing was entered
                }
            }

            // Accept only digit characters; reject everything else
            if (isDigit(c)) {
                input += c;
                Serial.print(c);  // Echo the accepted digit back to the terminal
            } else {
                Serial.println("\nInvalid character. Only digits are allowed.");
                input = "";  // Discard any partially entered input
            }
        }
    }
}
