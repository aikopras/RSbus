//******************************************************************************************************
//
// Basic example for the Arduino RS-Bus library.
//
// We send every second an 8 bit value. The values will be 0, 1, 2, 4, 8, 16, 32, 64, 128 
// The library will devide these 8 bits into two RS-Bus messages.
// First the low-level nibble is send, followed by the high level nibble (a nibble carries 4 bits).
//
// 2019-02-18 / AP: Initial version
// 2021-07-17 / AP: Typo (harware => hardware)
//                  USART selection no longer based on Arduino Pin, but on USART number
//
//******************************************************************************************************
#include <Arduino.h>
#include <RSbus.h>

// The following parameters specify the hardware that is being used and the RS-Bus addresses
const uint8_t RsBus_USART = 0;       // Use USART-0. On most boards TX, TXD, TX0 or TXD0
const uint8_t RsBus_RX = 10;         // INTx: one of the interrupt pins
const uint8_t RS_Address = 100;      // Must be a value between 1..128


// Instatiate the objects being used
// The RSbushardware object is responsible for the RS-Bus ISR and USART.
// The RSbusConnection object maintains the data channel with the master and formats the messages 
extern RSbusHardware rsbus_hardware; // This object is defined in rs_bus.cpp
RSbusConnection rsbus;               // Per RS-Bus address we need a dedicated object
unsigned long T_last;                // For testing: we send 1 message per second
uint8_t value;                       // The value we will send over the RS-Bus 


void setup() {
  rsbus_hardware.attach(RsBus_USART, RsBus_RX);
  rsbus.address = 100;               // 1.. 128
  // If a RS-Bus feedback decoder starts, it needs to connect to the master station
  if (rsbus.needConnect) rsbus.send8bits(0);
  value = 0;                         // Initial value
}


void loop() {
  // For testing purposes we send every second 8 bits (2 messages)
  if ((millis() - T_last) > 1000) {
    T_last = millis();
    rsbus.send8bits(value);          // Tell the library to buffer these 8 bits for later sending
    switch (value) {
      case 0   : value = 1;   break;
      case 1   : value = 2;   break;
      case 2   : value = 4;   break;
      case 4   : value = 8;   break;
      case 8   : value = 16;  break;
      case 16  : value = 32;  break;
      case 32  : value = 64;  break;
      case 64  : value = 128; break;
      case 128 : value = 1;   break;
     }
  }
  // Next functions should be called from main as often as possible
  // If a RS-Bus feedback decoder starts, it needs to connect to the master station
  if (rsbus.needConnect) rsbus.send8bits(0);
  rsbus_hardware.checkPolling();     // Listen to RS-Bus polling messages for our turn
  rsbus.checkConnection();           // Check if the buffer contains data, and give this to the ISR and USART
}
