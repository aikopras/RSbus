//*******************************************************************************************************
//
// file:      sup_isr_sw.cpp
// purpose:   Support file for the RS-bus library. 
//            Defines the Interrupt Service routine (ISR) that counts the polling pulses transmitted by
//            the master. Once this decoder is polled, the ISR can send data back via its USART.
//            This code was included with version 1 of the RSbus library.
// history:   2019-01-30 ap V0.1 Initial version
//            2021-08-18 ap V0.2 millis() replaced by flag
//            2022-07-27 ap V0.3 millis() replaced by micros()
//
// This source file is subject of the GNU general public license 3,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
// For details, see: ../extras/BasicOperation-Initial_Version.md

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

// This code will only be used if we defined in RSbusVariants.h the "RSBUS_USES_SW_4MS" directive
#if defined(RSBUS_USES_SW_4MS)


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
}


//******************************************************************************************************
//******************************************************************************************************
// The RSbusHardware class is responsible for controlling the RS-bus hardware, thus an ISR
// connected to an (interrupt) input pin that counts the polling pulses send by the master, and
// the USART connected to an output pin to send messages from the decoder to the master.
// Messages are: 8 bit, no parity, 1 stop bit, asynchronous mode, 4800 baud.
// The "attach" method makes the connection between input pin and ISR, initialises the USART and
// sets the USART dataRegister to the right USART (UDR, UDR0, UDR1, UDR2 or UDR3)
// A detach method is available to disable the ISR, which is needed before a decoder gets restarted.
// Note regarding the code: instead of the "RSbusHardware::attach" method we could have directly used
// the RSbusIsr and USART class constructors. However, since we also need a "RSbusHardware::detach" method
// to stop the rs_interrupt service routine, for symmetry reasons we have decided for an "attach"
// and "detach".

RSbusHardware::RSbusHardware() {                     // Constructor
  rsSignalIsOK = false;                              // No valid RS-bus signal detected yet
  swapUsartPin = false;                              // We use the default USART TX pin
  interruptModeRising = true;                        // Earlier hardware triggered on FALLING
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
// checkPolling() is called from the main loop. If there has been no RS-bus activity for 4 msec,
// it needs to reset the addressPolled attribute.
//
//    ____      ____                  4 msec                     ____    RS-bus ISR():
//   |    |    |    |                   |                       |    |   - addressPolled++
//  _|    |____|    |___________________v_______________________|    |   - timeIdle = 0
//
//  -----------------------------------><----------------------------><--------------------------
//      addressPolled > 0                    addressPolled = 0             addressPolled > 0
//
//                                       checkPolling() {
//                                         if (addressPolled > 0)
//                                           if (timeIdle == 0)
//                                             timeIdle = 1;
//                                             tLastInterrupt = micros();
//                                           else if ((micros() - tLastInterrupt) > 4 msec)
//                                             addressPolled = 0;
//
//************************************************************************************************
void RSbusHardware::checkPolling(void) {
  if (rsISR.addressPolled != 0) {
    if (rsISR.timeIdle == 0) {
      // We already had a RS-Bus interrupt just before
      rsISR.timeIdle = 1;
      rsISR.tLastInterrupt = micros();
    }
    else {
      // No ISR since previous call
      if ((micros() - rsISR.tLastInterrupt) > 4000) {
        // A new RS-bus cycle has started. Reset addressPolled
        // If 130 addresses were polled, layer 1 works fine
        if (rsISR.addressPolled == 130) rsSignalIsOK = true;
        else {rsSignalIsOK = false;  }
        rsISR.addressPolled = 0;
      }
    }
  }
  else
    if (rsSignalIsOK)
      if ((micros() - rsISR.tLastInterrupt) > 10000) rsSignalIsOK = false; // more than 10ms silent
  if (rsSignalIsOK == false) rsISR.data2sendFlag = false; // cancel possible data waiting for ISR
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
      rsISR.data2sendFlag = false;
    } 
  }
  rsISR.addressPolled ++;        // Address of slave that gets his turn next
  rsISR.timeIdle = 0;            // Reset the counter since the command station is not idle now
}

#endif // #if defined(RSBUS_USES_SW_4MS)
