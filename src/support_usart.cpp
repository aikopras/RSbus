//******************************************************************************************************
//
// file:      support_usart.cpp
// purpose:   Support file for the RS-bus library. Initialise the USART,
//            and return a pointer to the USART data register being used
// history:   2019-01-30 ap V0.1 Initial version
//
// This source file is subject of the GNU general public license 2,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
// The Arduino and the (standard) mightycore boards support the following USART pins:
// - 1   => Serial   - All standard Arduino boards
// - 18  => Serial1  - MEGA
// - 16  => Serial2  - MEGA
// - 14  => Serial3  - MEGA
// - 9   => TXD      - Mightycore - ATmega 8535/16/32
// - 9   => TXD0     - Mightycore - ATmega 164/324/644/1284
// - 11  => TXD1     - Mightycore - ATmega 164/324/644/1284 
//
// Based on the tx_pin value of the USART::init() function, the selected USART registers will be set.
// Adter initialisation, "dataRegister" will point to UDR, UDR0, UDR1, UDR2 or UDR3
// and data bytes can be send as follows: (*rsUSART.dataRegister) = data_byte; 
// If we use an incorrect pin value, "dataRegister" will point to a dummy variable
//
//******************************************************************************************************
#include <Arduino.h>
#include "support_usart.h"

// Instantiate the rsUSART object from the USART class
USART rsUSART;


// Define the constructor
USART::USART(void) {
  // Initialise the data register to a place that can be written even if init() fails.
  dataRegister = &dummy_byte;
}

void USART::init(int tx_pin) {
  // Messages should be: 8 bit, no parity, 1 stop bit, asynchronous mode, 4800 baud.
  // Step 1: Define which baudrate will be used
  // Step 2: Initialise USART Control and Status Register B
  // This register is responsible for (amongst others) activating (receive and) transmission circuitry
  // Step 3: Initialise USART Control and Status Register C
  // This register determines (amongst others) what type of serial format is being used
  // We use 8 bit (no parity, 1 stop bit, asynchronous mode)
  // Step 4: Set the baudrate register to 4800 baud
  // See: http://www.github.com/abcminiuser/avr-tutorials/blob/master/USART/Output/USART.pdf?raw=true
  #define USART_BAUDRATE 4800
  #define BAUD_PRESCALE ((((F_CPU / 16) + (USART_BAUDRATE / 2)) / (USART_BAUDRATE)) - 1)
  
  if (tx_pin == 1) {               // USART 0 on all Arduino boards
    #ifdef UDR0 
    dataRegister = &UDR0;
    UCSR0B |= (1 << TXEN0);        // Turn on transmission (but not reception!) circuiyty
    UCSR0C |= (1 << UCSZ00)        // Use 8 bit 
           |  (1 << UCSZ01);       // 
    UBRR0L  = BAUD_PRESCALE;       // Load lower 8-bits of the baud rate value
    UBRR0H = (BAUD_PRESCALE >> 8); // Load upper 8-bits of the baud rate
    #endif
  }
  else if (tx_pin == 18) {         // USART 1 on ATMega
    #ifdef UDR1 
    dataRegister = &UDR1;
    UCSR1B |= (1 << TXEN1);        // Turn on transmission (but not reception!) circuiyty
    UCSR1C |= (1 << UCSZ10)        // Use 8 bit 
           |  (1 << UCSZ11);       // 
    UBRR1L  = BAUD_PRESCALE;       // Load lower 8-bits of the baud rate value
    UBRR1H = (BAUD_PRESCALE >> 8); // Load upper 8-bits of the baud rate
    #endif
  }
  else if (tx_pin == 16) {         // USART 2 on ATMega
    #ifdef UDR2 
    dataRegister = &UDR2;
    UCSR2B |= (1 << TXEN2);        // Turn on transmission (but not reception!) circuiyty
    UCSR2C |= (1 << UCSZ20)        // Use 8 bit 
           |  (1 << UCSZ21);       // 
    UBRR2L  = BAUD_PRESCALE;       // Load lower 8-bits of the baud rate value
    UBRR2H = (BAUD_PRESCALE >> 8); // Load upper 8-bits of the baud rate
    #endif
  }
  else if (tx_pin == 14) {         // USART 3 on ATMega
    #ifdef UDR3 
    dataRegister = &UDR3;
    UCSR3B |= (1 << TXEN3);        // Turn on transmission (but not reception!) circuiyty
    UCSR3C |= (1 << UCSZ30)        // Use 8 bit 
           |  (1 << UCSZ31);       // 
    UBRR3L  = BAUD_PRESCALE;       // Load lower 8-bits of the baud rate value
    UBRR3H = (BAUD_PRESCALE >> 8); // Load upper 8-bits of the baud rate
    #endif
  }
  else if (tx_pin == 9) {          // USART 0 on Mightycore
    #ifdef UDR 
    // ATmega 8535/16/32
    dataRegister = &UDR;
    UCSRB |= (1 << TXEN);          // Control and Status Register B
    UCSRC |= (1 << URSEL)          // Register C. Use URSEL bit!
          |  (1 << UCSZ0)          // Character size bit 0
          |  (1 << UCSZ1);         // Character size bit 1
    UBRRL  = BAUD_PRESCALE;        // Load lower 8-bits of the baud rate value
    UBRRH = (BAUD_PRESCALE >> 8);  // Load upper 8-bits of the baud rate
   #elif defined (UDR0)
    // ATmega 164/324/644/1284
    dataRegister = &UDR0;
    UCSR0B |= (1 << TXEN0);        // Turn on transmission (but not reception!) circuiyty
    UCSR0C |= (1 << UCSZ00)        // Use 8 bit 
           |  (1 << UCSZ01);       // 
    UBRR0L  = BAUD_PRESCALE;       // Load lower 8-bits of the baud rate value
    UBRR0H = (BAUD_PRESCALE >> 8); // Load upper 8-bits of the baud rate
    #endif
  }
 else if (tx_pin == 11) {          // USART 1 on Mightycore (ATmega 164/324/644/1284)
    #ifdef UDR1 
    dataRegister = &UDR1;
    UCSR1B |= (1 << TXEN1);        // Turn on transmission (but not reception!) circuiyty
    UCSR1C |= (1 << UCSZ10)        // Use 8 bit 
           |  (1 << UCSZ11);       // 
    UBRR1L  = BAUD_PRESCALE;       // Load lower 8-bits of the baud rate value
    UBRR1H = (BAUD_PRESCALE >> 8); // Load upper 8-bits of the baud rate
    #endif
  }
}
