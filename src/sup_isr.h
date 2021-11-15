//************************************************************************************************
//
// file:      sup_isr.h
// purpose:   Support file for the RS-bus library.
// history:   2019-01-30 ap V0.1 Initial version
//            2021-09-25 ap V1.0 Production version
//
// This source file is subject of the GNU general public license 3,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
//
//************************************************************************************************
#pragma once
#include <Arduino.h>

// declare the RS-bus Interrupt Service Routine. 
void rs_interrupt(void);


// Define the RSbusIsr class
class RSbusIsr {
  public:                                   // All attributes are used by the ISR => Volatile
    volatile bool data2sendFlag;            // Flag: feedback module wants to send data
    uint8_t data2send;                      // Actual data byte that will be send over the RS-bus
    uint8_t address2use;                    // Address to use for the next message (data2send) 

    // For internal use by the software polling ISR
    volatile uint8_t addressPolled;         // Address of RS-bus slave that is polled now

    // For internal use by checkPolling, to determine the length of the silence period
    volatile uint8_t timeIdle;              // checkPolling(): how long is the command station idle
    uint16_t lastPulseCnt;                  // checkPolling(): Previous value of the silence counter
    unsigned long tLastCheck;               // checkPolling(): Time in msec 

    // Specific for sup_isr_tcb.cpp
    volatile boolean data4usartFlag;        // Flag between checkPolling and ISR
    uint8_t ccmpValue;                      // To reinitialise the CNT register of the Compare Match ISR

    // Specific for sup_isr_sw_4ms.cpp
    unsigned long tLastInterrupt;           // Time in msec, used by checkPolling()

    RSbusIsr(void);                         // Constructor: all attributes get default value 0
};
