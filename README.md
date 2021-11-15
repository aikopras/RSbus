# RSbus (Version 2) #

This Arduino library can be used to send feedback information from a decoder to the master station via the (LENZ) RS-bus. The RS-bus is the standard feedback bus used for Lenz products, and supported by several other vendors. As opposed to some other feedback systems, the RS-Bus implements a current loop (instead of voltage levels) for signalling, making the transmission less susceptible to noise and interference. RS-Bus packets also include a parity bit to provide some form of error detection, and the command station notifies the feedback decoders if it has detected such parity error. For more information on the RS-bus see the [der-moba website](http://www.der-moba.de/index.php/RS-Rückmeldebus) (in German) and [https://sites.google.com/site/dcctrains/rs-bus-feed](https://sites.google.com/site/dcctrains/rs-bus-feed). To understand the design decisions behind, and implementation of this library, see the file [Basic operation](extras/BasicOperation.md) for further details.

The RS-bus supports a maximum of 128 feedback addresses, numbered 1 to 128. The master polls all 128 addresses in a sequential order to ask if they have data to send. Per address 8 bits of feedback data can be send; individual feedback messages carry only 4 of these bits (a nibble), so 2 messages are needed to send all 8 bits. Note that this library allows a decoder to have multiple RS-bus addresses. For each address a separate RS-Bus connection should be established.

If no feedback module sends information, one complete polling cycle takes 33,1 ms. On the other hand, if all feedback modules send information, one polling cycle takes 33,1 + 128 * 1,875 = 273,1 ms. Since feedback modules can only send half of their data (one nibble) during one polling cycle, roughly 550 ms may be needed to send 8 bits.

## Used hardware and software ##
Two Arduino pins are needed for this library, as well as some software:
- A receive pin `rxPin`: needed to receive RS-bus polling pulses from the command station
- A transmit pin (and USART): needed to send RS-bus messages to the command station
- The timer associated with the Arduino millis() function
- Optional (depends on micro-controller and decoding approach, see below): Real Time Clock or one of the TCB timers. Installation of a MightyCore, MegaCore, MegaCoreX or DxCore board may be necessary (see for the URL the references below).


#### Receive pin ####
The choice of receive pin depends on the micro-controller being used, as well as decoding variant. See also the respective board info to determine the Arduino pin number that belongs to the (External Interrupt) port being used.
- For traditional ATMega processors, such as the 8535, 16 and the 328 (used by the Arduino UNO and Nano), `rxPin` must be one of the two External Interrupt pins: INT0 or INT1. INT0 is generally PD2, INT1 is generally PD3.
- The ATMega 2560 has eight External Interrupt pins: INT0 - INT7. `rxPin` must be one of these pins.
- The MegaCoreX processors, such as the 4808 (Nano Thinary) and 4809 (Nano Every) can use all digital pins as `rxPin`. However, due to the higher number of external interrupt pins, determining which pin raised the interrupt routine takes several microseconds extra (compared to the traditional ATMega processors). For these micro controllers a better approach is therefore to use the RTC decoding variant. See [Basic operation](extras/BasicOperation.md) for further details.
- The same holds for DxCore processors, such as the AVR-DA and AVR-DB series. In addition, DxCore processors also support the use of one of the Timer-Counters B to count RS-bus pulses. Using a TCB has as advantage that basically any pin can be used as `rxPin`, but the disadvantage is that TCB timers may be scarce resources. Again see [Basic operation](extras/BasicOperation.md) for further details.

#### Transmit pin ####
The transmit pin must be one of the USART TX-pins. However, for this library we should not provide the Arduino pin number, but the USART number. Most traditional ATMega processors support a single USART only. More powerful processors, such as the 2560 and newer MegaCoreX and DxCore processors support 4 or even more USARTS. See [src/support_usart.cpp](src/support_usart.cpp) for details.
Below an overview of (some) processors that support multiple USARTs.
- 2 USARTS: 64, 128, 162, 164, 324, 328PB, 644, 1281, 1284, 2561
- 4 USARTS: 640, 1280, 2560, 4808, 4809
- 6 USARTS: AVR-DA, AVR-DB

## Decoding approaches ##
Version 2 (V2) of the RS-bus library supports different RS-bus routines for different ATMega controllers and software.
To select a different approach, modify the file [src/rsbusVariants.h](src/rsbusVariants.h).

- **The default approach (V2):**
  The default approach is the software-based approach, where an interrupt is raised after each RS-bus transition. The advantage of this approach is that it works with all controllers, but the disadvantage is that it puts more load on the CPU and may therefore interfere with other timing sensitive code. See [Basic operation](extras/BasicOperation.md) for details. ***=> works on all ATMega micro-processors.***

- **RSBUS_USES_RTC (V2):**
  An alternative approach is to use the Real Time Clock (RTC) of the modern ATMegaX and DxCore processors (such as 4808, 4809, 128DA48 etc). This code puts less load on the CPU but has as disadvantage that the RS-Bus input signal *MUST* be connected to pin PA0 (ExtClk). This approach requires installation of the MegaCoreX or DxCore board software. ***=> recommended for newer processors on MegaCoreX / DxCore boards. Requires pin PA0!***

- **RSBUS_USES_TCBx (V2):**
  A similar approach is to use one of the five TCBs of a DxCore processor as event counter. For that purpose the TCB should be used in the "Input capture on Event" mode. That mode exists on novel DxCore (such as the 128DA48) processors, but not on MegaCoreX (such as 4808, 4809) or earlier processors. Like the RTC approach, this approach puts limited load on the CPU,  but as opposed to the RTC approach we have (more) freedom in choosing the RS-bus input pin. This approach requires installation of the DxCore board software. ***Supported only on DxCore boards. Useful if pin PA0 is not available.***

- **RSBUS_USES_SW_4MS (V1):**
  This was the default version in the previous release (V1) of the RS-bus library. Instead of checking every 2ms for a period of silence, we check every 4ms. This may be slightly more efficient, but doesn't allow the detection of parity errors. ***=> included for compatibility reasons.***

## The RSbusHardware class ##
The RSbusHardware class initialises the USART for sending the RS-bus messages, and the Interrupt Service Routine (ISR) used for receiving the RS-bus pulses send by the master.

- #### void attach(uint8_t usartNumber, uint8_t rxPin) ####
Should be called at the start of the program to select the USART (0...5) and connect the RS-bus Receive pin (Arduino pin number) to the RS-bus Interrupt Service Routine (ISR).
The attach() functions checks two additional parameters: `interruptModeRising` and `swapUsartPin`.

- #### void detach(void) ####
Should be called before of a (soft)reset of the decoder, to stop the ISR.

- #### void checkPolling(void) ####
Should be called as often as possible from the program's main loop. The RS-bus master sequentially polls each decoder. After all decoders have been polled, a new polling cycle will start. checkPolling() keeps the polling administration up-to-date.

- #### bool masterIsSynchronised ####
A flag maintained by checkPolling() and used by objects from the RSbusConnection class to determine if the start of the (first) polling cycle has been detected and the master is ready to receive feedback data.

- #### bool interruptModeRising (default: true) ####
Determines when the RS-bus ISR is triggered: at the RISING edge of the RS-bus polling signal or at the FALLING edge.
Default is at the RISING edge. See See [the file BasicOperation-TriggerEdge.md](extras/BasicOperation-TriggerEdge.md) for further details.

- #### bool swapUsartPin (default: false) ####
The MegaCoreX and DxCore processors have the possibility to swap the USART pins to alternative pins. This may be useful with boards that do not make available all pins of the micro-controller, but also in cases where RTC-based decoding is used. The RTC needs pin PA0 for clock (RS-bus) input, but this pin is also used by the USART0. By setting `swapUsartPin` USART0 will use Pin PA4 instead of PA0. See [support_usart.cpp](src/support_usart.cpp) for furter details.

- #### bool parityErrorFlag ####
This flag indicates if the Command System has detected a parity error somewhere during the last polling cycle. The flag is automatically cleared after a subsequent polling cycle (provided no new parity errors were detected). The flag can be tested in the main program, and used to retransmit the value from the previous cycle.

- #### uint8_t parityErrors ####
This counter increases after each parity error that has been detected. A high value indicates transmission problems on the RS-bus. Another error source may be that the same USART is used for both RS-bus data transmission, as well as standard Arduino Serial communication.


## The RSbusConnection class ##
For each address this decoder uses a dedicated `RSbusConnection` object, that should be instantiated by the main program. To connect to the master station, each `RSbusConnection` object should start with sending all 8 feedback bits to the master, using `send8bits()`.

- #### uint8_t address ####
The address used by this RS-bus connection object. Valid values are: 1..128.

- #### bool needConnect ####
A flag indicating to the user that a connection should be established to the master station. If this flag is set, the main program should react with calling `send8bits()`, to tell the master the value of all 8 feedback bits. This flag can be ignored if the main program always sends all 8 feedback bits using `send8bits()`, and never use `send4bits()` for sending only part of our feedback bits.

  Note that it is the responsibility of the user program (instead of the library) to connect to the master station, since only the user program knows the correct value of the feedback bits. Especially in case of short power drops (as a result of a temporary shortcut) it may be important to immediately send the correct feedback values, to avoid that train-controller software gets confused.

- #### Decoder_t type ####
A variable that specifies the type of decoder. The default value is 'Stand-alone feedback decoder', but this may be changed into 'Switching receiver with feedback decoder'. The decoder type is conveyed in the RS-bus messages towards the master, which in turn will forward this information on request of a handheld device, such as the LH100, or PC software, such as train-controller. In case of switch decoders, handhelds use the type information to display to the user the current switch position and that the switch is feedback capable. In case of feedback decoders, handhelds use the type information to display to the user the value of the feedback bits (see the LH100 manual for details).
Decoder_t is an enumeration with two values: {Switch, Feedback}.

- #### void send4bits(Nibble_t nibble, uint8_t value) ####
Sends a single 4 bit message (nibble) to the master station. We have to specify whether the high or low order bits are being send. Note that the data will not immediately be send, but first be stored in an internal FIFO buffer until the address that belongs to this object is polled by the master. Only the four lower order bits of value are used (0..15).
Nibble_t is an enumeration with two values: {HighBits, LowBits}.

- #### void send8bits(uint8_t value) ####
Sends two 4 bit messages (nibbles) to the master station. Note that the data will not immediately be send, but first be stored (as two nibbles) in an internal FIFO buffer until the address that belongs to this object is polled by the master.

- #### void checkConnection(void) ####
Should be called as often as possible from the program's main loop. It maintains the connection logic and checks if data is waiting in the FIFO buffer. If data is waiting, it checks if the USART and RS-bus ISR are able to accept that data. The RS-bus ISR waits till its address is being polled by the master, and once it gets polled sends the RS-bus message (carrying 4 bits of feedback data) to the master.

# Example #
```
#include <Arduino.h>
#include <RSbus.h>

// The following parameters specify the hardware that is being used and the RS-Bus address
const uint8_t ledPin = 8;            // Pin for the LED. Usually Pin 13
const uint8_t RsBus_USART = 0;       // Use USART-0. On most boards TX, TXD, TX0 or TXD0
const uint8_t RsBus_RX = 10;         // INTx: one of the interrupt pins
const uint8_t RS_Address = 100;      // Must be a value between 1..128

// Instatiate the objects being used
// The RSbushardware object is responsible for the RS-Bus ISR and USART.
// The RSbusConnection object maintains the data channel with the master and formats the messages
extern RSbusHardware rsbusHardware;  // This object is defined and instantiated in rs_bus.cpp
RSbusConnection rsbus;               // Per RS-Bus address we need a dedicated object
unsigned long T_last;                // For testing: we send 1 message per second
uint8_t value;                       // The value we will send over the RS-Bus
uint8_t nibble;                      // HighBits or LowBits

void setup() {
  rsbusHardware.attach(RsBus_USART, RsBus_RX);
  rsbus.address = RS_Address;        // 1.. 128
  value = 0;                         // Initial value
  pinMode(ledPin, OUTPUT);
}

void loop() {
  // For testing purposes we try every second to send 8 bits (2 messages)
  if ((millis() - T_last) > 1000) {
    T_last = millis();
    if (rsbusHardware.masterIsSynchronised) {
      digitalWrite(ledPin, 1);       // We have a valid RS-Bus signal
      rsbus.send8bits(value);        // Tell the library to buffer these 8 bits for later sending
      switch (value) {
        case 0   : value = 1;   break;
        case 1   : value = 2;   break;
        case 2   : value = 4;   break;
        case 4   : value = 8;   break;
        case 8   : value = 16;  break;
        case 16  : value = 32;  break;
        case 32  : value = 64;  break;
        case 64  : value = 128; break;
        case 128 : value = 1;   break;
      }
    }
    else digitalWrite(ledPin, 0);    // No valid RS-Bus signal
  }
  // Next functions should be called from main as often as possible
  // If a RS-Bus feedback decoder starts, it needs to connect to the master station
  if (rsbus.needConnect) rsbus.send8bits(0);
  rsbusHardware.checkPolling();      // Listen to RS-Bus polling messages for our turn
  rsbus.checkConnection();           // Check if the buffer contains data, and give this to the ISR and USART
}
```

# Release notes #
The current release (2.0.0) of this RS-bus code relies for the TCB-based decoding routine still on the event library as found in DxCore release 1.3.6. That version of the event library does not (yet) support the use of Arduino pin numbers to specify the RS-bus input pin. Instead we will use a fixed pin (PA0). Newer versions of the event library are expected to support Arduino pin numbers in the attach() method. Once this newer version is available, this software will be updated to support complete freedom in choosing the RS-bus input pin.  
References:
 - [https://github.com/SpenceKonde/DxCore](https://github.com/SpenceKonde/DxCore)
 - [https://github.com/SpenceKonde/DxCore/tree/master/megaavr/libraries/Event](https://github.com/SpenceKonde/DxCore/tree/master/megaavr/libraries/Event)

# Printed Circuit Boards (PCBs) #
The schematics and PCBs are available from my EasyEda homepage [EasyEda homepage](https://easyeda.com/aikopras),
- [RS-Bus piggy-back print](https://easyeda.com/aikopras/rs-bus-tht)
- [SMD version of the RS-Bus piggy-back print](https://easyeda.com/aikopras/rs-bus-smd)

# References #
- Der-Moba (in German): http://www.der-moba.de/index.php/RS-Rückmeldebus
- https://sites.google.com/site/dcctrains/rs-bus-feed
- MegaCoreX: see [MCUdude](https://github.com/MCUdude)
- DxCore: see [SpenceKonde](https://github.com/SpenceKonde)
