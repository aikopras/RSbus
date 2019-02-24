
//******************************************************************************************************
//
// file:      support_isr.cpp
// purpose:   Support file for the RS-bus library. 
//            Defines the Interrupt Service routine (ISR) that counts the polling pulses transmitted by
//            the master. Once this decoder is polled, the ISR can send data back via its USART. 
// history:   2019-01-30 ap V0.1 Initial version
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
// The rs_interrupt() ISR is called whenever a transistion (falling) is detected on the RS-bus.
// Such transistion indicates that the next feedback decoder is allowed to send information.
// To determine which decoder has its turn, the ISR increments at each transition the
// "addressPolled" variable. 
// If data is made available to the ISR (the "data2sendFlag" is set and the data has been entered into
// the "data2send" variable), the data will be send once the "addressPolled" variable matches the 
// address ("address2use") of this decoder (with offset 1),
// The ISR will also update the "tLastInterrupt" to allow detection of the silence period preceeding
// each new polling cyclus.
//
//******************************************************************************************************
#include <Arduino.h>
#include "support_isr.h"
#include "support_usart.h"

// Instantiate the rsISR object from the RSbusIsr class. 
volatile RSbusIsr rsISR;         // Volatile, since the ISR changes the attributes

// The rsUSART object is declared in "support_usart.cpp" and sends messages to the master. 
extern USART rsUSART;            // We will only use the init() methoud

   
RSbusIsr::RSbusIsr(void) {       // Define the constructor
  addressPolled = 0;             // Start with any address; the first polling cyclus will not be used
  data2send = 0;                 // Empty our send data byte
  data2sendFlag = 0;             // No, we don't have anything to send yet
}


//******************************************************************************************************
// Define the Interrupt Service routines (ISR) for the RS-bus
//
//******************************************************************************************************
void rs_interrupt(void) {
  if (rsISR.data2sendFlag) {
    if (rsISR.address2use == rsISR.addressPolled) {    
      // We have data to send, it is our turn and the decoder is synchronised
      // Note: general USART code often includes some kind of flow control, but that is not needed here
      (*rsUSART.dataRegister) = rsISR.data2send;
      rsISR.data2sendFlag = 0;
    } 
    else if (rsISR.address2use > 128) { rsISR.data2sendFlag = 0; } // drop data for impossible addresses
  }
  rsISR.tLastInterrupt = micros();     // Update the timer
  rsISR.addressPolled ++;              // Address of slave that gets his turn next
}
