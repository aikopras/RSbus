#ifndef support_isr_h
#define support_isr_h
//************************************************************************************************
//
// file:      support_isr.h
// purpose:   Support file for the RS-bus library.
// history:   2019-01-30 ap V0.1 Initial version
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
//
//************************************************************************************************
#include <Arduino.h>

// declare the RS-bus Interrupt Service Routine. 
void rs_interrupt(void);


// Define the RSbusIsr class
class RSbusIsr {
  public:                                   // All attributes are used by the ISR => Volatile
    RSbusIsr(void);                         // Constructor: all attributes get default value 0
    volatile uint8_t address2use;           // Address to use for the next message (data2send) 
    volatile uint8_t addressPolled;         // Address of RS-bus slave that is polled now 
    volatile uint8_t data2send;             // Actual data byte that will be send over the RS-bus
    volatile uint8_t data2sendFlag;         // Flag: feedback module wants to send data
    volatile unsigned long tLastInterrupt;  // Time in msec last RS-bus puls was received
};

#endif
