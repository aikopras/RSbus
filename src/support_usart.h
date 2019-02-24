#ifndef rsbus_usart_h
#define rsbus_usart_h
//************************************************************************************************
//
// file:      rsbus_usart.h
// purpose:   Support file for the RS-bus library. Initialise the USART,
//            and return a pointer to the USART data register being used
// history:   2019-01-30 ap V0.1 Initial version
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
// The tx_pin parameter can take one of the following values:
// - 1   => Serial   - All standard Arduino boards
// - 18  => Serial1  - MEGA
// - 16  => Serial2  - MEGA
// - 14  => Serial3  - MEGA
// - 9   => TXD      - Mightycore - ATmega 8535/16/32
// - 9   => TXD0     - Mightycore - ATmega 164/324/644/1284
// - 11  => TXD1     - Mightycore - ATmega 164/324/644/1284 
//
// After initialisation "dataRegister" points to the USART data register being used
// If the tx_pin has an invalid value, a pointer to "dummy_byte" is returned
//
// To send a data_byte, use: (*rsUSART.dataRegister) = data_byte;
//
//************************************************************************************************
#include <Arduino.h>


class USART {
  public:
    USART(void);                      // Constructor, initializes the dataRegister
    void init(int tx_pin );           // Used by rsbus.cpp: selects and sets the right USART registers
    volatile uint8_t *dataRegister;   // Used by support_isr.cpp: pointer to the USART data register

  private:
    uint8_t dummy_byte;
};


#endif
