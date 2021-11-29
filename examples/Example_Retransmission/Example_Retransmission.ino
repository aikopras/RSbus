//******************************************************************************************************
// 
// Example for the Arduino RS-Bus library.
// This example sets 'rsbus.retransmissions', to enable the retransmission of feedback messages.
// Reasonable values for 'rsbus.retransmissions' are 1 or 2. Retransmission takes place always,
// irrespective if a transmission error was signalled by the RSbus master or not.
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
const uint8_t Retransmissions = 1;   // Reasonable values: 0 (=no retransmission) / 1 or 2.

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
  rsbus.retransmissions = Retransmissions;
}


void loop() {
  // For testing purposes we try every two second to send 8 bits (2 message)
  if ((millis() - T_last) > 2000) {
    T_last = millis();  
    if (rsbusHardware.masterIsSynchronised) {
      digitalWrite(ledPin, HIGH);
      rsbus.send8bits(value);
      if (value == 0) value = 255;
        else value = 0;
    }
    else digitalWrite(ledPin, LOW);  // No RSbus connection
  }
  // Next functions should be called from main as often as possible
  rsbusHardware.checkPolling();     // Listen to RS-Bus polling messages for our turn
  rsbus.checkConnection();          // Check if the buffer contains data, and give this to the ISR and USART
}
