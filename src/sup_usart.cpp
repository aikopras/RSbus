//******************************************************************************************************
//
// file:      sup_usart.cpp
// purpose:   Support file for the RS-bus library. Select and initialise the USART for 
//            transmission, and return a pointer to the USART data register. 
//            Note that for reception of RS-bus messages the USART is not used.
//
// history:   2019-01-30 ap V0.1 Initial version
//            2021-07-16 ap V0.2 Changed the way the USART is selected
//            2021-07-19 ap V0.3 Added support for the 4809 and AVR-Dx controllers
//            2025-03-08 lr V0.4 Addes support for ATtiny series 0, 1 & 2
//
// Based on the `usartNumber` parameter, this code allows selection of which USART will be used
// for RS-Bus transmission. After the USART is selected, the USART will be initialised as follows: 
// 8 data bits, no parity, 1 stop bit, asynchronous mode and 4800 baud transmission.
//
// After the USART is initialised, the `dataRegister` variable will point to the USART's data register
// and the user sketch can send data bytes as follows:
//    (*rsUSART.dataRegister) = data_byte; 
//
// Number of USARTs
// ================
// Many of the `old` ATMega processors support a single USART only. Examples of such
// processors are the 8535, 16 and the 328 (the latter is used in the Arduino UNO and Nano).
// More powerful processors, such as the 2560 (used in the Arduino Mega) and newer
// processors such as the 4809 (used in the Arduino Every) support 4 or even more USARTS.
// Below an overview of (some) processors that support multiple USARTs.
// - 1 USART: `Old` ATMega processors, ATtiny Series 0 and 1
// - 2 USARTS: 64, 128, 162, 164, 324, 328PB, 644, 1281, 1284, 2561, ATtiny series 2
// - 4 USARTS: 640, 1280, 2560, 4808, 4809
// - 6 USARTS: AVR-DA, AVR-DB
// Note that the first USART on Arduino boards may also be used for the serial monitor.
// On such boards combined use of both RS-Bus and serial monitor may cause problems.
//
// USART Data Register names
// =========================
// For old ATMega processors, such as the 8535 and 16A, the data register is called UDR.
// For later ATMega processors, such as the 328 and 2560, the data register are called UDR0, UDR1, UDR2 and UDR3.
// For the newest ATMegaX processors, such as the 4808, 4809, ATtiny Series 0/1 & 2, DA and DB, the data registers
// are called USART0.TXDATAL, USART1.TXDATAL, USART2.TXDATAL etc. Note that there are also TXDATAH registers,
// which are used for sending a ninth data bit, if needed.
//
// New __AVR_MEGAX__ processors
// ============================
// All of the AVR processors are known to the compiler as __AVR_MEGA__ processors,
// but the newer processors have the additional __AVR_MEGAX__ compiler directive. 
// In literature these newer processors are also known as the megaAVR®-0, tinyAVR®-0,
// tinyAVR®-1, tinyAVR®-2 AVR-DA, AVR-DB, AVR-DD, AVR-EA and AVR-EB Devices
//
// With these new __AVR_MEGAX__ processors, Microchip introduced a novel C-like register
// naming structure and coding style. The consequence of introducing novel register names is 
// that old code must be modified to work with these newer processors. In this .cpp file
// the code is therefore split into 2 parts: one for the newer processors (identified by the 
// __AVR_MEGAX__ compiler directive) and the other for the traditional __AVR_MEGA__ processors.
//
// A good introduction to this novel C-like naming structure can be found in TB3262:
// -  AVR1000b: Getting Started with Writing C-Code for AVR® MCUs
// A good introduction for writing USART code for the new processors is TB3216:
// - Getting Started with Universal Synchronous and Asynchronous Receiver and Transmitter (USART) 
//
// Alternative Pins
// ================
// An interesting feature of the __AVR_MEGAX__ processors, is that the USARTs need not always be
// connected to their default pins, but may alternatively be connected to other pins. 
// In this .cpp code, this alternative pin assignment will be performed if the (optional) `defaultPins` 
// parameter in the init() call is set to false.
//
// To perform this alternative pin assignment, the Port Multiplexer (PORTMUX) should be used. For the 
// USARTs 0..3 we should use PORTMUX.USARTROUTEA; for the USARTs 4..5 it is PORTMUX.USARTROUTEB.
//
// Processors within a certain family but with a lower number of pins will not implement all USARTS. 
// In the code below #ifdef xxx constructs are therefore used to prevent the compiler from generating
// calls to non-existent USART registers. If a (non-AVR) processor is selected that has no or different
// type of USARTS, a "human understandable" error message will be displayed during compile time.
//
// Although the compiler will not generate calls to non-supported USART registers, the compiler does
// not check if the user sketch selects non-existing registers.
// If the user sketch provides an unsupported usartNumber, "dataRegister" will point to a dummy variable 
// and the boolean variable `noUsart` will be set.
//
// As opposed to the `old` AVR processors, for the new  __AVR_MEGAX__ processors the USART TX pin 
// must explicitly be defined as output, which implies that this .cpp code needs to know to which pins
// the USART will be connected. 
// 
// The comments in the __AVR_MEGAX__ code below explains for each case which pins are default,
// and which may be used as alternatives.
//
// Ols ATMega processors, mapping between Arduino digital pin numbers and TX Pins
// ==============================================================================
// - 1   => Serial  (TXD0) - UNO, NANO 
// - 1   => Serial1 (TXD1) - NANO Every 
// - 2   => Serial2 (TXD2) - NANO Every 
// - 6   => Serial3 (TXD3) - NANO Every 
// - 2   => Serial  (TXD0) - Thinary Every 
// - 0   => Serial2 (TXD2) - Thinary Every 
// - 1   => Serial  (TXD0) - Mega / MegaCore Mega layout
// - 18  => Serial1 (TXD1) - Mega / MegaCore Mega layout
// - 16  => Serial2 (TXD2) - Mega / MegaCore Mega layout
// - 14  => Serial3 (TXD3) - Mega / MegaCore Mega layout
// - 9   => Serial  (TXD)  - Mightycore standard layout - ATmega 8535/16/32
// - 9   => Serial  (TXD0) - Mightycore standard layout - ATmega 164/324/644/1284
// - 11  => Serial1 (TXD1) - Mightycore standard layout - ATmega 164/324/644/1284
// - 0   => Serial  (TXD0) - MegaCoreX (48pin) - ATMega 4809
// - 14  => Serial1 (TXD1) - MegaCoreX (48pin) - ATMega 4809
// - 34  => Serial2 (TXD2) - MegaCoreX (48pin) - ATMega 4809
// - 8   => Serial3 (TXD3) - MegaCoreX (48pin) - ATMega 4809

// Testing
// =======
// This source has been tested on:
// 1) Standard `old` Arduino AVR boards, such as the Arduino UNO, Arduino Nano, Arduino Mega.
// 2) New Arduino megaAVR boards, such as the Arduino Nano Every
// 3) MegaCore boards with the ATMEGA 2560
// 4) MightyCore boards with the ATMega 16A
// 5) MegaCoreX boards with the ATMega 4809
// 6) DxCore boards with the AVR128DA48 (Curiosity Nano)
//
// Compile errors??
// ================
// If compiled using an "Arduino megaAVR Board" with Register Emulation: 'ATMEGA328', 
// the following error will be shown: 'class PORTCClass' has no member named 'DIR'
// Make sure to select Register Emulation: 'NONE'
//
// License
// =======
// This source file is subject of the GNU general public license 3,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
//******************************************************************************************************
#include <Arduino.h>
#include "sup_usart.h"

// Instantiate the rsUSART object from the USART class
USART rsUSART;


// Define the constructor
USART::USART(void) {
  // Initialise the data register to a place that can be written even if init() fails.
  dataRegister = &dummy_byte;
}

void USART::init(uint8_t usartNumber, bool defaultPins) {
  // Messages should be: 8 bit, no parity, 1 stop bit, asynchronous mode, 4800 baud.
  //
  // The code has two main parts:
  // 1) For newer __AVR_XMEGA__ processors, that support a C-like interface to access the USART
  // 2) For traditional __AVR_MEGA__ processors.

  //****************************************************************************************************
  // Part 1: __AVR_XMEGA__ processors (such as 4808, 4809, AVR DA, AVR DB, ATtiny series 0, 1 & 2, ...)
  //****************************************************************************************************
  #if defined(__AVR_XMEGA__) 

    // Set the baudrate. 
    // Baudrate formula for the `new` Xmega controllers 
    // See: TB3216-Getting-Started-with-USART-DS90003216.pdf
    #define USART_BAUD_RATE(BAUD_RATE) ((float)(F_CPU * 64 / (16 * (float)BAUD_RATE)) + 0.5)
    // Set the USART registers and the dataRegister pointer  
    // The __AVR_XMEGA__ processors may have upto 6 USARTS
    switch (usartNumber) {
      case 0:
        #ifdef  USART0
        #define HAS_USART 
        USART0.BAUD = (uint16_t)USART_BAUD_RATE(4800);
        USART0.CTRLB |= USART_TXEN_bm;  // Turn on transmission (but not reception!) circuitry
        dataRegister = &USART0.TXDATAL; // The low 8-bit USART data register        
        #endif
      break;
      case 1:
        #ifdef  USART1
        #define HAS_USART 
        USART1.BAUD = (uint16_t)USART_BAUD_RATE(4800);
        USART1.CTRLB |= USART_TXEN_bm;  // Turn on transmission (but not reception!) circuitry
        dataRegister = &USART1.TXDATAL; // The low 8-bit USART data register        
        #endif
      break;
      case 2:
        #ifdef  USART2
        #define HAS_USART 
        USART2.BAUD = (uint16_t)USART_BAUD_RATE(4800);
        USART2.CTRLB |= USART_TXEN_bm;  // Turn on transmission (but not reception!) circuitry
        dataRegister = &USART2.TXDATAL; // The low 8-bit USART data register        
        #endif
      break;
      case 3:
        #ifdef  USART3
        #define HAS_USART 
        USART3.BAUD = (uint16_t)USART_BAUD_RATE(4800);
        USART3.CTRLB |= USART_TXEN_bm;  // Turn on transmission (but not reception!) circuitry
        dataRegister = &USART3.TXDATAL; // The low 8-bit USART data register        
        #endif
      break;
      case 4:
        #ifdef  USART4
        #define HAS_USART 
        USART4.BAUD = (uint16_t)USART_BAUD_RATE(4800);
        USART4.CTRLB |= USART_TXEN_bm;  // Turn on transmission (but not reception!) circuitry
        dataRegister = &USART4.TXDATAL; // The low 8-bit USART data register        
        #endif
      break;
      case 5:
        #ifdef  USART5
        #define HAS_USART 
        USART5.BAUD = (uint16_t)USART_BAUD_RATE(4800);
        USART5.CTRLB |= USART_TXEN_bm;  // Turn on transmission (but not reception!) circuitry
        dataRegister = &USART5.TXDATAL; // The low 8-bit USART data register        
        #endif
      break;
    }
    
    // Determine which pin should be used for TX and set this pin as output
    // This depends on the processor being used

    #if defined(__AVR_TINY_0__) || defined(__AVR_TINY_1__)
    //==================================================================================================
    // The ATtiny Series 0 and 1 have a single USART only
    // For 8-pin devices, the default pin is PA6 and the alternative pin is PA1
    // For the other devices, the default pin is PB2 and the alternative pin is PA1
      #ifdef  USART0
        #if (_AVR_PINCOUNT == 8)
          bitClear(PORTMUX.CTRLB,0);     //reset value to default
          if (defaultPins) 
            PORTA.DIR |= PIN6_bm;        // Set the default pin as output on PB2
          else {
            bitSet(PORTMUX.CTRLB,0);     //set alternativ pin
            PORTA.DIR |= PIN1_bm;        // Set the default pin as output on PA1
          }
        #else
          bitClear(PORTMUX.CTRLB,0);     //reset value to default
          if (defaultPins) 
            PORTB.DIR |= PIN2_bm;        // Set the default pin as output on PB2
          else {
            bitSet(PORTMUX.CTRLB,0);     //set alternativ pin
            PORTA.DIR |= PIN1_bm;        // Set the default pin as output on PA1
          }
        #endif
      #endif

    #elif defined(__AVR_TINY_2__)
    //==================================================================================================
    // The ATtiny Series 2 has 1 or 2 USARTs
      switch (usartNumber) {
        case 0:
          #ifdef  USART0
          // USART0 uses as default TX pin PB2, and as alternative PA1
          // Initialise at the default port, by clearing the PORTMUX USART0 fields
          PORTMUX.USARTROUTEA &= ~(PORTMUX_USART0_gm);
          if (defaultPins) {
            PORTB.DIR |= PIN2_bm;    // Set the default pin as output
          }
          else {
            // Set the PORTMUX USART0 fields to the alternative pin
            PORTMUX.USARTROUTEA |= PORTMUX_USART00_bm; 
            PORTA.DIR |= PIN1_bm;    // Set the alternative pin as output          
          }
          #endif
        break;
 
        case 1:
          #ifdef  USART1
          // USART1 uses as default TX pin PA1 and PC2 as alternative
          // Initialise at the default port, by clearing the PORTMUX USART1 fields
          PORTMUX.USARTROUTEA &= ~(PORTMUX_USART1_gm);
          if (defaultPins) {
            PORTA.DIR |= PIN1_bm;    // Set the default pin as output
          }
          else {
            // Set the PORTMUX USART0 fields to the alternative pin
            PORTMUX.USARTROUTEA |= PORTMUX_USART10_bm; 
            PORTC.DIR |= PIN2_bm;    // Set the alternative pin as output          
          }
          #endif
        break;
      }

    #else
    //==================================================================================================
      // The DxCore processors may have upto 6 USARTS
      switch (usartNumber) {
        case 0:
          #ifdef  USART0
          // USART0 uses as default TX pin PA0, and as alternative PA4
          // Initialise at the default port, by clearing the PORTMUX USART0 fields
          PORTMUX.USARTROUTEA &= ~(PORTMUX_USART0_gm);
          if (defaultPins) {
            PORTA.DIR |= PIN0_bm;    // Set the default pin as output
          }
          else {
            // Set the PORTMUX USART0 fields to the alternative pin
            PORTMUX.USARTROUTEA |= PORTMUX_USART00_bm; 
            PORTA.DIR |= PIN4_bm;    // Set the alternative pin as output          
          }
          #endif
        break;
 
        case 1:
          #ifdef  USART1
          // USART1 uses as default TX pin PC0, and as alternative PC4
          // Initialise at the default port, by clearing the PORTMUX USART1 fields
          PORTMUX.USARTROUTEA &= ~(PORTMUX_USART1_gm);
          if (defaultPins) {
            PORTC.DIR |= PIN0_bm;    // Set the default pin as output
          }
          else {
            // Set the PORTMUX USART0 fields to the alternative pin
            PORTMUX.USARTROUTEA |= PORTMUX_USART10_bm; 
            PORTC.DIR |= PIN4_bm;    // Set the alternative pin as output          
          }
          #endif
        break;
 
        case 2:
          #ifdef  USART2
          // USART2 uses as default TX pin PF0, and as alternative PF4
          // Initialise at the default port, by clearing the PORTMUX USART2 fields
          PORTMUX.USARTROUTEA &= ~(PORTMUX_USART2_gm);
          if (defaultPins) {
            PORTF.DIR |= PIN0_bm;    // Set the default pin as output
          }
          else {
            // Set the PORTMUX USART2 fields to the alternative pin
            PORTMUX.USARTROUTEA |= PORTMUX_USART20_bm; 
            PORTF.DIR |= PIN4_bm;    // Set the alternative pin as output          
          }
          #endif
        break;
 
        case 3:
          #ifdef  USART3
          // USART3 uses as default TX pin PB0, and as alternative PB4
          // Initialise at the default port, by clearing the PORTMUX USART3 fields
          PORTMUX.USARTROUTEA &= ~(PORTMUX_USART3_gm);
          if (defaultPins) {
            PORTB.DIR |= PIN0_bm;    // Set the default pin as output
          }
          else {
            // Set the PORTMUX USART3 fields to the alternative pin
            PORTMUX.USARTROUTEA |= PORTMUX_USART30_bm; 
            PORTB.DIR |= PIN4_bm;    // Set the alternative pin as output          
          }
          #endif
        break;

        case 4:
          #ifdef  USART4
          // USART4 uses as default TX pin PE0, and as alternative PE4
          // Initialise at the default port, by clearing the PORTMUX USART4 fields
          PORTMUX.USARTROUTEB &= ~(PORTMUX_USART4_gm);
          if (defaultPins) {
            PORTE.DIR |= PIN0_bm;    // Set the default pin as output
          }
          else {
            // Set the PORTMUX USART4 fields to the alternative pin
            PORTMUX.USARTROUTEA |= PORTMUX_USART40_bm; 
            PORTE.DIR |= PIN4_bm;    // Set the alternative pin as output          
          }
         #endif
        break;

        case 5:
          #ifdef  USART5
          // Initialise at the default port, by clearing the PORTMUX USART5 fields
          PORTMUX.USARTROUTEB &= ~(PORTMUX_USART5_gm);
          if (defaultPins) {
            PORTG.DIR |= PIN0_bm;    // Set the default pin as output
          }
          else {
            // Set the PORTMUX USART5 fields to the alternative pin
            PORTMUX.USARTROUTEA |= PORTMUX_USART50_bm; 
            PORTG.DIR |= PIN4_bm;    // Set the alternative pin as output          
          }
          #endif
        break;
      }
    #endif

  //****************************************************************************************************
  // Part 2: Other __AVR_MEGA__ processors (such as 8535, 16, 328, 2560, ...)
  //****************************************************************************************************
  #elif defined(__AVR_MEGA__) || defined(__AVR_ATmega8535__)    // ATMega 328, 2560, 16, 8535, etc...
    // Step 1: Define which baudrate will be used
    // Step 2: Initialise USART Control and Status Register B
    // This register is responsible for (amongst others) activating (receive and) transmission circuitry
    // Step 3: Initialise USART Control and Status Register C
    // This register determines (amongst others) what type of serial format is being used
    // We use 8 bit (no parity, 1 stop bit, asynchronous mode)
    // Step 4: Set the baudrate register to 4800 baud
    // See: http://www.github.com/abcminiuser/avr-tutorials/blob/master/USART/Output/USART.pdf?raw=true

    // Baudrate and Prescaler for the standard `old` ATmega controllers 
    #define BAUD_RATE 4800
    #define BAUD_PRESCALE ((((F_CPU / 16) + (BAUD_RATE / 2)) / (BAUD_RATE)) - 1)

    switch (usartNumber) {

      case 0:
        #ifdef UDR                     // ATmega 8535/16/32
        #define HAS_USART 
        dataRegister = &UDR;
        UCSRB |= (1 << TXEN);          // Control and Status Register B
        UCSRC |= (1 << URSEL)          // Register C. Use URSEL bit!
              |  (1 << UCSZ0)          // Character size bit 0
              |  (1 << UCSZ1);         // Character size bit 1
        UBRRL  = BAUD_PRESCALE;        // Load lower 8-bits of the baud rate value
        UBRRH = (BAUD_PRESCALE >> 8);  // Load upper 8-bits of the baud rate
        //
        #elif defined (UDR0)           // The standard `old` ATmega boards
        #define HAS_USART 
        dataRegister = &UDR0;
        UCSR0B |= (1 << TXEN0);        // Turn on transmission (but not reception!) circuitry
        UCSR0C |= (1 << UCSZ00)        // Use 8 bit
               |  (1 << UCSZ01);       //
        UBRR0L  = BAUD_PRESCALE;       // Load lower 8-bits of the baud rate value
        UBRR0H = (BAUD_PRESCALE >> 8); // Load upper 8-bits of the baud rate
        #endif
      break;
 
      case 1:
        #ifdef UDR1 
        #define HAS_USART 
        dataRegister = &UDR1;
        UCSR1B |= (1 << TXEN1);        // Turn on transmission (but not reception!) circuitry
        UCSR1C |= (1 << UCSZ10)        // Use 8 bit 
               |  (1 << UCSZ11);       // 
        UBRR1L  = BAUD_PRESCALE;       // Load lower 8-bits of the baud rate value
        UBRR1H = (BAUD_PRESCALE >> 8); // Load upper 8-bits of the baud rate
        #endif
      break;
 
      case 2:
        #ifdef UDR2 
        #define HAS_USART 
        dataRegister = &UDR2;
        UCSR2B |= (1 << TXEN2);        // Turn on transmission (but not reception!) circuitry
        UCSR2C |= (1 << UCSZ20)        // Use 8 bit 
               |  (1 << UCSZ21);       // 
            UBRR2L  = BAUD_PRESCALE;       // Load lower 8-bits of the baud rate value
            UBRR2H = (BAUD_PRESCALE >> 8); // Load upper 8-bits of the baud rate
        #endif
      break;
 
      case 3:
        #ifdef UDR3 
        #define HAS_USART 
        dataRegister = &UDR3;
        UCSR3B |= (1 << TXEN3);        // Turn on transmission (but not reception!) circuitry
        UCSR3C |= (1 << UCSZ30)        // Use 8 bit 
               |  (1 << UCSZ31);       // 
        UBRR3L  = BAUD_PRESCALE;       // Load lower 8-bits of the baud rate value
        UBRR3H = (BAUD_PRESCALE >> 8); // Load upper 8-bits of the baud rate
        #endif
      break;
    }
  
  if (defaultPins) {;}                 // Only included to avoid compiler warnings.
  #endif                               // End of conditional code

  //****************************************************************************************************
  // Raise an error if no USART support is found
  //****************************************************************************************************
  #ifndef HAS_USART
  #error No USART could be found that is supported by this processor
  #endif
  noUsart = false;
  if (dataRegister == &dummy_byte) {
    noUsart = true;                // the selected usartNumber is too high: no USART at this number
  }
}


  
