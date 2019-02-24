# RSbus
This Arduino library sends feedback information from a decoder to the master station via the RS-bus
A RS-bus supports a maximum of 128 feedback addresses, numbered 1 to 128. The master polls all 128 addresses in a sequential order to ask if they have data to send. Per address 8 bits of feedback data can be send; individual feedback messages carry only 4 of these bits (a nibble), so 2 messages are needed to send all 8 bits. Note that this library supports multiple RS-bus addresses per decoder. For each address a separate RS-Bus connection should be established.
