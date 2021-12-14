//******************************************************************************************************
// 
// Example for the Arduino RS-Bus library.
// This example sets 'rsbus.forwardErrorCorrection', to enable multiple transmissions of the same 
// feedback message (forward error correction).
// Reasonable values for 'rsbus.forwardErrorCorrection' are:
// - 0: feedback data is send once, 
// - 1: after the data is send, 1 copy of the same data is send again, or
// - 2: after the data is send, 2 copies of the same data are send again. 
// Forward error correction should not be confused with retransmission. 
// Forward error correction is proactive, in the sense that it anticipates that messages may get lost
// by sending the same message again, irrespective if a transmission error was signalled
// by the RSbus master or not.
// Retransmission is reactive, and sends a copy only after a transmission error was signalled
// by the RSbus master
//
// We send every second an 8 bit value. The values will be 0 or 255 
// The library will devide these 8 bits into two RS-Bus messages.
// First the low-level nibble is send, followed by the high level nibble (a nibble carries 4 bits).
//
// 2021-11-30 / AP: Initial version (Tested on Arduino UNO with Arduino UNO DCC Shield)
//
//******************************************************************************************************
#include <Arduino.h>
#include <RSbus.h>

// The following parameters specify the hardware that is being used and the RS-Bus address
const uint8_t ledPin = 4;            // Pin for the LED. Usually Pin 13
const uint8_t RsBus_USART = 0;       // Use USART-0. On most boards TX, TXD, TX0 or TXD0
const uint8_t RsBus_RX = 2;          // INTx: Arduino UNO DCC Shield is Pin 2
const uint8_t RS_Address = 112;      // Must be a value between 1..128
const uint8_t data_copies = 1;       // Reasonable values: 0 (=no copies) / 1 or 2.

// Instatiate the objects being used
// The RSbushardware object is responsible for the RS-Bus interrupts and USART.
extern RSbusHardware rsbusHardware;  // This object is defined in rs_bus.cpp
RSbusConnection rsbus;               // Per RS-Bus address we need a dedicated object
unsigned long T_last;                // For testing: we send 1 message per second
uint8_t value;                       // The value we will send over the RS-Bus 



//**************************************** Main ******************************************* 
void setup() {
  rsbusHardware.attach(RsBus_USART, RsBus_RX);
  rsbus.address = RS_Address;        // 1.. 128
  value = 0;                         // Initial value
  pinMode(ledPin, OUTPUT); 
  rsbus.forwardErrorCorrection = data_copies;
}


void loop() {
  // For testing purposes we try every two second to send 8 bits (2 message)
  if ((millis() - T_last) > 2000) {
    T_last = millis();  
    if (rsbusHardware.rsSignalIsOK) {
      digitalWrite(ledPin, HIGH);
      rsbus.send8bits(value);
      if (value == 0) value = 255;
        else value = 0;
    }
    else digitalWrite(ledPin, LOW);  // No RSbus connection
  }
  // Next functions should be called from main as often as possible
  // If a RS-Bus feedback decoder starts, or after certain errors,
  // it needs to send its feedback data to the master station
  if (rsbus.feedbackRequested) rsbus.send8bits(value);
  rsbusHardware.checkPolling();     // Listen to RS-Bus polling messages for our turn
  rsbus.checkConnection();          // Check if the buffer contains data, and give this to the ISR and USART
}
