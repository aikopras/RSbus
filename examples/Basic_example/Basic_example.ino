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
// 2021-07-26 / AP: Has been tested on MegaCoreX and DxCore boards
// 2021-10-25 / AP: Support added for different RS-bus decoders (4ms, RTC, TCB). 
//
// RsBus_USART
// ===========
// The USART number, starting from 0.
// Many of the `old` ATMega processors support a single USART only. Examples of such
// processors are the 8535, 16 and the 328 (the latter is used in the Arduino UNO and Nano).
// More powerful processors, such as the 2560 (used in the Arduino Mega) and newer
// processors such as the 4809 (used in the Arduino Every) support 4 or even more USARTS.
// Below an overview of (some) processors that support multiple USARTs.
// - 2 USARTS: 64, 128, 162, 164, 324, 328PB, 644, 1281, 1284, 2561
// - 4 USARTS: 640, 1280, 2560, 4808, 4809
// - 6 USARTS: AVR-DA, AVR-DB
// Note that the first USART on Arduino boards may also be used for the serial monitor.
// On such boards combined use of both RS-Bus and serial monitor may cause problems.
//
// RsBus_RX
// ========
// The Arduino pin number.
// The `old` ATMega processors have a limited number of pins that can be used for interrupts:
//  Interrupt   Port   Pin   Where 
//    INT0      PD2      2   All standard Arduino boards
//    INT1      PD3      3   All standard Arduino boards
//    INT0      PD0     21   MEGA
//    INT1      PD1     20   MEGA
//    INT2      PD2     19   MEGA
//    INT3      PD3     18   MEGA
//    INT4      PE4      2   MEGA
//    INT5      PE5      3   MEGA
//    INT0      PD2     10   Mightycore - ATmega 8535/16/32/164/324/644/1284
//    INT1      PD3     11   Mightycore - ATmega 8535/16/32/164/324/644/1284
//    INT2      PB2      2   Mightycore - ATmega 8535/16/32/164/324/644/1284
//
// The newer MegaCoreX and DxCore processors can use basically every pin to generate interrupts.
// On these newer processors more efficient RS-bus decoding can be obtained by using the RTC or TCB
// decoding variant. If RTC-based decoding is used, pin PA0 must (!) be used. 
// For details of the variants, see the file RSbusVariants.h in the src directory of the RS-bus library.
//
// RS_Address
// ==========
// The RS-Bus address, which should be in the range between 1 and 128.
//
//******************************************************************************************************
#include <Arduino.h>
#include <RSbus.h>

// The following parameters specify the hardware that is being used and the RS-Bus addresses (see above)
const uint8_t RsBus_USART = 0;       // USART number: 0, 1, ...5 (depending on processor / board)
const uint8_t RsBus_RX = 2;          // INTx: see above
const uint8_t RS_Address = 112;      // Must be a value between 1..128


// Instatiate the objects being used
// The RSbushardware object is responsible for the RS-Bus ISR and USART.
// The RSbusConnection object maintains the data channel with the master and formats the messages 
extern RSbusHardware rsbusHardware;  // This object is defined in rs_bus.cpp
RSbusConnection rsbus;               // Per RS-Bus address we need a dedicated object
unsigned long T_last;                // For testing: we send 1 message per second
uint8_t value = 0;                   // The value we will send over the RS-Bus 


void setup() {
  rsbusHardware.attach(RsBus_USART, RsBus_RX);
  rsbus.address = RS_Address;        // 1.. 128
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
  rsbusHardware.checkPolling();      // Listen to RS-Bus polling messages for our turn
  rsbus.checkConnection();           // Check if the buffer contains data, and give this to the ISR and USART
}
