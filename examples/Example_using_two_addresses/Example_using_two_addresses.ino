//******************************************************************************************************
//
// Test sketch for the Arduino RS-Bus library
// In this sketch we use two RS-Bus addresses
//
// We send every second a 4 bit value (nibble), using two RS-Bus addresses.
// If there is no connection yet / anymore with the RS_bus master, we send 8 bits
// to establish such connection.
//
// 2019-02-18 / AP: Initial version
// 2021-07-17 / AP: Typo (harware => hardware)
//                  USART selection no longer based on Arduino Pin, but on USART number
//
//******************************************************************************************************
#include <Arduino.h>
#include <RSbus.h>

// The following parameters specify the hardware that is being used and the RS-Bus addresses
const uint8_t ledPin = 8;            // Pin for the LED. Usually Pin 13
const uint8_t RsBus_USART = 0;       // Use USART-0. On most boards TX, TXD, TX0 or TXD0
const uint8_t RsBus_RX = 10;         // INTx: one of the interrupt pins (pin 2 or 3)
const uint8_t RS_Address1 = 98;      // Must be a value between 1..128
const uint8_t RS_Address2 = 99;      // Must be a value between 1..128


// Instatiate the objects being used
// The RSbushardware object is responsible for the RS-Bus ISR and USART.
// The RSbusConnection object maintains the data channel with the master and formats the messages 
extern RSbusHardware rsbus_hardware; // This object is defined in rs_bus.cpp
RSbusConnection rsbus1;              // Per RS-Bus address we need a dedicated object
RSbusConnection rsbus2;              // We use two RS-Bus addresses
unsigned long T_last;                // For testing: we send 1 message per second
uint8_t value;                       // The value we will send over the RS-Bus 
uint8_t nibble;                      // HighBits or LowBits
uint8_t nubbleNumber;                // We send the four nibbles one after the other


void setup() {
  pinMode(ledPin, OUTPUT);
  rsbus_hardware.attach(RsBus_USART, RsBus_RX);
  rsbus1.address = RS_Address1;      // 1.. 128
  rsbus2.address = RS_Address2;      // 1.. 128
  nubbleNumber = 1;
  value = 1;
}


void loop() {
  // For testing purposes we try every second to send 8 bits (2 messages)
  if ((millis() - T_last) > 1000) {
    T_last = millis();
    if (rsbus_hardware.masterIsSynchronised) {
      digitalWrite(ledPin, 1);
      if (nubbleNumber == 1) rsbus1.send4bits(LowBits, value);
      if (nubbleNumber == 2) rsbus2.send4bits(LowBits, value);
      if (nubbleNumber == 3) rsbus1.send4bits(HighBits, value);
      if (nubbleNumber == 4) rsbus2.send4bits(HighBits, value);
    }
    else {
      digitalWrite(ledPin, 0);
      nubbleNumber = 1;
      value = 1;
    }
    // Update value and nibble
    nubbleNumber ++;  
    if (nubbleNumber == 5) {
      nubbleNumber = 1;    
      switch (value) {
        case 0   : value = 1;   break;
        case 1   : value = 2;   break;
        case 2   : value = 4;   break;
        case 4   : value = 8;  break;
        case 8   : value = 0;  break;
      }
    }
  }
  // Next functions should be called from main as often as possible
  // If a RS-Bus feedback decoder starts, it needs to connect to the master station
  if (rsbus1.needConnect) rsbus1.send8bits(0);
  if (rsbus2.needConnect) rsbus2.send8bits(0);
  rsbus_hardware.checkPolling();     // Listen to RS-Bus polling messages for our turn
  rsbus1.checkConnection();          // Check if the buffer contains data, and give this to the ISR and USART
  rsbus2.checkConnection();          // Check buffer for second address
}
