# RSbus #

This Arduino library can be used to send feedback information from a decoder to the master station via the (LENZ) RS-bus. The RS-bus is the standard feedback bus used for Lenz products, and supported by several other vendors. As opposed to some other feedback systems, the RS-Bus implements a current loop (instead of voltage levels) for signalling, making the transmission less susceptible to noise and interference. RS-Bus packets also include a parity bit to provide some form of error detection. For more information on the RS-bus see the [der-moba website](http://www.der-moba.de/index.php/RS-Rückmeldebus) (in German) and [https://sites.google.com/site/dcctrains/rs-bus-feed](https://sites.google.com/site/dcctrains/rs-bus-feed).

The RS-bus supports a maximum of 128 feedback addresses, numbered 1 to 128. The master polls all 128 addresses in a sequential order to ask if they have data to send. Per address 8 bits of feedback data can be send; individual feedback messages carry only 4 of these bits (a nibble), so 2 messages are needed to send all 8 bits. Note that this library supports multiple RS-bus addresses per decoder. For each address a separate RS-Bus connection should be established.

If no feedback module sends information, one complete polling cyclus takes 33,1 ms. On the other hand, if all feedback modules send information, one polling cyclus takes 33,1 + 128 * 1,875 = 273,1 ms. Since feedback modules can only send half of their data (one nibble) during one polling cyclus, roughly 550 ms may be needed to send 8 bits.

Used hardware and software:
- INTx (an interrupt pin): used to receive RS-bus polling pulses from the command station
- TXD/TXDx (a USART): used to send RS-bus messages to the command station
- the Arduino micros() function


## The RSbusHardware class ##
The RSbusHardware class initialises the USART for sending the RS-bus messages, and the Interrupt Service Routine (ISR) used for receiving the RS-bus pulses send by the master to poll each decoder if it has data to send. Most AVRs have a single USART, but the ATMega 162, 164 and 644 have two, while the 1280 and 2560 as well as newer AtMegaX controllers (such as 4809, 128DA) have 4 or even 6 USARTs.

To specify which USART to use, a parameter exists in the attach function.
Note that many of the standard Arduino boards (such as the UNO and Nano) will also use their single USART for serial communication with the Arduino monitor. Joint operation of the serial monitor and the RS-Bus software will likely cause interference resulting in errors.


- ### attach() ###
Should be called at the start of the program to select the USART (0...5) and connect the RX pin (Arduino pin number) to the RS-bus Interrupt Service Routine (ISR).
For newer ATMegaX processors an optional "swap pins" boolean may be included to indicate that the USART should use the alternative pins.

- ### detach() ###
Should be called in case of a (soft)reset of the decoder, to stop the ISR.

- ### checkPolling() ###
Should be called as often as possible from the program's main loop. The RS-bus master sequentially polls each decoder. After all decoders have been polled, a new polling cyclus will start. checkPolling() maintains the polling status.

- ### masterIsSynchronised ###
A flag maintained by checkPolling() and used by objects from the RSbusConnection class to determine if the start of the (first) polling cyclus has been detected and the master is ready to receive feedback data.


## The RSbusConnection class ##
For each address this decoder uses a dedicated RSbusConnection object should be created by the main program. To connect to the master station, each RSbusConnection object should start with sending all 8 feedback bits to the master. Since RS_bus messages carry only 4 bits of user data (a nibble), the 8 bits will be split in 4 low and 4 high bits and send in two consecutive messages.

- ### address ###
The address used by this RS-bus connection object. Valid values are: 1..128

- ###  needConnect  ###
A flag indicating to the user that a connection should be established to the master station. If this flag is set, the main program should react with calling `send8bits()`, to tell the master the value of all 8 feedback bits. This flag can be ignored if the main program always sends all 8 feedback bits using `send8bits()`, and never use `send4bits()` for sending only part of our feedback bits.

Note that it is the responsibility of the user program (instead of the library) to connect to the master station, since only the user program knows the correct value of the feedback bits. Especially in case of short power drops (as a result of a temporary shortcut) it may be important to immediately send the correct feedback values, to avoid that train controller software gets confused.

- ### type ###
A variable that specifies the type of decoder. The default value is `Switching receiver with feedback decoder`, but this may be changed into `Stand-alone feedback decoder`. The decoder type is conveyed in the RS-bus messages towards the master, which in turn will forward this information on request of a handheld device, such as the LH100, or PC software, such as traincontroller. In case of switch decoders, handhelds use the type information to display to the user the current switch position and that the switch is feedback capable. In case of feedback decoders, handhelds use the type information to display to the user the value of the feedback bits (see the LH100 manual for details).

- ### send4bits() ###
Sends a single 4 bit message (nibble) to the master station. We have to specify whether the high or low order bits are being send. Note that the data will not immediately be send, but first be stored in an internal FIFO buffer until the address that belongs to this object is called by the master.

- ### send8bits() ###
Sends two 4 bit messages (nibbles) to the master station. Note that the data will not immediately be send, but first be stored (as two nibbles) in an internal FIFO buffer until the address that belongs to this object is called by the master.

- ### checkConnection() ###
Should be called as often as possible from the program's main loop. It maintains the connection logic and checks if data is waiting in the FIFO buffer. If data is waiting, it checks if the USART and RS-bus ISR are able to accept that data. The RS-bus ISR waits till its address is being polled by the master, and once it gets polled sends the RS-bus message (carrying 4 bits of feedback data) to the master.

# Details #

    enum Decoder_t { Switch, Feedback };
    enum Nibble_t  { HighBits, LowBits };

    class RSbusHardware {
      public:
        uint8_t masterIsSynchronised;

        RSbusHardware();
        void attach(uint8_t usartNumber, uint8_t rx_pin);
        void detach(void);
        void checkPolling(void);
    }


    class RSbusConnection {
      public:
        uint8_t address;
        uint8_t needConnect;
        Decoder_t type;

        RSbusConnection();
        void send4bits(Nibble_t nibble, uint8_t value);
        void send8bits(uint8_t value);
        void checkConnection(void);
    }

# Printed Circuit Boards (PCBs) #
The schematics and PCBs are available from my EasyEda homepage [EasyEda homepage](https://easyeda.com/aikopras),
* [RS-Bus piggy-back print](https://easyeda.com/aikopras/rs-bus-tht)
* [SMD version of the RS-Bus piggy-back print](https://easyeda.com/aikopras/rs-bus-smd)
