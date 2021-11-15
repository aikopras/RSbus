//******************************************************************************************************
//
// file:      sup_isr_sw.cpp
// purpose:   Support file for the RS-bus library. 
//            Defines the Interrupt Service routine (ISR) that counts the polling pulses transmitted 
//            by the master. Once this decoder is polled, the ISR can send data back via its USART. 
// history:   2019-01-30 ap V0.1 Initial version
//            2021-08-18 ap V0.2 millis() replaced by flag
//            2021-10-12 ap V1.0 Check every 2ms and ability to detect parity errors
//
// This source file is subject of the GNU general public license 3,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
// For details, see: ../extras/BasicOperation.md
//
// The rs_interrupt() ISR is called whenever a transistion is detected on the RS-bus.
// Such transistion indicates that the next feedback decoder is allowed to send information.
// To determine which decoder has its turn, the ISR increments at each transition the
// 'addressPolled' variable. 
// If data is made available to the ISR (the 'data2sendFlag' is set and the data has been entered 
// into the 'data2send' variable), the data will be send once the 'addressPolled' variable matches  
// theaddress ('address2use') of this decoder (with offset 1).
//
//         <-0,2ms->                                       <-------------7ms------------->
//    ____      ____      ____              ____      ____                                 ____
//   |    |    |    |    |    |            |    |    |    |                               |    |
//  _|    |____|    |____|    |____________|    |____|    |_______________________________|    |__ Rx
//        ++        ++        ^                 ++       =130                                 1
//                            |                                               
//                        my address                                         
//                                                                   
// ____________________________XXXXXXXXY___________________________________________________________Tx
//                             <1,875ms>
//
// CheckPolling() is called from the main loop as often as possible. Every 2 milliseconds it
// compares the value of 'addressPolled' to the value stored in 'lastPulseCnt' during the previous
// check. If both values match, we (very likely) have a period of silence.
// See for details: ../extras/BasicOperation-CheckPolling.md
//
// Used hardware and software:
// - INTx: used to receive RS-bus information from the command station
// - TXD/TXDx: used to send RS-bus information to the command station (USART)
// - the Arduino micros() function
//
//******************************************************************************************************
#include <Arduino.h>
#include "RSbus.h"
#include "sup_isr.h"
#include "sup_usart.h"

// This code is the default: it is only used if we didn't specify in RSbus.h usage of other code
#if !defined(RSBUS_USES_RTC)  && !defined(RSBUS_USES_SW_4MS) && !defined(RSBUS_USES_TCB0) && \
    !defined(RSBUS_USES_TCB1) && !defined(RSBUS_USES_TCB2)   && !defined(RSBUS_USES_TCB3) && \
    !defined(RSBUS_USES_TCB4)


//************************************************************************************************
// The following objects are instantiated elsewhere, but are used here
extern RSbusHardware rsbusHardware;  // instantiated in "RS-bus.cpp"
extern volatile RSbusIsr rsISR;      // instantiated in "RS-bus.cpp"
extern USART rsUSART;                // instantiated in "sup_usart.cpp" We only use init()


//**********************************************************************************************
// RSbusIsr: constructor
//**********************************************************************************************
RSbusIsr::RSbusIsr(void) {       // Define the constructor
  addressPolled = 0;             // Start with any address; the first polling cyclus will not be used
  data2send = 0;                 // Empty our send data byte
  data2sendFlag = false;         // No, we don't have anything to send yet
  lastPulseCnt = 0;              // Any value
  tLastCheck = millis();         // Current time
}


//******************************************************************************************************
//******************************************************************************************************
// The RSbusHardware class is responsible for controlling the RS-bus hardware, thus an ISR
// connected to an (interrupt) input pin that counts the polling pulses send by the master, and
// the USART connected to an output pin to send messages from the decoder to the master.
// Messages are: 8 bit, no parity, 1 stop bit, asynchronous mode, 4800 baud.
// The 'attach' method makes the connection between input pin and ISR, initialises the USART and
// sets the USART dataRegister to the right USART (UDR, UDR0, UDR1, UDR2 or UDR3)
// A detach method is available to disable the ISR, which is needed before a decoder gets restarted.
// Note regarding the code: instead of the 'RSbusHardware::attach' method we could have directly used
// the RSbusIsr and USART class constructors. However, since we also need a 'RSbusHardware::detach' method
// to stop the rs_interrupt service routine, for symmetry reasons we have decided for an 'attach'
// and 'detach'.

RSbusHardware::RSbusHardware() {                     // Constructor
  masterIsSynchronised = false;                      // No RS-bus signal detected yet
  swapUsartPin = false;                              // We use the default USART TX pin
  interruptModeRising = true;                        // Earlier hardware triggered on FALLING
  parityErrors = 0;                                  // Counter for the number of 10,7ms gaps
  parityErrorFlag = false;                           // Flag for the previous cycle
}

void RSbusHardware::attach(uint8_t usartNumber, uint8_t rxPin) {
  // Step 1: attach the interrupt to the RSBUS_RX pin.  
  if (interruptModeRising) attachInterrupt(digitalPinToInterrupt(rxPin), rs_interrupt, RISING);
  else attachInterrupt(digitalPinToInterrupt(rxPin), rs_interrupt, FALLING);
  // Store the pin number, to allow a detach later
  rxPinUsed = rxPin;
  // STEP 2: initialise the RS bus transmission hardware (USART)
  // Use swapUsartPin to set the defaultUsartPins parameter.
  rsUSART.init(usartNumber, !swapUsartPin);
}

void RSbusHardware::detach(void) {
  detachInterrupt(digitalPinToInterrupt(rxPinUsed));
}


//************************************************************************************************
// checkPolling(): Called from main as frequent as possible
//************************************************************************************************
// See for details: ../extras/BasicOperation-CheckPolling.md
// Every 2ms we check if addressPolled has changed or not  
// CheckPolling() ignores all checks, except check 3 and check 5
// - check 1: addressPolled is 130, but lastPulseCnt has a lower value  
// - check 2: addressPolled is 130, and lastPulseCnt is now also 130 => silence 
// - check 3: Period of silence: set addressPolled and lastPulseCnt to 0 
// - check 4: same as check 3 
// - check 5: only happens after more than 8ms of silence: parity error (or signal loss) 
// - check 6: same as check 5 
// - check 7: 12ms of silence: seems we lost the RS-signal
void RSbusHardware::checkPolling(void) {
  unsigned long currentTime = millis();                // will not chance during sub routine
  if ((currentTime - rsISR.tLastCheck) >= 2) {         // Check once every 2 ms
    rsISR.tLastCheck = currentTime;   
    uint16_t currentCnt = rsISR.addressPolled;         // will not chance during sub routine
    if (currentCnt == rsISR.lastPulseCnt) {            // This may be a silence period
      rsISR.timeIdle++;                                // Counts which 2ms check we are in
      switch (rsISR.timeIdle) {                        // See figures in documentation
      case 1:                                          // addressPolled differs from previous count
      case 2:                                          // May also occur if UART send byte 
      case 4:                                          // Same as case 3, nothing new
      case 6:                                          // Same as case 5, nothing new
      break;
      case 3:                                          // Third check 
        if (rsISR.addressPolled == 130) 
          masterIsSynchronised = true;
        else
          masterIsSynchronised = false;
        rsISR.addressPolled = 0;                       // Reset 
        rsISR.lastPulseCnt = 0;                        // Reset
        parityErrorFlag = false;                       // Cycle is over. Clear flag
      break;
      case 5:                                          // 8ms of silence
        parityErrorFlag = true;                        // Retransmission may be attempted in next cycle
        parityErrors++;                                // Keep track of number of parity errors
      break;
      case 7:                                          // 12ms of silence
        parityErrorFlag = false;                       // Wasn't a parity error
        parityErrors--;                                // Wasn't a parity error
        masterIsSynchronised = false;                  // But worse: a RS-bus signal loss
        rsISR.data2sendFlag = false;                   // Cancel possible data waiting for ISR
      break;
      default:                                         // Silence >= 14ms
      break;
      };
    }
    else {                                             // Not a silence period
      rsISR.lastPulseCnt = currentCnt;                 // Store current addressPolled
      rsISR.timeIdle = 1;                              // Reset silence (idle) period counter
    }
  }
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
      *rsUSART.dataRegister = rsISR.data2send;
      rsISR.data2sendFlag = false;
    } 
  }
  rsISR.addressPolled ++;        // Address of slave that gets his turn next
}


#endif // #ifndef (RSBUS_USES_RTC || RSBUS_USES_SW_4MS)
