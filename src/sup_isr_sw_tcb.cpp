//******************************************************************************************************
//
// file:      sup_isr_sw_tcb.cpp
// purpose:   Support file for the RS-bus library. 
//            Defines the Interrupt Service routine (ISR) that counts the polling pulses transmitted 
//            by the master. Once this decoder is polled, the ISR can send data back via its USART. 
//            Uses a TCB in Periodic Interrupt mode to count the number and measure the length
//            (=duration) of pulses transmitted by the master.
//            Of all variants, this should be the most reliable option for the RS-bus.
//            This option only exists on MegaCoreX and DxCore processors, but not on
//            traditional ATmega processors (such as the UNO).
// history:   2021-12-13 ap V1.0 Initial version
//
// This source file is subject of the GNU general public license 3,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
// For details, see: ../extras/BasicOperation.md
//
//******************************************************************************************************
#include <Arduino.h>
#include "RSbus.h"
#include "sup_isr.h"
#include "sup_usart.h"

// This code will only be used if we define in RSbusVariants.h the "RSBUS_USES_SW_TCB" directive
#if defined(RSBUS_USES_SW_TCB0) || defined(RSBUS_USES_SW_TCB1) || defined(RSBUS_USES_SW_TCB2) || \
defined(RSBUS_USES_SW_TCB3) || defined(RSBUS_USES_SW_TCB4)
#include <Event.h>


//******************************************************************************************************
// The following objects are instantiated elsewhere, but are used here
extern RSbusHardware rsbusHardware;  // instantiated in "RS-bus.cpp"
extern volatile RSbusIsr rsISR;      // instantiated in "RS-bus.cpp"
extern USART rsUSART;                // instantiated in "sup_usart.cpp" We only use init()


static volatile TCB_t* _timer;       // In init and detach we use a pointer to the timer
#define T180uS F_CPU / 1000000 * 180 // RS-bus pulses shorter than 180us will trigger an error

//******************************************************************************************************
// RSbusIsr: constructor
//******************************************************************************************************
RSbusIsr::RSbusIsr(void) {       // Define the constructor
  addressPolled = 0;             // Start with any address; the first polling cyclus will not be used
  lastPulseCnt = 0;              // Any value
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
// The RSbusHardware class is responsible for controlling the RS-bus hardware, thus the TCB that counts
// the polling pulses send by the master, and the USART connected to an output pin to send messages
// to the master.
// Messages are: 8 bit, no parity, 1 stop bit, asynchronous mode, 4800 baud.
// The "attach" method initialises the TCB and USART.
// A detach method is available to disable the TCB, which is needed before a decoder gets restarted.
// Note regarding the code: instead of the "RSbusHardware::attach" method we could have directly used
// the RSbusIsr and USART class constructors. However, since we also need a "RSbusHardware::detach" method
// to stop the rs_interrupt service routine, for symmetry reasons we have decided for an "attach"
// and "detach".

void initTcb(void) {
  // Step 1: Instead of calling a specific timer directly, in init and detach we use a pointer to the
  // selected timer. However, since pointers add a level of indirection, the registers that we use
  // (EVCTRL and CCMP) will be directly accessed via #defines
#if defined(RSBUS_USES_SW_TCB0)
  _timer = &TCB0;
  #define timer_EVCTRL TCB0_EVCTRL
  #define timer_CCMP   TCB0_CCMP
#elif defined(RSBUS_USES_SW_TCB1)
  _timer = &TCB1;
  #define timer_EVCTRL TCB1_EVCTRL
  #define timer_CCMP   TCB1_CCMP
#elif defined(RSBUS_USES_SW_TCB2)
  _timer = &TCB2;
  #define timer_EVCTRL TCB2_EVCTRL
  #define timer_CCMP   TCB2_CCMP
#elif defined(RSBUS_USES_SW_TCB3)
  _timer = &TCB3;
  #define timer_EVCTRL TCB3_EVCTRL
  #define timer_CCMP   TCB3_CCMP
#elif defined(RSBUS_USES_SW_TCB4)
  _timer = &TCB4;
  #define timer_EVCTRL TCB4_EVCTRL
  #define timer_CCMP   TCB4_CCMP
#endif
  // Step 2: fill the registers. See the data sheets for details
  noInterrupts();
  // Clear the main timer control registers. Needed since the Arduino core creates some presets
  _timer->CTRLA = 0;
  _timer->CTRLB = 0;
  _timer->EVCTRL = 0;
  _timer->INTCTRL = 0;
  _timer->CCMP = 0;
  _timer->CNT = 0;
  _timer->INTFLAGS = 0;
  // Initialise the control registers
  _timer->CTRLA = TCB_ENABLE_bm;                    // Enable the TCB peripheral, clock is CLK_PER (=F_CPU)
  _timer->CTRLB = TCB_CNTMODE_FRQ_gc;               // Input Capture Frequency Measurement mode
//  _timer->EVCTRL = TCB_CAPTEI_bm | TCB_FILTER_bm;   // Enable input capture events and noise cancelation
  _timer->EVCTRL = TCB_CAPTEI_bm;   // Enable input capture events and noise cancelation
  _timer->INTCTRL |= TCB_CAPT_bm;                   // Enable CAPT interrupts
  interrupts();
}


void initEventSystem(uint8_t rxPin) {
  noInterrupts();
  // Assign Event generator: a positive edge on the RS-bus input pin starts the timer
  Event& myEvent = Event::assign_generator_pin(rxPin);
  // Set Event Users
  #if defined(RSBUS_USES_SW_TCB0)
    myEvent.set_user(user::tcb0_capt);
  #elif defined(RSBUS_USES_SW_TCB1)
    myEvent.set_user(user::tcb1_capt);
  #elif defined(RSBUS_USES_SW_TCB2)
    myEvent.set_user(user::tcb2_capt);
  #elif defined(RSBUS_USES_SW_TCB3)
    myEvent.set_user(user::tcb3_capt);
  #elif defined(RSBUS_USES_SW_TCB4)
    myEvent.set_user(user::tcb4_capt);
  #endif
  // Start the event channel
  myEvent.start();
  interrupts();
}


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
  // Store the pin number, to allow a detach later
  rxPinUsed = rxPin;
  // STEP 1: initialise the RS bus transmission hardware (USART)
  // Use swapUsartPin to set the defaultUsartPins parameter.
  rsUSART.init(usartNumber, !swapUsartPin);
  // Step 2: attach the interrupt to the RSBUS_RX pin.
  initTcb();
  initEventSystem(rxPin);
}

void RSbusHardware::detach(void) {
  noInterrupts();
  // Clear all TCB timer settings
  // For "reboot" (jmp 0) it is crucial to set INTCTRL = 0
  _timer->CTRLA = 0;
  _timer->CTRLB = 0;
  _timer->EVCTRL = 0;
  _timer->INTCTRL = 0;
  _timer->CCMP = 0;
  _timer->CNT = 0;
  _timer->INTFLAGS = 0;
  interrupts();
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
// Every 2ms we check if addressPolled has changed or not  
// CheckPolling() ignores all checks, except check 3, 5 and check 7
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
      case 3:                                          // Third check => SILENCE!
        // Set flags for possible retransmission
        rsISR.flagPulseCount = rsISR.dataWasSendFlag;  // ISR may set the dataWasSendFlag
        rsISR.flagParity     = rsISR.dataWasSendFlag;  // and flags may trigger retransmission
        rsISR.dataWasSendFlag = false;                 // but only is previous cycle had errors
        if (rsISR.addressPolled == 130) {
          // Step 2A: signal is OK, so tell ISR if data is waiting to be transmitted
          rsSignalIsOK = true;
          if (rsISR.data2sendFlag) rsISR.data4IsrFlag = true;
        }
        else {                                         // pulse count problem
          if (rsSignalIsOK) {                          // Do nothing during initialisation
            pulseCountErrors ++;
            triggerRetransmission(pulseCountErrorHandling, rsISR.flagPulseCount);
          }
        }
        rsISR.addressPolled = 0;                       // Reset 
        rsISR.lastPulseCnt = 0;                        // Reset
      break;
      case 5:                                          // 8ms of silence => Parity error
        if (rsSignalIsOK) {                            // Only act if everything was OK before
          parityErrors++;                              // Keep track of number of parity errors
          triggerRetransmission(parityErrorHandling, rsISR.flagParity);
        }
      break;
      case 7:                                          // 12ms of silence: RS-bus signal loss
        if (rsSignalIsOK) parityErrors--;              // Wasn't a parity error
        rsSignalIsOK = false;                          // Will trigger a reconnect to the master
        rsISR.data4IsrFlag = false;                    // Cancel possible data waiting for ISR
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
// Define the TCB-based Interrupt Service routine (ISR) for the RS-bus
//******************************************************************************************************
// Select the corresponding ISR
#if defined(RSBUS_USES_SW_TCB0)
  ISR(TCB0_INT_vect) {
#elif defined(RSBUS_USES_SW_TCB1)
  ISR(TCB1_INT_vect) {
#elif defined(RSBUS_USES_SW_TCB2)
  ISR(TCB2_INT_vect) {
#elif defined(RSBUS_USES_SW_TCB3)
  ISR(TCB3_INT_vect) {
#elif defined(RSBUS_USES_SW_TCB4)
  ISR(TCB4_INT_vect) {
#endif
  uint16_t delta = timer_CCMP;             // Delta holds the time since the previous interrupt
  if (delta <= T180uS)
    // Pulse count error. We lost track of where we are within the current pulse train.
    // Therefore don't transmit during this pulse train, and let checkPolling() decide what to do
    rsISR.data4IsrFlag = false;
  else {
    if (rsISR.data4IsrFlag) {
      if (rsISR.address2use == rsISR.addressPolled) {
        // We have data to send, it is our turn and the RSbus signal is valid
        // Note: general USART code often includes some kind of flow control, but that is not needed here
        *rsUSART.dataRegister = rsISR.data2send;
        rsISR.data2sendFlag = false;         // RSbusConnection::sendNibble may now prepare new data
        rsISR.data4IsrFlag = false;          // CheckPolling may reset the flag, is there is data2send
        rsISR.dataWasSendFlag = true;        // used to trigger retransmission after arrors
      }
    }
  }
  rsISR.addressPolled ++;                  // Address of slave that gets his turn next
}

#endif // #if defined(RSBUS_USES_SW_TCB....)
