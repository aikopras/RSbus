#ifndef rsbus_usart_h
#define rsbus_usart_h
//************************************************************************************************
//
// file:      rsbus_usart.h
// purpose:   Support file for the RS-bus library. Initialise the USART,
//            and return a pointer to the USART data register being used
// history:   2019-01-30 ap V0.1 Initial version
//            2021-07-16 ap V0.2 Changed the way the USART is selected
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
// The parameter `usartNumber` determines which USART will be used:
// - 0  => TX(0) - All boards; used for Serial
// - 1  => TX1   - Used for Serial1
// - 2  => TX2   - Used for Serial2
// - 3  => TX3   - Used for Serial3
//
// After initialisation "dataRegister" points to the USART data register being used
// If the usartNumber has an unsupported value, a pointer to "dummy_byte" is returned
//
// To send a data_byte, use: (*rsUSART.dataRegister) = data_byte;
//
//************************************************************************************************
#include <Arduino.h>


class USART {
  public:
    USART(void);                      // Constructor, initializes the dataRegister
    void init(uint8_t usartNumber);   // Used by rsbus.cpp: selects and sets the right USART registers
    volatile uint8_t *dataRegister;   // Used by support_isr.cpp: pointer to the USART data register

  private:
    uint8_t dummy_byte;
};


#endif
