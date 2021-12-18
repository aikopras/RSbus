//******************************************************************************************************
//
// file:      sup_isr_hw_tcb.cpp
// purpose:   Support file for the RS-bus library. 
//            Defines the Interrupt Service routine (ISR) that counts the polling pulses transmitted by
//            the master. Once this decoder is polled, the ISR can send data back via its USART.
//            Uses a TCB as event counter to poll the pulses transmitted by the master.
//            The option of using TCB as event counter only exists on DxCore processors, but not on 
//            MegaCoreX (such as the Nano Every) or traditional ATmega processors (such as the UNO).
// history:   2021-10-16 ap V1.0 initial version
//
// This source file is subject of the GNU general public license 3,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
// If data is available for sending, the data should be entered  into the "data2send" variable and
// the "data2sendFlag" should be set.
// TCBx counts the number of RS-bus pulses and triggers an interrupt once the counter "matches"
// the RS-bus address.
//
// Release note:
// =============
// The current code for this RS-bus approach still relies on the event library as found in  
// DxCore release 1.3.6. That version of the event library does not (yet) support the use of 
// Arduino pin numbers to specify the RS-bus input pin. Instead we will use a fixed pin (PA0). 
// Newer versions of the event library do support using Arduino pin numbers in the attach()
// method. Once a new release of DxCore supports this new version, this software will be updated
// to support complete freedom in choosing the RS-bus input pin  
// References: 
// - https://github.com/SpenceKonde/DxCore
// - https://github.com/SpenceKonde/DxCore/tree/master/megaavr/libraries/Event
//
//
// TCB as Event User - Introduction
// ================================
// TCB can be configured as event user. Two types of event usage can be configured:
// 1) CAPT: in this mode the normal clock is used, and captured in case of an event
// 2) COUNT: Events are used as clock source, and counted according to the selected mode
// => We need to configure as COUNT 
//
// TCB as Event User - COUNT
// -------------------------
// Copied from the AVR128DA datasheet:
// The COUNT event user is enabled on the peripheral by modifying the Clock Select
// (CLKSEL) bit field in the Control A (TCBn.CTRLA) register to EVENT and setting up
// the Event System accordingly. If the Capture Event Input Enable (CAPTEI) bit in the 
// Event Control (TCBn.EVCTRL) register is written to ‘1’, incoming events will result
// in an event action as defined by the Event Edge (EDGE) bit in Event Control (TCBn.EVCTRL)
// register and the Timer Mode (CNTMODE) bit field in Control B (TCBn.CTRLB) register.
// The event needs to last for at least one CLK_PER cycle to be recognized.
//  
// The Timer Mode (CNTMODE) bit field in Control B (TCBn.CTRLB) register must be set to
// Periodic Interrupt Mode.
// 
// TCB - Periodic Interrupt Mode
// -----------------------------
// In Periodic Interrupt mode, the counter (TCBx.CNT) counts to the value stored in TCBx.CCMP.
// If the value is matched, a CAPT interrupt is generated. In the CAPT ISR the interrup flag
// must be cleared, and data (if available) may be transmitted using the USART.
//
// According to the datasheet, a CAPT interrupt automatically resets / clears the counter 
// (TCBx.CNT) to bottom (0). To ensure that after a complete pulse train (thus at the start
// of the silence period) the counter value is 130, the ISR will "reload" the counter with
// the previous CCMP value + 1.  
//
// TCB initialisation
// ------------------
// The counter will be reinitialised / reset by CheckPolling() during the silence period.
// This ensures that the counter value is zero when the next pulse train starts.
// Initialisation is also needed after start-up or after the RS-bus signal was lost and reappears.
// To determine if initialisation is needed, checks are performed during the silence period
// if the counter value matches 130. If this is not the case, a new initialisation takes place.
// If the RS-bus address has changed, CheckPolling() will load the Compare register (TCBx.CCMP)
// to refelct the new RS-bus address
// 
// RS-Bus input pin
// ================
// The RS-Bus can be connected to any input pin that is available to the Event system.
// The Event system triggers on positive edges, although the edge could be inverted via
// the port pin control register after adding some code for that.
//
// Parity errors
// =============
// If the master station detects a parity error, it will enlarge the silence period 
//
// Pins:
// =====
// - a RS-Bus input pin
// - a transmit pin for the UART
//
//
//******************************************************************************************************
#include <Arduino.h>
#include "RSbus.h"
#include "sup_isr.h"
#include "sup_usart.h"

// This code will only be used if we define in RSbusVariants.h the "RSBUS_USES_HW_TCB" directive
#if defined(RSBUS_USES_HW_TCB0) || defined(RSBUS_USES_HW_TCB1) || defined(RSBUS_USES_HW_TCB2) || \
    defined(RSBUS_USES_HW_TCB3) || defined(RSBUS_USES_HW_TCB4)
#include <Event.h>

//******************************************************************************************************
// The following objects are instantiated elsewhere, but are used here
extern RSbusHardware rsbusHardware;  // instantiated in "RS-bus.cpp"
extern volatile RSbusIsr rsISR;      // instantiated in "RS-bus.cpp"
extern USART rsUSART;                // instantiated in "sup_usart.cpp" We only use init()


static volatile TCB_t* _timer;       // In init and detach we use a pointer to the timer

//******************************************************************************************************
// RSbusIsr: constructor
//******************************************************************************************************
RSbusIsr::RSbusIsr(void) {       // Define the constructor
  lastPulseCnt = 0;              // Any value
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

void RSbusHardware::initTcb(void) {
  // Step 1: Instead of calling a specific timer directly, in init and detach we use a pointer to the
  // selected timer. However, since pointers add a level of indirection, the registers that we use
  // (CCMP, CNT and INTFLAGS) will be directly accessed via #defines
  #if defined(RSBUS_USES_HW_TCB0)
    _timer = &TCB0;
    #define timer_CCMP     TCB0_CCMP
    #define timer_CNT      TCB0_CNT
    #define timer_INTFLAGS TCB0_INTFLAGS
  #elif defined(RSBUS_USES_HW_TCB1)
    _timer = &TCB1;
    #define timer_CCMP     TCB1_CCMP
    #define timer_CNT      TCB1_CNT
    #define timer_INTFLAGS TCB1_INTFLAGS
  #elif defined(RSBUS_USES_HW_TCB2)
    _timer = &TCB2;
    #define timer_CCMP     TCB2_CCMP
    #define timer_CNT      TCB2_CNT
    #define timer_INTFLAGS TCB2_INTFLAGS
  #elif defined(RSBUS_USES_HW_TCB3)
    _timer = &TCB3;
    #define timer_CCMP     TCB3_CCMP
    #define timer_CNT      TCB3_CNT
    #define timer_INTFLAGS TCB3_INTFLAGS
  #elif defined(RSBUS_USES_HW_TCB4)
    _timer = &TCB4;
    #define timer_CCMP     TCB4_CCMP
    #define timer_CNT      TCB4_CNT
    #define timer_INTFLAGS TCB4_INTFLAGS
  #endif
  // Step 2: Initialise the TCB in Periodic Interrupt Mode
  // Reset the main timer control registers, needed since the Arduino core creates presets
  noInterrupts();
  _timer->CTRLA = 0;
  _timer->CTRLB = 0;
  _timer->EVCTRL = 0;
  _timer->INTCTRL = 0;
  // Initialise the control registers 
  _timer->CTRLA = TCB_ENABLE_bm | TCB_CLKSEL_EVENT_gc; // Enable TCB and count Events
  _timer->CTRLB = TCB_CNTMODE_INT_gc;                  // Periodic Interrupt Mode
  _timer->EVCTRL = TCB_CAPTEI_bm | TCB_FILTER_bm;      // Enable input capture events and noise cancelation
  _timer->INTCTRL |= TCB_CAPT_bm;                      // Enable CAPT interrupts
  _timer->CCMP = rsISR.address2use;                    // Initial RS-Bus address
  interrupts();
}


void RSbusHardware::initEventSystem(uint8_t rxPin) {
  noInterrupts();
  // Assign Event generator: RS-bus input starts timer
  Event& myEvent = Event::assign_generator_pin(rxPin);
  // Set Event Users
  #if defined(RSBUS_USES_HW_TCB0)
    myEvent.set_user(user::tcb0_cnt);
  #elif defined(RSBUS_USES_HW_TCB1)
    myEvent.set_user(user::tcb1_cnt);
  #elif defined(RSBUS_USES_HW_TCB2)
    myEvent.set_user(user::tcb2_cnt);
  #elif defined(RSBUS_USES_HW_TCB3)
    myEvent.set_user(user::tcb3_cnt);
  #elif defined(RSBUS_USES_HW_TCB4)
    myEvent.set_user(user::tcb4_cnt);
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
  // In principle we could have implemented the 'interruptModeRising' parameter if we include
  // something like PORT*.PIN*CTRL |= PORT_INVEN_bm  
  rxPinUsed = rxPin;                                 // Store, to allow a detach later
  rxPinUsed = PIN_PA0;                               // TODO: Remove, after DxCore event lib has been updated 
  rsUSART.init(usartNumber, !swapUsartPin);          // RS-bus transmission hardware (USART)
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
// CheckPolling() ignores all checks, except check 3 and check 5
// - check 1: ignore
// - check 2: ignore 
// - check 3: TCBx.CCMP should be 130 => reinitialise values, including RS-bus address and flags
// - check 4: ignore 
// - check 5: only happens after more than 8ms of silence: parity error (or signal loss) 
// - check 6: ignore 
// - check 7: 12ms of silence: seems we lost the RS-signal
void RSbusHardware::checkPolling(void) {
  unsigned long currentTime = millis();                // will not chance during sub routine
  if ((currentTime - rsISR.tLastCheck) >= 2) {         // Check once every 2 ms
    rsISR.tLastCheck = currentTime;
    uint16_t currentCnt = timer_CNT;                   // will not chance during sub routine
    if (currentCnt == rsISR.lastPulseCnt) {            // This may be a silence period
      rsISR.timeIdle++;                                // Counts which 2ms check we are in
      switch (rsISR.timeIdle) {                        // See figures above
      case 1:                                          // RTC.CNT differs from previous count
      case 2:                                          // May also occur if UART send byte 
      case 4:                                          // Same as case 3, nothing new
      case 6:                                          // Same as case 5, nothing new
      break;
      case 3:                                          // Third check => SILENCE!
        rsISR.flagPulseCount = rsISR.dataWasSendFlag;  // ISR may set the dataWasSendFlag
        rsISR.flagParity     = rsISR.dataWasSendFlag;  // and flags may trigger retransmission
        rsISR.dataWasSendFlag = false;                 // but only is previous cycle had errors
        if (timer_CNT == 130) {
          rsSignalIsOK = true;
          timer_CNT = 0;                               // Start a new polling cycle
          rsISR.lastPulseCnt = 0;                      // Update as well, since we still have silence
          if (rsISR.data2sendFlag) {
            timer_CCMP = rsISR.address2use;            // The RS-bus address may be changed
            rsISR.ccmpValue = rsISR.address2use;       // For the ISR to reinitialise TCBx.CNT
            rsISR.data4IsrFlag = true;                 // Tell the ISR that data may be send
          }
        }
        else {
          timer_CNT = 0;                               // Start a new polling cycle
          rsISR.lastPulseCnt = 0;                      // Update as well, since we still have silence
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
      rsISR.lastPulseCnt = currentCnt;                 // CNT can have any value <= 129
      rsISR.timeIdle = 1;                              // Reset silence (idle) period counter
    }
  }
}


//******************************************************************************************************
// Define the TCB-based Interrupt Service routine (ISR) for the RS-bus
//******************************************************************************************************
// Select the corresponding ISR
#if defined(RSBUS_USES_HW_TCB0)
  ISR(TCB0_INT_vect) {
#elif defined(RSBUS_USES_HW_TCB1)
  ISR(TCB1_INT_vect) {
#elif defined(RSBUS_USES_HW_TCB2)
  ISR(TCB2_INT_vect) {
#elif defined(RSBUS_USES_HW_TCB3)
  ISR(TCB3_INT_vect) {
#elif defined(RSBUS_USES_HW_TCB4)
  ISR(TCB4_INT_vect) {
#endif
  // Note: the ISR automatically clears the pulse counter TCBx.CNT
  timer_INTFLAGS |= TCB_CAPT_bm;          // We had an interrupt. Clear!
  timer_CNT = rsISR.ccmpValue + 1;        // Revert clearing the pulse counter
  if (rsISR.data4IsrFlag) {
    // We have data to send, it is our turn and the decoder is synchronised
    // Note: general USART code often includes some kind of flow control, but that is not needed here
    *rsUSART.dataRegister = rsISR.data2send;
    rsISR.data2sendFlag = false;         // RSbusConnection::sendNibble may now prepare new data
    rsISR.dataWasSendFlag = true;        // used to trigger retransmission after arrors
    rsISR.data4IsrFlag = false;          // CheckPolling may now select a new RS-bus address
  } 
}

#endif // #if defined(RSBUS_USES_HW_TCB....)
