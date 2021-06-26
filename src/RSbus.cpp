//******************************************************************************************************
//
// file:      rsbus.cpp
//
// purpose:   Library to send feedback data back via the RS-bus
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
// history:   2019-02-10 ap V0.1 Initial version
//
//
// This library can be used to send feedback information from decoder to master station via
// the RS-bus.  For details on the operation of the RS-bus, see also:
// http://www.der-moba.de/index.php/RS-R%C3%BCckmeldebus
//
// The basic operation of the RS-bus is as follows. 
// The master polls sequentially all 128 decoders to ask them to send feedback data, if available. 
// A complete polling sequence consists of 130 pulses; if there is no feedback data to be send 
// the time between individual pulses will be 200 micro-seconds. Each puls raises an interrupt
// (FALLING), which triggers an Interrupt Service Routine (ISR) that increments the
// "addressPolled" variable. Once all feedback modules are polled, the master is idle for 7 ms. 
//
// Feedback modules may send information back to the master once their address is polled.
// The length of information is 9 bits, and takes around 1,875 ms (4800 baud).
// During that period, no RS-bus transistions will occur. Therefore the length of the
// timing interval to detect if the master is idle, should be between 1,875 ms and 7 ms.
// We will use a value of 4 ms (4000 microsecond), which seems a save choice.
// If there has been more than 8 ms of silence, we conclude that the synchronisation with the
// master has been lost.
//  
//         <-0,2ms->                                       <-------------7ms------------->
//    ____      ____      ____              ____      ____                                 ____
//   |    |    |    |    |    |            |    |    |    |                               |    |
//  _|    |____|    |____|    |____________|    |____|    |_______________________________|    |__ Rx
//        ++        ++        ^                 ++       =130                 ^                1
//                            |                                               |
//                        my address                                         4ms
//                                                                   reset addressPolled
// ____________________________XXXXXXXXY___________________________________________________________Tx
//                             <1,875ms>
//
// The 4 ms idle period allows all feedback modules to synchronize (which means, in our case,
// that the "addressPolled" variable is reset to zero). To detect the idle time, the rs_interrupt()
// ISR updates the tLastInterrupt attribute, to save the current time (in microseconds). 
// As part of the main loop of the program, checkPolling() is called as often as possible to see
// if the last RS-bus interrupt was more than 4 ms (4000 microseconds) ago.
// Since the decoder can (re)start in the middle of a polling cyclus, checkPolling() also sets the
// "masterIsSynchronised" attribute once the start of a new RS-bus polling cyclus is detected. 
//
// The actual data transfer is performed by the Interrupt Service Routine (instead of the main loop)
// to ensure that:
// 1) data is send immediately after the feedback module gets its turn, and
// 2) to ensure only a single byte is send per polling cyclus (flow control).
//
// Data is offered from the "RSbusConnection" object to the ISR using a "data2send" variable, 
// in combination with a "data2sendFlag" and an "addres2use".
// The prepare4bit() or prepare8bit() methods of the "RSbusConnection" object ensure that this
// "data2send" variable is used in accordance with the RS-bus specification, which means that the
// parity, TT-bits, nibble (high or low) and four data bits will be set. The prepare4bit() and
// prepare8bit() methods do not send the data themselves, but store it in a FIFO buffer. 
// This FIFO queue is emptied by the checkConnection() method, which should be called from the
// main program loop as frequent as possible. Once the ISR is ready for new data
// ("data2sendFlag == 0"), the data is taken by the checkConnection() method from the FIFO.
// 
// After start-up, or after the master station resets, the decoder should first connect to the 
// master by sending two 4 bit messages (the high and low order nibble) in two consequetive cycles. 
// To signal that such connect is needed, the "needConnect" flag is set by the checkConnection()
// method. Upon start-up, this flag is initialised to zero (no need to connect yet, since the
// beginning of a new polling cyclus has not yet been detected). This flag is also set to zero
// if the RS-bus master resets (which the master indicates by sending a puls of 88 ms, followed
// by silence period of roughly 562 ms). The check_RS_bus() method maintains a state machine to
// determine if we are in the start-up phase, if the start of a new polling cyclus has been detected,
// if we already have send both connection nibbles and if we are connected.
//
// Used hardware and software:
// - INTx: used to receive RS-bus information from the command station
// - TXD/TXDx: used to send RS-bus information to the command station (USART)
// - the Arduino micros() function
//
//  Ralation between the files:
//                                      +-----------------------+
//                                      |                       |
//                                      |         Main          |
//                                      |                       |
//                                      +-----------+-----------+
//                                                  |
//                                                  V
//                                      +-----------+-----------+
//                                      |                       |
//                  +-------------------+         RSBUS         |
//                  |                   |                       |
//                  |                   +-----------+-----------+
//                  |                               |
//                  V                               V
//      +-----------+-----------+       +-----------+-----------+       +-----------------------+
//      |                       |       |                       |       |                       |
//      |         FIFO          |       |          ISR          +------>|         USART         |
//      |                       |       |                       |       |                       |
//      +-----------------------+       +-----------------------+       +-----------------------+
//
//
//******************************************************************************************************
#include <Arduino.h>
#include <util/parity.h>      // Needed to calculate the parity bit
#include <RSbus.h>
#include "support_isr.h"
#include "support_fifo.h"


//******************************************************************************************************
// The rsISR object is declared in "support_isr.cpp" and listens to the master's polling signal. 
// The rsUSART object is declared in "support_usart.cpp" and sends messages to the master. 
extern volatile RSbusIsr rsISR;  // Defined in support_isr
extern USART rsUSART;            // Defined in support_usart. We will only use the init() methoud
RSbusHardware rsbus_hardware;    // The object that checks the timing of RS-bus events


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

RSbusHardware::RSbusHardware() {
  masterIsSynchronised = 0;  // No begin of next polling cyclus detected yet
}

void RSbusHardware::attach(int tx_pin, int rx_pin) {
  // Step 1: attach the interrupt to the RSBUS_RX pin.
  // Triggering on the falling edge allows immediate sending after the right number of pulses
  attachInterrupt(digitalPinToInterrupt(rx_pin), rs_interrupt, FALLING );
  // Store the pin number, to allow a detach later
  rx_pin_used = rx_pin;
  // STEP 2: initialise the RS bus hardware
  rsUSART.init(tx_pin);
}

void RSbusHardware::detach(void) {
  detachInterrupt(digitalPinToInterrupt(rx_pin_used));
}


//************************************************************************************************
// checkPolling() is called from the main loop. If there has been no RS-bus activity for 4 msec,
// it needs to reset the addressPolled attribute.   
//  
//    ____      ____                  4 msec                     ____    RS-bus ISR():
//   |    |    |    |                   |                       |    |   - addressPolled++
//  _|    |____|    |___________________v_______________________|    |   - tLastInterrupt = micros()
//                                      
//  -----------------------------------><----------------------------><--------------------------
//      addressPolled > 0                    addressPolled = 0             addressPolled > 0
//  
//                                       checkPolling() {
//                                         if (addressPolled != 0)
//                                           if ((micros() - tLastInterrupt) > 4 msec) 
//                                             addressPolled = 0;
//                                       
//************************************************************************************************
void RSbusHardware::checkPolling(void) {
  // make sure the ISR is not calling micros() in parallel
  noInterrupts();
  unsigned long T_interval = micros() - rsISR.tLastInterrupt;
  interrupts();
  if (rsISR.addressPolled != 0) {    
    if (T_interval >= 4000) {      
      // A new RS-bus cycle has started. Reset addressPolled
      // If 130 addresses were polled, layer 1 works fine
      if (rsISR.addressPolled == 130) masterIsSynchronised = 1;
        else {masterIsSynchronised = 0;  }
      rsISR.addressPolled = 0;
    }    
  }
  if (T_interval > 10000) masterIsSynchronised = 0;         // more than 10ms of silence
  if (masterIsSynchronised == 0) rsISR.data2sendFlag = 0;   // cancel possible data waiting for ISR

}


//******************************************************************************************************
//******************************************************************************************************
// The RSbusConnection class is needed for establishing a connection to the master station and
// sending RS-bus messages. Per address a separate connection is needed.
//******************************************************************************************************
// Define the location of the various bits within a RS-bus message
const uint8_t DATA_0    = 7;      // feedback 1 or 5
const uint8_t DATA_1    = 6;      // feedback 2 or 6
const uint8_t DATA_2    = 5;      // feedback 3 or 7
const uint8_t DATA_3    = 4;      // feedback 4 or 8
const uint8_t NIBBLEBIT = 3;      // low or high order nibble
const uint8_t TT_BIT_0  = 2;      // this bit must always be 0
const uint8_t TT_BIT_1  = 1;      // this bit must always be 1
const uint8_t PARITY    = 0;      // parity bit; will be calculated by software
  

RSbusConnection::RSbusConnection() {
  // Note: the following kind of RS-bus modules exist (see also http://www.der-moba.de/):
  // - 0: accessory decoder without feedback
  // - 1: accessory decoder with RS-Bus feedback (this would be the default case)
  // - 2: feedback module for the RS-Bus (can not switch anything)
  // - 3: Reserved for future use
  address = 0;                                 // Initialise to 0
  type = Switch;                               // Default value
  status = NotConnected;                       // state machine starts NotConnected
  needConnect = 0;                             // Initialise to 0
}


void RSbusConnection::format_nibble(uint8_t value) {
  // Input is a byte, representing 4 feedback bits, plus one nibble bit
  // 1) the routine sets the TT and parity bits
  // 2) it stores the formatted byte in the FIFO
  // Define the bits of the RS-bus "packet" (1 byte)
  // Note: least significant bit (LSB) first. Thus the parity bit comes first,
  // immediately after the USART's start bit. Because of this (unusual) order, the USART hardware
  // can not calculate the parity bit itself; such calculation must be done in software.
  unsigned char parity;
  // Step 1A: set the "type" bit
  if (type == Switch)   {value |= (1<<TT_BIT_0) | (0<<TT_BIT_1);}  // switch decoder with feedback
  if (type == Feedback) {value |= (0<<TT_BIT_0) | (1<<TT_BIT_1);}  // feedback module
  // Step 1B: Set the parity bit
  parity = parity_even_bit(value);              // check parity
  if (parity)                                   // if parity is even
    {value |= (0<<PARITY);}                     // clear the parity bit
    else {value |= (1<<PARITY);}                // set the parity bit
  // Step 2: store the formatted "data byte" into the FIFO queue 
  // Data in this queue will later be send (using the USART) from send_nibble(). 
  my_fifo.push(value);
}


void RSbusConnection::send4bits(Nibble_t nibble, uint8_t value) {
  // Takes a 4 bit value to create a single RS-bus nibble and stores this nibble in a FIFO 
  // This nibble is later send to the RS-bus master station using sendnibble() 
  // nibble = 1..2, specifies if we use the first or the second nibble
  // data   = 0..15  
  uint8_t data;
  uint8_t nibbleValue;
  if (nibble == LowBits) nibbleValue = 0;
    else nibbleValue = 1;
  data = ((value & 0b00000001) <<7)  // move bit 7 to bit 0 (distance = 7)
       | ((value & 0b00000010) <<5)  // move bit 6 to bit 1 (distance = 5)
       | ((value & 0b00000100) <<3)  // move bit 5 to bit 2 (distance = 3)
       | ((value & 0b00001000) <<1)  // move bit 4 to bit 3 (distance = 1)
       | (nibbleValue<<NIBBLEBIT);
  format_nibble(data);
}

    
void RSbusConnection::send8bits(uint8_t value) {
  // Takes an 8 bit value to create two RS-bus nibbles and stores these nibbles in a FIFO 
  // These nibbles will later be send to the RS-bus master station using sendnibble() 
  // data = 0..255  
  // Note that bit order should be changed.
  const uint8_t NIBBLEBIT = 3;       // low or high order nibble
  uint8_t data;
  uint8_t nibbleValue;
  // Sending 8 bits is sufficient to connect to the master 
  needConnect = 0;                             
  // send first nibble (for the low order bits
  data = ((value & 0b00000001) <<7)  // move bit 7 to bit 0 (distance = 7)
       | ((value & 0b00000010) <<5)  // move bit 6 to bit 1 (distance = 5)
       | ((value & 0b00000100) <<3)  // move bit 5 to bit 2 (distance = 3)
       | ((value & 0b00001000) <<1)  // move bit 4 to bit 3 (distance = 1)
       | (0<<NIBBLEBIT);
  format_nibble(data);
  // send second nibble (for the high order bits)
  data = ((value & 0b00010000) <<3)  // move bit 3 to bit 0 (distance = 3)
       | ((value & 0b00100000) <<1)  // move bit 2 to bit 1 (distance = 1)
       | ((value & 0b01000000) >>1)  // move bit 1 to bit 2 (distance = -1)
       | ((value & 0b10000000) >>3)  // move bit 0 to bit 3 (distance = -3)
       | (1<<NIBBLEBIT);
  format_nibble(data);
}



uint8_t RSbusConnection::sendNibble(void) {
  // Perform the checks here, since this part is less time critical then the ISR part, which get its input from here
  uint8_t result = 0;                          // Function return value
  if (my_fifo.size() > 0) {                    // We have data to send
    if (rsISR.data2sendFlag == 0) {            // And the USART (rs_interrupt routine) can accept new data
      if ((address > 0) && (address <= 128)) { // And the address for this connection has been initialized and is valid
        rsISR.address2use = address;           // Use the address that belongs to this connection
        rsISR.data2send = my_fifo.pop();       // Take the oldest element from the FIFO
        rsISR.data2sendFlag = 1;               // Tell the rs_interrupt routine that it should send data
        result = 1;                            // Succesfully presented the nibble to the rs_interrupt routine
      }
    }
  }   
  return result; 
}


void RSbusConnection::checkConnection(void) {
  // This function maintains the statemachine for the RS-bus connection, and should be called from main frequestly
  if (rsbus_hardware.masterIsSynchronised) {   // The decoder has detected the start of a new polling cyclus
    switch (status) {
      case NotConnected : 
        status = ConnectionIsNeeded;           // status is used for our internal (private) statemachine
        needConnect = 1;                       // used as external (public) flag towards main() and send8bits()
        break;
      case ConnectionIsNeeded: 
        if (needConnect == 0) status = ConnectNibble1;
        break;
      case ConnectNibble1:
        if (RSbusConnection::sendNibble()) status = ConnectNibble2;
        break;
      case ConnectNibble2:
        if (RSbusConnection::sendNibble()) status = Connected;
        break;
      case Connected:
        RSbusConnection::sendNibble();
        break;
    }
  }
  else {
    status = NotConnected;                     // No RS-bus signal, so connection is lost
    my_fifo.empty();                           // Drop all data that is still waiting in the FIFO for transmission
  }
}

   
