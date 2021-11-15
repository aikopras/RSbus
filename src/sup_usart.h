//************************************************************************************************
//
// file:      sup_usart.h
// purpose:   Support file for the RS-bus library. Select and initialise the USART for 
//            transmission, and return a pointer to the USART data register. 
//            Note that for reception of RS-bus messages the USART is not used.
//
// history:   2019-01-30 ap V0.1 Initial version
//            2021-07-16 ap V0.2 Changed the way the USART is selected
//            2021-07-21 ap V0.3 Support added for AVR_XMEGA chips (4809, 128DA, ...)
//
// After initialisation, `dataRegister` points to the selected USART's data register.
// To send a data_byte, the main programm should use:
//    (*rsUSART.dataRegister) = data_byte; 
// Data is transmitted using 8 bit, no parity, 1 stop bit, asynchronous mode and 4800 baud. 
//
// Many of the `old` ATMega processors support a single USART only. Examples of such
// processors are the 8535, 16 and the 328 (which is used in the Arduino UNO and Nano).
// More powerful processors, such as the 2560 (used in the Arduino Mega) and newer
// processors, such as the 4809 (used in the Arduino Every) support 4 or even more USARTS.
//
// The newer AVR_XMEGA chips support the use of alternative pins. For such chips the
// optional boolean defaultPins parameter can be set to `false` to use these alternative pins.
//
// If the specified `usartNumber` is not supported, a pointer to "dummy_byte" is returned
// and the boolean variable `noUsart` is set.
//
// This source file is subject of the GNU general public license 3,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
//************************************************************************************************
# pragma once
#include <Arduino.h>


class USART {
  public:

    // Constructor, initializes the `dataRegister` to a dummy value
    USART(void); 

    // Init() is used by rsbus.cpp to select and initialise the USART register
    void init(uint8_t usartNumber, bool defaultPins = true); 

    // The USART `dataRegister` pointer is set by init() and used by sup_isr.cpp
    volatile uint8_t *dataRegister;     

    // Flags to the user program that no USART exists for this `usartNumber`.
    bool noUsart;                       

  private:
    uint8_t dummy_byte;
};
