//******************************************************************************************************
//
// file:      rsbus.cpp
//
// purpose:   Library to send feedback data back via the RS-bus
//
// This source file is subject of the GNU general public license 3,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
// history:   2019-02-10 ap V0.1 Initial version
//            2021-07-26 ap v0.2 Default type is now 'Feedback decoder'
//            2021-09-30 ap v1.0 Different types of hardware are supported
//            2021-11-29 ap v2.1 Forward Error Correction added (irrespective of transmission errors)
//
//
//
//  Ralation between the files:
//                                      +-----------------------+
//                                      |                       |
//                                      |         Main          |
//                                      |                       |
//                                      +-----------+-----------+
//                                                  |
//                                                  V
//                                      +-----------+-----------+
//                                      |                       |
//                  +-------------------+         RSBUS         |
//                  |                   |                       |
//                  |                   +-----------+-----------+
//                  |                               |
//                  V                               V
//      +-----------+-----------+       +-----------+-----------+       +-----------------------+
//      |                       |       |                       +-+     |                       |
//      |         FIFO          |       |         ISR_*         +-|---->|         USART         |
//      |                       |       |                       | +---->|                       |
//      +-----------------------+       +-+---------------------+ |     |                       |
//                                        +-----------------------+     +-----------------------+
//                      
//
//******************************************************************************************************
#include <Arduino.h>
#include <util/parity.h>               // Needed to calculate the parity bit
#include "RSbus.h"
#include "sup_isr.h"
#include "sup_fifo.h"


//******************************************************************************************************
// The following object must be visible for the main sketch. This object is declared here,
// thus the main sketch doesn't need to bother about declaring this necessary object itself.
// In addition, since this object is defined here, there can only be a single instantiation
// of the RSbusHardware class, and thus a single RSbusHardware interface per decoder.
RSbusHardware rsbusHardware;    // Interface to the main sketch for accessory commands

// The following object should NOT be used by the main sketch, but is instead used by this
// c++ file as interface to the various support files.
volatile RSbusIsr rsISR;        // Interface to sup_isr*



//******************************************************************************************************
// The RSbusConnection class is needed for establishing a connection to the master station and
// sending RS-bus messages. Per address a separate connection is needed.
//******************************************************************************************************
// Define the location of the various bits within a RS-bus message
const uint8_t DATA_0    = 7;           // feedback 1 or 5
const uint8_t DATA_1    = 6;           // feedback 2 or 6
const uint8_t DATA_2    = 5;           // feedback 3 or 7
const uint8_t DATA_3    = 4;           // feedback 4 or 8
const uint8_t NIBBLEBIT = 3;           // low or high order nibble
const uint8_t TT_BIT_0  = 2;           // this bit must always be 0
const uint8_t TT_BIT_1  = 1;           // this bit must always be 1
const uint8_t PARITY    = 0;           // parity bit; will be calculated by software
  

RSbusConnection::RSbusConnection() {
  // The following kind of RS-bus modules exist (see also http://www.der-moba.de/):
  // - 0: accessory decoder without feedback
  // - 1: accessory decoder with RS-Bus feedback
  // - 2: feedback module for the RS-Bus (Default)
  // - 3: Reserved for future use
  // The 'type' is also conveyed in XpressNet response messages, and used for example
  // by handhelds to indicate if a switch has feedback capabilities or if the feedback
  // decoder is connected to the master station.
  address = 0;                                 // Initialise to 0
  type = Feedback;                             // Default value
  status = notSynchronised;                    // state machine starts notSynchronised
  feedbackRequested = false;                   // Initialise to false
  forwardErrorCorrection = 0;                  // Default: no forward error correction
}


void RSbusConnection::format_nibble(uint8_t value) {
  // Input is a byte, representing 4 feedback bits, plus one nibble bit
  // 1) the routine sets the TT and parity bits
  // 2) it stores the formatted byte in the FIFO
  // Define the bits of the RS-bus `packet' (1 byte)
  // Note: least significant bit (LSB) first. Thus the parity bit comes first,
  // immediately after the USART's start bit. Because of this (unusual) order, the USART hardware
  // can not calculate the parity bit itself; such calculation must be done in software.
  unsigned char parity;
  // Step 1A: set the `type' bit
  if (type == Switch)   {value |= (1<<TT_BIT_0) | (0<<TT_BIT_1);}  // switch decoder with feedback
  if (type == Feedback) {value |= (0<<TT_BIT_0) | (1<<TT_BIT_1);}  // feedback module
  // Step 1B: Set the parity bit
  parity = parity_even_bit(value);              // check parity
  if (parity)                                   // if parity is even
    {value |= (0<<PARITY);}                     // clear the parity bit
    else {value |= (1<<PARITY);}                // set the parity bit
  // Step 2: store the formatted `data byte' into the FIFO queue
  // Data in this queue will later be send (using the USART) from send_nibble(). 
  my_fifo.push(value);
}


void RSbusConnection::send4bits(Nibble_t nibble, uint8_t value) {
  // Takes a 4 bit value to create a single RS-bus nibble and stores this nibble in a FIFO 
  // This nibble is later send to the RS-bus master station using sendnibble() 
  // nibble = 1..2, specifies if we use the first or the second nibble
  // data   = 0..15  
  uint8_t data;
  uint8_t nibbleValue;
  if (nibble == LowBits) nibbleValue = 0;
    else nibbleValue = 1;
  data = ((value & 0b00000001) <<7)  // move bit 7 to bit 0 (distance = 7)
       | ((value & 0b00000010) <<5)  // move bit 6 to bit 1 (distance = 5)
       | ((value & 0b00000100) <<3)  // move bit 5 to bit 2 (distance = 3)
       | ((value & 0b00001000) <<1)  // move bit 4 to bit 3 (distance = 1)
       | (nibbleValue<<NIBBLEBIT);
  // If data should be send multiple times, store the same nibble multiple times
  for (uint8_t i = 0; i <= forwardErrorCorrection; i++) {format_nibble(data);}
}

    
void RSbusConnection::send8bits(uint8_t value) {
  // Takes an 8 bit value to create two RS-bus nibbles and stores these nibbles in a FIFO 
  // These nibbles will later be send to the RS-bus master station using sendnibble() 
  // data = 0..255  
  // Note that bit order should be changed.
  const uint8_t NIBBLEBIT = 3;       // low or high order nibble
  uint8_t dataNibble1;
  uint8_t dataNibble2;
// Sending 8 bits is sufficient to connect to the master
  feedbackRequested = false;
  // send first nibble (for the low order bits
  dataNibble1 = ((value & 0b00000001) <<7)  // move bit 7 to bit 0 (distance = 7)
              | ((value & 0b00000010) <<5)  // move bit 6 to bit 1 (distance = 5)
              | ((value & 0b00000100) <<3)  // move bit 5 to bit 2 (distance = 3)
              | ((value & 0b00001000) <<1)  // move bit 4 to bit 3 (distance = 1)
              | (0<<NIBBLEBIT);
  // send second nibble (for the high order bits)
  dataNibble2 = ((value & 0b00010000) <<3)  // move bit 3 to bit 0 (distance = 3)
              | ((value & 0b00100000) <<1)  // move bit 2 to bit 1 (distance = 1)
              | ((value & 0b01000000) >>1)  // move bit 1 to bit 2 (distance = -1)
              | ((value & 0b10000000) >>3)  // move bit 0 to bit 3 (distance = -3)
              | (1<<NIBBLEBIT);
  // If data should be send multiple times, store the same nibbles multiple times
  for (uint8_t i = 0; i <= forwardErrorCorrection; i++) {
    format_nibble(dataNibble1);
    format_nibble(dataNibble2);
  }
}


//******************************************************************************************************
uint8_t RSbusConnection::sendNibble(void) {
  // Perform the checks here, since this part is less time critical then the ISR part, which get its input from here
  uint8_t result = 0;                          // Function return value
  if (my_fifo.size() > 0) {                    // We have data to send
    if (rsISR.data2sendFlag == false) {        // And the USART (rs_interrupt routine) can accept new data
      if ((address > 0) && (address <= 128)) { // And the address for this connection has been initialized and is valid
        rsISR.address2use = address;           // Use the address that belongs to this connection
        rsISR.data2send = my_fifo.pop();       // Take the oldest element from the FIFO
        rsISR.data2sendFlag = true;            // Tell the rs_interrupt routine that it should send data
        result = 1;                            // Succesfully presented the nibble to the rs_interrupt routine
      }
    }
  }   
  return result; 
}


void RSbusConnection::checkConnection(void) {
  // This function maintains the statemachine for the RS-bus connection, and should be called from main frequestly
  if (rsbusHardware.rsSignalIsOK) {            // The decoder has received a polling cyclus (without errors)
    switch (status) {
      case notSynchronised :
        status = feedbackIsNeeded;             // status is used for our internal (private) statemachine
        feedbackRequested = true;              // used as external (public) flag towards main() and send8bits()
        break;
      case feedbackIsNeeded:
        if (feedbackRequested == false) status = feedbackNibble1;
        break;
      case feedbackNibble1:
        if (RSbusConnection::sendNibble()) status = feedbackNibble2;
        break;
      case feedbackNibble2:
        if (RSbusConnection::sendNibble()) status = connected;
        break;
      case connected:
        RSbusConnection::sendNibble();
        break;
    }
  }
  else {
    status = notSynchronised;                  // No RS-bus signal, or count / parity errors are detected
    my_fifo.empty();                           // Drop all data that is still waiting in the FIFO for transmission
    rsISR.data2sendFlag = false;               // Cancel possible data waiting for ISR
  }
}

   
