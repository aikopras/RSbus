#ifndef rsbus_h
#define rsbus_h

//************************************************************************************************
//
// file:      rsbus.h
//
// purpose:   Library to send feedback data back via the RS-bus
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
// history:   2019-02-10 ap V1.0.1 Initial version
//            2021-07-18 ap V1.1   USART can now be selected with number: 0, 1, 2 ...
//
//
// This library can be used to send feedback information from a decoder to the master station
// via the RS-bus. A RS-bus supports a maximum of 128 feedback addresses, numbered 1 to 128.
// The master polls all 128 addresses in a sequential order to ask if they have data to send.
// Per address 8 bits of feedback data can be send; individual feedback messages carry only
// 4 of these bits (a nibble), so 2 messages are needed to send all 8 bits.
// Note that this library supports multiple RS-bus addresses per decoder. For each address
// a separate RS-Bus connection should be established.
//
// If no feedback module sends information, one complete polling cyclus takes 33,1 ms.
// On the other hand, if all feedback modules send information, one polling cyclus takes
// 33,1 + 128 * 1,875 = 273,1 ms. Since feedback modules can only send half of their data
// (one nibble) during one polling cyclus, roughly 550 ms may be needed to send 8 bits.
//
// Used hardware and software:
// - INTx (an interrupt pin): used to receive RS-bus polling pulses from the command station
// - TXD/TXDx (the/an USART): used to send RS-bus messages to the command station
// - the Arduino micros() function
//
//
// The RSbusHardware class
// =======================
// The RSbusHardware class initialises the USART for sending the RS-bus messages, and the 
// Interrupt Service Routine (ISR) used for receiving the RS-bus pulses send by the master
// to poll each decoder if it has data to send.
// Most AVRs have a single USART, but the ATMega 162, 164 and 644 have 2, while the
// 1280 and 2560 as well as newer AtMegaX controllers (such as 4809, 128DA) have 4 USARTs.
// To specify which USART to use a parameter exists in the attach function.
// Take care with traditional Arduino's (such as the UNO and Nano) that may want to use
// their single USART also for communication with the Arduino monitor.
//
// attach()
// Should be called at the start of the program to select the USART and connect
// the RX pin to the RS-bus Interrupt Service Routine (ISR).
//
// detach()
// Should be called in case of a (soft)reset of the decoder, to stop the ISR
//
// checkPolling()
// Should be called as often as possible from the program's main loop.
// The RS-bus master sequentially polls each decoder. After all decoders have been polled,
// a new polling cyclus will start. checkPolling() maintains the polling status.
//
// masterIsSynchronised
// A flag maintained by checkPolling() and used by objects from the RSbusConnection class
// to determine if the start of the (first) polling cyclus has been detected and the master
// is ready to receive feedback data.
//
//
// The RSbusConnection class
// =========================
// For each address this decoder will use, a dedicated RSbusConnection object should be created by
// the main program. To connect to the master station, each RSbusConnection object should start with 
// sending all 8 feedback bits to the master. Since RS_bus messages carry only 4 bits of user data
// (a nibble), the 8 bits will be split in 4 low and 4 high bits and send in two consequetive messages. 
//
// address
// The address used by this RS-bus connection object. Valid values are: 1..128 
//
// needConnect
// A flag indicating to the user that a connection should be established to the master station.
// If this flag is set, the main program should react with calling send8bits(), to tell the
// master the value of all 8 feedback bits.
// Note that this flag may be ignored if we always use send8bits() and send all 8 feedback bits,
// and never use send4bits() for sending only part of our feedback bits.
//
// type
// A variable that specifies the type of decoder. The default value is Switch, but this may be
// changed to Feedback. Note that RS-bus messages do contain two bits to specify this decoder
// type, but there is no evidence that these bits are actually being used by the master. 
// Therefore the "type" can be set via this library for completeness, but an incorrect value
// will (most likely) not lead to any negative effect.

// send4bits()
// Sends a single 4 bit message (nibble) to the master station. We have to specify whether the 
// high or low order bits are being send. 
// Note that the data will not immediately be send, but first be stored in an internal FIFO 
// buffer until the address that belongs to this object is called by the master.
//
// send8bits()
// Sends two 4 bit messages (nibbles) to the master station.
// Note that the data will not immediately be send, but first be stored (as two nibbles) in  
// an internal FIFO buffer until the address that belongs to this object is called by the master.
//
// checkConnection()
// Should be called as often as possible from the program's main loop.
// It maintains the connection logic and checks if data is waiting in the FIFO buffer.
// If data is waiting, it checks if the USART and RS-bus ISR are able to accept that data.
// The RS-bus ISR waits till its address is being polled by the master, and once it gets polled
// sends the RS-bus message (carrying 4 bits of feedback data) to the master. 
//
//
//************************************************************************************************
#include <Arduino.h>
#include "support_isr.h"
#include "support_usart.h"
#include "support_fifo.h"


// The following types are defined as globals, so they can be used by the main program
enum Decoder_t { Switch, Feedback };
enum Nibble_t  { HighBits, LowBits };


//************************************************************************************************
class RSbusHardware {
  public:
    RSbusHardware();                    // The constructor for this class
  
    uint8_t masterIsSynchronised;       // Flag: decoder has detected the start of a new polling cyclus 

    void attach(                        // configures the RS-bus ISR and USART
      uint8_t usartNumber,              // usart for sending (0..4)
      uint8_t rx_pin                    // pin used for receiving, must be an INT pin
    );
    
    void detach(void);                  // stops the RS-bus ISR
    void checkPolling(void);            // Checks the polling logic of the RS-bus receiver.

  private:
    int rx_pin_used;                    // local copy of pin used for sending, using the USART
 };


//************************************************************************************************
class RSbusConnection {
  public:
    RSbusConnection();                  // The constructor for this class

    uint8_t address;                    // 1..128. The address used for this RS-bus connection
    uint8_t needConnect;                // A flag signalling the main program that it should to connect to the master
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
      NotConnected,
      ConnectionIsNeeded,
      ConnectNibble1,
      ConnectNibble2,
      Connected
    } status;
};

#endif
