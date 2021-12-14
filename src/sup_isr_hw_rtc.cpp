//******************************************************************************************************
//
// file:      sup_isr_hw_rtc.cpp
// purpose:   Support file for the RS-bus library. 
//            Defines the Interrupt Service routine (ISR) that counts the polling pulses transmitted by
//            the master. Once this decoder is polled, the ISR can send data back via its USART.
//            Uses the Real Time Counter (RTC) of MegaCoreX and DxCore processors.
// history:   2021-10-10 ap V1.0 initial version
//
// This source file is subject of the GNU general public license 3,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
// If data is available for sending, the data should be entered  into the "data2send" variable and
// the "data2sendFlag" should be set.
// A counter (RTC.CNT) counts the number of RS-bus pulses and once the counter value matches the
// compare (RTC.CMP) value, an interrupt is raised and the data will be send.
//
// RTC Introduction
// ================
// The RTC has two modes: Real-Time Counter (RTC) and Periodic Interrupt Timer (PIT).
// Both modes use the same clock source (CLK_RTC), which is either an external 32Khz X-tal or 
// pulses on the EXTCLK pin (PA0).
//
// For DCC decoders all modes with an external (aynchronous) 32Khz clock don't seem very
// useful. However, the ability to count pulses from the EXTCLK pin may be interesting for
// RS-Bus operation. 
// 
// RTC Mode - Pulse Counter
// ========================
// One of the RTC mode options is to count input pulses on the EXTCLK input pin (PA0). 
// The number of pulses is stored in a Counter (CNT) register and compared to the content of the
// Period (PER) register and Compare (CMP) register. The RTC can generate interrupts on compare 
// match and/or overflow. It generates the compare interrupt at the first count after the counter 
// equals the Compare register value, and an overflow interrupt at the first count after the 
// counter value equals the Period register value. The overflow will reset the counter value
// to zero.
// 
// In this mode the RTC can count RS-Bus pulses and generate an interrupt whenever the number
// of pulses matches the RS-Bus address (Compare Match). Since the counter automatically resets
// after an overflow, the last pulse of each RS-bus polling cycle should be used to trigger the 
// overflow interrupt.
//
// The RS-Bus should be connected to the EXTCLK input pin, which is PA0.
// Unfortunately the RTC can not be configured as Event User, which means that other pins are
// not possible. Since only positive edges are counted, the RS-Bus signal must be inverted.
//
// Initialising the Overflow Interrupt
// ===================================
// The overflow should be triggered by the last puls of a RS-bus polling cycle.
// To ensure that in this last cycle the counter value (CNT) matches the overflow value (PER),
// the counter value must be initialised to an appropriate value in the silence period between
// two polling cycles.
// At first sight a good solution seems to set in this silence period the counter value to zero.
// Unfortunately that doesn't work, since (as stated in the datasheet) "due to the
// synchronization between the RTC clock and main clock domains, there is a latency of two RTC
// clock cycles from updating the register until this has an effect". 
// The right solution therefore is to set the counter to 3 (to compensate for the synchronization
// between both clock domains. In this way the overflow will be triggered at the end of each
// polling cycle.
// 
// Initialisation is needed after start-up or after the RS-bus signal was lost and reappears.
// To determine if initialisation is needed, checks are performed during the silence period
// if the counter value matches zero. If this is not the case, a new initialisation takes place.
//
// RS-Bus address
// ==============
// The RS-bus address (RTC.CMP) can be changed in the CMP match ISR. Addresses between 3 and 128
// become active in the next polling cycle. Since register changes take effect only after 2
// RTC clock transistions, the addresses 1 and 2 will not be active in the next polling cycle,
// but only in a subsequent polling cycle after the next polling cycle has been completed. 
// 
// RS-Bus input signal
// ===================
// The RTC count positive edges only. 
//
// Parity errors
// =============
// If the master station detects a parity error, it will enlarge the silence period 
//
// Pins:
// =====
// - PA0: RS-Bus in (EXTCLK).
// - a transmit pin for the UART
// 
// References:
// ===========
// Microchip manual TB3218
// http://ww1.microchip.com/downloads/en/Appnotes/TB3213-Getting-Started-with-RTC-DS90003213B.pdf
//
//************************************************************************************************
// Timing overview   
//
// 1) Normal operation (no parity errors)
//   _   _   _   _   _   _   _                                       _   _   _   _   _   _
//  | | | | | | | | | | | | | |                                     | | | | | | | | | | | |
//  | |_| |_| |_| |_| |_| |_| |_____________________________________| |_| |_| |_| |_| |_| |_
//                             <-------------- 7ms ------------->
//
//
// 2) Parity error(s)
//   _   _   _   _   _   _   _                                                    _   _  
//  | | | | | | | | | | | | | |                                                  | | | | 
//  | |_| |_| |_| |_| |_| |_| |__________________________________________________| |_| |_
//                             <--------------------- 10,7ms -------------------> 
//
//******************************************************************************************************
#include <Arduino.h>
#include "RSbus.h"
#include "sup_isr.h"
#include "sup_usart.h"

// This code will only be used if we define in RSbusVariants.h the "RSBUS_USES_RTC" directive
#if defined(RSBUS_USES_RTC)


//******************************************************************************************************
// The following objects are instantiated elsewhere, but are used here
extern RSbusHardware rsbusHardware;  // instantiated in "RS-bus.cpp"
extern volatile RSbusIsr rsISR;      // instantiated in "RS-bus.cpp"
extern USART rsUSART;                // instantiated in "sup_usart.cpp" We only use init()


//******************************************************************************************************
// RSbusIsr: constructor
//******************************************************************************************************
RSbusIsr::RSbusIsr(void) {       // Define the constructor
  lastPulseCnt = RTC.CNT;        // RTC.CNT value
  data4IsrFlag = false;          // ISR acts on data4IsrFlag, instead of data2sendFlag
  address2use = 0;               // Initialise address to 0
  data2send = 0;                 // Empty our send data byte
  data2sendFlag = false;         // No, we don't have anything to send yet
  dataWasSendFlag = false;       // No, we didn't send anything yet
  flagParity = false;            // No, we don't need to retranmit after a parity error
  flagPulseCount = false;        // No, we don't need to retranmit after a pulse count error
  tLastCheck = millis();         // Current time
}

   
//******************************************************************************************************
//******************************************************************************************************
// The RSbusHardware class is responsible for controlling the RS-bus hardware, thus the RTC that counts
// the polling pulses send by the master, and the USART connected to an output pin to send messages
// to the master.
// Messages are: 8 bit, no parity, 1 stop bit, asynchronous mode, 4800 baud.
// The "attach" method initialises the RTC and USART.
// A detach method is available to disable the RTC, which is needed before a decoder gets restarted.
// Note regarding the code: instead of the "RSbusHardware::attach" method we could have directly used
// the RSbusIsr and USART class constructors. However, since we also need a "RSbusHardware::detach" method
// to stop the rs_interrupt service routine, for symmetry reasons we have decided for an "attach"
// and "detach".

RSbusHardware::RSbusHardware() {                     // Constructor
  rsSignalIsOK = false;                              // No valid RS-bus signal detected yet
  swapUsartPin = false;                              // We use the default USART TX pin
  interruptModeRising = true;                        // Earlier hardware triggered on FALLING
  parityErrors = 0;                                  // Counter for the number of 10,7ms gaps
  pulseCountErrors = 0;                              // Number of times a cycli did not have 130 pulses
  parityErrorHandling = 1;                           // Default: if we have just send data, retransmit!
  pulseCountErrorHandling = 2;                       // Default: always retransmit all feedback data
}


void RSbusHardware::attach(uint8_t usartNumber, uint8_t rxPin) {
  rxPinUsed = rxPin;                                 // included, to avoid compiler warnings
  rxPinUsed = PIN_PA0;                               // PA0 = EXTCLK
  // Step 1: Initialise the RS bus transmission hardware (USART)
  rsUSART.init(usartNumber, !swapUsartPin);
  // Step 2: Initialise the RTC
  while (RTC.STATUS > 0) { ;}                        // Wait for all register to be synchronized
  RTC.CLKSEL = 0;                                    // Reset the main timer control registers
  RTC.CTRLA = 0;
  RTC.INTCTRL = 0;
  // Initialise the control registers data2sendFlag
  while (RTC.STATUS > 0) { ;}                        // Wait for all register to be synchronized
  RTC.CMP = 3;                                       // Start value after silence period
  RTC.PER = 129;                                     // Total number of pulses
  RTC.CLKSEL = RTC_CLKSEL_EXTCLK_gc;                 // Select EXTCLK as clock input
  RTC.CTRLA = RTC_RTCEN_bm;                          // Enable the RTC
  RTC.INTCTRL = RTC_CMP_bm | RTC_OVF_bm;             // Enable Compare Match and Overflow Interrupt    
}


void RSbusHardware::detach(void) {
  RTC.CTRLA &= ~RTC_RTCEN_bm;                        // Disable the RTC
  RTC.INTCTRL = 0;
}


//******************************************************************************************************
void RSbusHardware::triggerRetransmission(uint8_t strategy, bool justTransmitted) {
  // Retransmissions can be triggered by clearing the rsSignalIsOK flag.
  // If this flag is cleared, checkConnection() sets the status to 'notSynchronised'
  // empties the FIFO and clears the data2SendFlag.
  // This will trigger the main sketch to retransmit (all 8 bits of) feedback data
  switch (strategy) {
    case 0:                                  // Do nothing
    break;
    case 1:                                  // Signal an error if we just transmitted
      if (justTransmitted) rsSignalIsOK = false;
    break;
    case 2:                                  // Always signal an error
      rsSignalIsOK = false;                  // Will trigger a retransmission
    break;
    default:                                 // Ignore
    break;
  }
  // If the application will retransmit, we can cancel data ready for transmission
  if (rsSignalIsOK == false) rsISR.data4IsrFlag = false;
}

//******************************************************************************************************
// checkPolling(): Called from main as frequent as possible
//******************************************************************************************************
// See for details: ../extras/BasicOperation-CheckPolling.md
// Every 2ms we check if RTC.CNT has changed or not  
// CheckPolling() ignores all checks, except check 3 and check 5
// - check 1: RTC.CNT does not match previous count 
// - check 2: RTC.CNT matches previous count, but may happen during UART byte transmission 
// - check 3: RTC.CNT matches previous count, and can only happen during a period of silence !!!
// - check 4: same as check 3 
// - check 5: only happens after more than 8ms of silence: parity error (or signal loss) 
// - check 6: same as check 5 
// - check 7: 12ms of silence: seems we lost the RS-signal
void RSbusHardware::checkPolling(void) {
  unsigned long currentTime = millis();                // will not chance during sub routine
  if ((currentTime - rsISR.tLastCheck) >= 2) {         // Check once every 2 ms
    rsISR.tLastCheck = currentTime;   
    uint16_t currentCnt = RTC.CNT;                     // will not chance during sub routine
    if (currentCnt == rsISR.lastPulseCnt) {            // This may be a silence period
      rsISR.timeIdle++;                                // Counts which 2ms check we are in
      switch (rsISR.timeIdle) {                        // See figures above
      case 1:                                          // RTC.CNT differs from previous count
      case 2:                                          // May also occur if UART send byte 
      case 4:                                          // Same as case 3, nothing new
      case 6:                                          // Same as case 5, nothing new
      break;
      case 3:                                          // Third check => SILENCE!
        // Set flags for possible retransmission
        rsISR.flagPulseCount = rsISR.dataWasSendFlag;  // ISR may set the dataWasSendFlag
        rsISR.flagParity     = rsISR.dataWasSendFlag;  // and flags may trigger retransmission
        rsISR.dataWasSendFlag = false;                 // but only is previous cycle had errors
        if (currentCnt == 0) {                         // Figure: case 1A)
          rsSignalIsOK = true;
          if (rsISR.data2sendFlag) {
            if (RTC.CMP == rsISR.address2use) rsISR.data4IsrFlag = true;
          }
        }
        else {                                         // RTC Overflow out of sync
          RTC.CNT = 3;                                 // RTC register updates takes 2 cycles
          if (rsSignalIsOK) {                          // Do nothing during initialisation
            pulseCountErrors ++;
            triggerRetransmission(pulseCountErrorHandling, rsISR.flagPulseCount);
          }
        }
      break;
      case 5:                                          // 8ms of silence
        if (rsSignalIsOK) {                            // Only act if everything was OK before
          parityErrors++;                              // Keep track of number of parity errors
          triggerRetransmission(parityErrorHandling, rsISR.flagParity);
        }
      break;
      case 7:                                          // 12ms of silence
        if (rsSignalIsOK) parityErrors--;              // Wasn't a parity error
        rsSignalIsOK = false;                          // But worse: a RS-bus signal loss
        rsISR.data4IsrFlag = false;                    // Cancel possible data waiting for ISR
      break;
      default:                                         // Silence >= 14ms
      break;
      };
    }
    else {                                             // Not a silence period
      rsISR.lastPulseCnt = currentCnt;                 // CNT can have any value <= 129 (=OVF)
      rsISR.timeIdle = 1;                              // Reset silence (idle) period counter
    }
  }
}


//******************************************************************************************************
// Define the RTC-based Interrupt Service routines (ISR) for the RS-bus
//******************************************************************************************************
ISR(RTC_CNT_vect) {
  // Do we have an Compare Match or an Overflow Interrupt?
  if (RTC.INTFLAGS == RTC_CMP_bm) {        // Compare Match
    RTC.INTFLAGS |= RTC_CMP_bm;            // Clear Compare Match interrupt
    if (rsISR.data4IsrFlag) {
      // We have data to send, it is our turn and the RSbus signal is valid
      // Note: general USART code often includes some kind of flow control, but that is not needed here
      *rsUSART.dataRegister = rsISR.data2send;
      rsISR.data2sendFlag = false;         // RSbusConnection::sendNibble may now prepare new data
      rsISR.data4IsrFlag = false;          // CheckPolling may reset the flag, is there is data2send
      rsISR.dataWasSendFlag = true;        // used to trigger retransmission after arrors
    }
    // Modify, if the next data byte must be send from a different RS-bus address
    if (RTC.CMP != rsISR.address2use) RTC.CMP = rsISR.address2use;
  }
  else {                                   // Must be an overflow (
    RTC.INTFLAGS |= RTC_OVF_bm;            // Clear Overflow interrupt
  }
}

#endif // #if defined(RSBUS_USES_RTC)
