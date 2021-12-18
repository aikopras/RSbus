//************************************************************************************************
//
// file:      rsbus.h
//
// purpose:   Library to send feedback data back via the RS-bus
//
// This source file is subject of the GNU general public license 3,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
// history:   2019-02-10 ap V1.0.1 Initial version
//            2021-07-18 ap V1.1.1 USART can now be selected with number: 0, 1, 2 ...
//            2021-07-26 ap v1.1.2 Default type is now 'Feedback decoder'
//            2021-10-30 ap v2.0.0 Major rewrite of sup.isr*. Hardware decoding (RTC, TCB) added 
//
// This Arduino library can be used to send feedback information from a decoder to the master
// station via the (LENZ) RS-bus. The RS-bus is the standard feedback bus used for Lenz products,
// and supported by several other vendors. 
// For further information on this library and the RS-bus see:
// - https://github.com/aikopras/RSbus/blob/master/README.md
// - http://www.der-moba.de/index.php/RS-Rückmeldebus (in German),
// - https://sites.google.com/site/dcctrains/rs-bus-feed
//
// Used hardware and software:
// Two Arduino pins are needed for this library, as well as some software:
// - A receive pin `rxPin`: needed to receive RS-bus polling pulses from the command station
// - A USART transmit pin: needed to send RS-bus messages to the command station
// - The timer associated with the Arduino millis() function
// - Optional (depends on micro-controller and variant): MightyCore, MegaCore, MegaCoreX or
//   DxCore board. See https://github.com/MCUdude and [https://github.com/SpenceKonde.
//
// Important: The file rsbusVariants.h may be modified to select a different decoding approach.
// The following approaches are supported:
// - default (V2) => works on all ATMega micro-processors
// - RTC (V2) => recommended for newer processors on MegaCoreX / DxCore boards. Requires pin PA0!
// - TCB (V2) => Supported only on DxCore boards. Useful if pin PA0 is not available. 
// - earlier version of the default approach (V1.1.2) => included for compatibility reasons
//
//************************************************************************************************
#pragma once
#include <Arduino.h>
#include "RSbusVariants.h"           // Edit this file to choose your favoured RS-Bus ISR approach
#include "sup_isr.h"
#include "sup_usart.h"
#include "sup_fifo.h"


//************************************************************************************************
// The following types are defined as globals, so they can be used by the main program
enum Decoder_t { Switch, Feedback };
enum Nibble_t  { HighBits, LowBits };

//************************************************************************************************
class RSbusHardware {
  public:
    RSbusHardware();                    // The constructor for this class
  
    bool rsSignalIsOK;                  // Flag to indicate if the polling cyclus is error-free 
    bool interruptModeRising;           // The interrupt triggers at the RISING edge (default: true)
    bool swapUsartPin;                  // Enables the use of alternative USART pins (default: false)
    uint8_t parityErrors;               // Number of parity errors detected
    uint8_t pulseCountErrors;           // Number of pulse count errors detected
    uint8_t parityErrorHandling;        // 0..2. 0: no reaction, 1: only if just transmitted, 2: always
    uint8_t pulseCountErrorHandling;    // 0..2. 0: no reaction, 1: only if just transmitted, 2: always
  
    void attach(                        // Initialises the RS-bus ISR
      uint8_t usartNumber,              // usart for sending (0..4)
      uint8_t rxPin);                   // pin used for receiving; in the default case an INT pin
   
    void detach(void);                  // stops the RS-bus ISR
    void checkPolling(void);            // Checks the polling logic of the RS-bus receiver.
  
  private:
    int rxPinUsed;                      // local copy of pin used for sending, using the USART
    void triggerRetransmission(         // May set rsSignalIsOK to false, which triggers retransmission
      uint8_t strategy,                 // 0 = never, 1 = if just transmitted, 2 = always
      boolean dataWasSendFlag           // for strategy = 1
    );
    void initTcb(void);                 // For the TCB variants
    void initEventSystem(uint8_t rxPin); // For the TCB variants
};


//************************************************************************************************
class RSbusConnection {
  public:
    RSbusConnection();                  // The constructor for this class

    uint8_t address;                    // 1..128. The address used for this RS-bus connection
    uint8_t forwardErrorCorrection;     // 0..2. 0 = no retransmission, 1 = one retransmission, 2 = two ...
    bool feedbackRequested;             // A flag signalling the main program that it should send 8 feedback bits
    Decoder_t type;                     // Do we send Switch or Feedback messages? Default: Switch

                                     
    void send4bits(                     // Sends a single message (nibble) to the master station
      Nibble_t nibble,                  // Do we use the high or low bits for this nibble?
      uint8_t value                     // 0..15, thus 4 bits that fit in 1 RS-bus message
    );
    
    void send8bits(
      uint8_t value                     // 0..255, which means we send 2 RS-bus messages
    );

    void checkConnection(void);         // checks if data is waiting in the FIFO queue. If yes,
                                        // calls sendNibble to handle that data to the RS-bus ISR. 

  private:
    FIFO my_fifo;                       // FIFO queue that can store a number of nibbles
    uint8_t sendNibble(void);           // Called by checkConnection to send a nibble from the FIFO  
    void format_nibble(uint8_t value);  // To set the decoder type and the parity bit

    enum Status {                       // The state machine that is maintained for each connection
      notSynchronised,
      feedbackIsNeeded,
      feedbackNibble1,
      feedbackNibble2,
      connected
    } status;
};
