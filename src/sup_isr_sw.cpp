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
// If data is made available by the main sketch (the 'data2sendFlag' is set and the data has been
// entered into the 'data2send' variable), the data will be send once the 'addressPolled' variable
// matches the address ('address2use') of this decoder (with offset 1).
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
// - Timer5: If we have a 2560 processor
//
//******************************************************************************************************
#include <Arduino.h>
#include "RSbus.h"
#include "sup_isr.h"
#include "sup_usart.h"

// This code is the default: it is only used if we didn't specify in RSbus.h any other directive
#if defined(RSBUS_USES_SW)

// In case of a processor with many timers (like the 2560), use Timer 5 to make checkPolling more robust
#if defined(__AVR_ATmega2560__)
  #define USE_TIMER_5
#endif
//******************************************************************************************************
// The following objects are instantiated elsewhere, but are used here
extern RSbusHardware rsbusHardware;  // instantiated in "RS-bus.cpp"
extern volatile RSbusIsr rsISR;      // instantiated in "RS-bus.cpp"
extern USART rsUSART;                // instantiated in "sup_usart.cpp" We only use init()


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
  if (interruptModeRising) attachInterrupt(digitalPinToInterrupt(rxPin), rs_interrupt, RISING);
  else attachInterrupt(digitalPinToInterrupt(rxPin), rs_interrupt, FALLING);
  // Step 3: In case we use a 2560 processor, Timer5 is used to update addressPolled
  init_timer5();
}

void RSbusHardware::detach(void) {
  detachInterrupt(digitalPinToInterrupt(rxPinUsed));
  stop_timer5();
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
  #if !defined(USE_TIMER_5)                            // Skip, since Timer 5 takes over
  unsigned long currentTime = millis();                // will not chance during sub routine
  if ((currentTime - rsISR.tLastCheck) >= 2) {         // Check once every 2 ms
    rsISR.tLastCheck = currentTime;
      updateAddressPolled();
  }
  #endif
}


void RSbusHardware::updateAddressPolled(void) {
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



//******************************************************************************************************
// Define the Interrupt Service routines (ISR) for the RS-bus
//******************************************************************************************************
void rs_interrupt(void) {
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
  rsISR.addressPolled ++;                  // Address of slave that gets his turn next
}


//******************************************************************************************************
// Timer 5 Interrupt Service routines to call updateAddressPolled() every 2ms
//******************************************************************************************************
// In "normal" sketches checkPolling() is called as frequent as possible from the main loop.
// checkPolling() in turn calls updateAddressPolled(), which resets the addressPolled variable.
// It is important that updateAddressPolled() is called at least every 2ms.
// Unfortunately some libraries, like the LCD library, are quite slow. For example, writing to the
// LCD display can easily costs many ms.
// As a result, updateAddressPolled() may miss the right moment to reset the polled address,
// which in turn will lead to RS-Bus pulse count errors.
// Fortunately the Mega2560 processor has multiple unused 16 bit timers.
// By using a timer ISR, we can ensure that updateAddressPolled() will be called as often as needed.
//
// For simplicity We will use Timer 5, and use hard-coded values presuming the CPU runs at 16Mhz.
// Calculation:
// - 16Mhz and a prescaler of 8 results in an interrupt every 0,5 microseconds
// - For 2ms we therefore need 4000 ticks
// - TCNT should therefore be preloaded with 65535 - 4000 = 61535
#define START_VALUE      61535
#define PRESCALER_BITS    0x02  // Means the prescaler becomes 8


void RSbusHardware::init_timer5() {
  #if defined(USE_TIMER_5)
    noInterrupts();             // disable all interrupts
    TCCR5A = 0x00;              // Should remain 0 for overflow mode
    TCCR5B = 0x00;              // Should hold the prescaler. 0 = stop (needed to setup)
    TCNT5 = START_VALUE;        // Preload the timer
    TIMSK5 = 0x01;              // Timer5 INT Reg: Timer5 Overflow Interrupt Enable
    TCCR5B = PRESCALER_BITS;    // Start the timer by setting the Prescaler
    interrupts();               // enable all interrupts
  #endif
}


void RSbusHardware::stop_timer5() {
#if defined(USE_TIMER_5)
  noInterrupts();             // disable all interrupts
  TCCR5B = 0x00;              // 0 = stop
  interrupts();               // enable all interrupts
#endif
}


#if defined(USE_TIMER_5)
ISR(TIMER5_OVF_vect) {
  TCNT5 = START_VALUE;        // Reload the timer
  rsbusHardware.updateAddressPolled();
}
#endif

//******************************************************************************************************
#endif // #if defined(RSBUS_USES_SW)
