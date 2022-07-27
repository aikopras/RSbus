//************************************************************************************************
//
// file:      sup_isr.h
// purpose:   Support file for the RS-bus library.
// history:   2019-01-30 ap V0.1 Initial version
//            2021-09-25 ap V1.0 Production version
//            2022-07-27 ap V1.1 millis() replaced by micros()
//
// This source file is subject of the GNU general public license 3,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
//************************************************************************************************
#pragma once
#include <Arduino.h>

// declare the RS-bus Interrupt Service Routine. 
void rs_interrupt(void);


// Define the RSbusIsr class
class RSbusIsr {
  public:                                   // All attributes modified by the ISR => Volatile
    volatile bool data2sendFlag;            // Flag set by main sketch that there is data to send
    uint8_t data2send;                      // Actual data byte that will be send over the RS-bus
    uint8_t address2use;                    // Address to use for the next message (data2send)

    // -------------------------------------------------------------------------------------------
    // The variables defined below are for internal use between checkPolling() and the ISR,
    // and must be defined as public to allow access by the ISR(s)
    volatile bool data4IsrFlag;             // Flag set by checkPolling for the ISR that data is ready
    volatile bool dataWasSendFlag;          // Flag between the ISR and CheckPolling
    volatile bool flagPulseCount;           // Retransmit after a pulse count error?
    volatile bool flagParity;               // Will we retransmit after a parity error?

    volatile uint8_t timeIdle;              // How long is the command station idle (volatile: reset by 4ms ISR)
    unsigned long tLastCheck;               // Time in microsec
    uint16_t lastPulseCnt;                  // Previous value of the silence counter

    // Specific for the software based ISRs (pulse count is performed in software within the ISR)
    volatile uint8_t addressPolled;         // Address of RS-bus slave that is polled now
  
    // Specific for sup_isr_hw_tcb.cpp
    uint8_t ccmpValue;                      // To reinitialise the CNT register of the Compare Match ISR

    // Specific for sup_isr_sw_4ms.cpp
    unsigned long tLastInterrupt;           // Time in msec, used by checkPolling()

    RSbusIsr(void);                         // Constructor: all attributes get default value 0
};
