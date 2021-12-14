
# The RS-bus - Error Handling #

As opposed to other feedback systems that use voltage levels to signal the command station of feedback events, the RS-Bus implements a current loop for signalling. The advantage of current loops is that transmission becomes less susceptible to noise and interference. Still interference remains possible.
RS-Bus packets therefore include a parity bit to provide some form of error detection, and the command station notifies the feedback decoders if it has detected such parity error.
In addition, the feedback decoder itself can also inspect if the RS-bus signal operates as expected. All variants of this software verify if the previous pulse train consists of 130 pulses. The variant that uses the Event System to trigger TCB interrupts, has also the ability to measure the length of each pulse.

## What error sources exist? ##
On Internet fora one may find several discussions on RS-Bus errors, in particular in conjunction with the new LZV 200 Master Station and LDT feedback decoders. Although (unfortunately) I have not been able to test this specific combination, I've performed many other tests and been able to find a number of error sources. Interestingly, most of the errors did not result from bit errors on the RS-Bus itself; it seems that the RS-Bus current loop approach performs remarkably well, provided the RS-Bus cable is drilled.

In fact, the errors that I could reproduce could be related to the decoder hardware itself, and fell in two categories: problems on the decoder's power line or processor input signal interference.
- Feedback decoders that received their power from the DCC signal (instead of a separate 50Hz AC or DC power source), could generate random feedback data if a train would move from booster section A to B. It turned out that in such situations huge spikes could be observed on the DCC signal. It is likely that these spikes are the result of small differences in time (less than microseconds?) between the DCC signals from booster A and B. These spikes caused problems on the (5V) power line of the micro controller, resulting in random behaviour of the micro controller. A simple 100nF / 1 microFarad capacitor over the DCC line was sufficient to remove the spikes, and make (as checked on an oscilloscope) a nice DCC signal.
- Another power related issue observed on my train layout was that startup problems could occur if the Lenz LZV 100 system was powered up before the boosters and other electronics. It is likely that the start-up current by my transformers caused spikes on various power lines. By changing the order in which my transformers were switched on (the LZV100 for the RS-Bus feedback is now switched on last) these start-up problems disappeared.
- Other potential error sources are big electronic consumers, such as TL lights, airco's and heating systems, that are switched on or off. Interestingly these devices did not generate errors on the RS-bus itself, but on the RS-bus input pin of the micro-controller (of the feedback decoder; often an Arduino board). The micro-controller input errors usually last 1 or a few CPU clock cycles, and could be effectively filtered by using the noise cancelation circuitry that can be found on modern ATMega controllers (by setting TCB_FILTER_bm of the EVCTRL register). Still such errors can not 100% be avoided.

**Conclusion:** The RS-Bus itself is remarkably resilient against interference. Most errors are caused by the feedback decoder hardware itself. Detecting and correcting these errors is relatively simple.

## Dealing with errors ##
This RS-bus library provides three approaches to deal with detected or expected errors.

### 1) Retransmission after Parity Errors ###
If the RS-bus command station detects a parity error, it extends the silence period between two pulse trains from 7ms till 10,7ms. If a feedback decoder detects such 10,7ms period of silence, it takes the following actions.

First it increases the `parityErrors` counter. The main sketch may monitor the value of this parameter, and take appropriate actions.

Second it checks the value of the `parityErrorHandling` parameter. If the value if this parameter is zero, it will not perform any additional action. If the value of this parameter is one, it requests the main sketch to retransmit the value of all feedback bits, **provided this decoder has transmitted a feedback message in the previous cycle**. If the value of this parameter is two, it requests the main sketch to retransmit the value of all feedback bits, **irrespective whether this decoder has transmitted a feedback message in the previous cycle or not**.

The default value of the `parityErrorHandling` parameter is one, since the parity error can only originate from this feedback decoder if it has just sent a feedback message.

### 2) Retransmission after Pulse count errors ###
The feedback decoder can also check if the number of pulses within a pulse train match 130. In case a nearby electrical device switches on, spikes on the power line may result, which in turn can generate extra RS-bus pulses. This kind of error seems to be the most common of all errors. If a feedback decoder detects such count errors, it takes the following actions.

First it increases the `pulseCountErrors` counter. The main sketch may monitor the value of this parameter, and take appropriate actions.

Second it checks the value of the `pulseCountErrorHandling` parameter. If the value if this parameter is zero, it will not perform any additional action. If the value of this parameter is one, it requests the main sketch to retransmit the value of all feedback bits, **provided this decoder has transmitted a feedback message in the previous cycle**. If the value of this parameter is two, it requests the main sketch to retransmit the value of all feedback bits, **irrespective whether this decoder has transmitted a feedback message in the previous cycle or not**.

The default value of the `pulseCountErrorHandling` parameter is two, since the injection of extra pulses may trigger *other* feedback decoders that have data waiting in their transmission queue to use the time slot (RS-bus address) that belongs to this decoder. Therefore it may make sense that all feedback decoders send fresh feedback data to the RS-bus command station.

To limit the number of feedback messages, a value of one could also make sense, since it is likely that at least the data send around the time of the interference has to be repeated.

### 3) Forward Error Correction ###
The last approach to increase reliability, it to send each feedback messages multiple times. For that purpose the `forwardErrorCorrection` parameter can be set. Forward error correction is proactive, in the sense that it anticipates that messages may get lost by sending the same message again, irrespective if a transmission error was signalled by the RS-Bus master or not.
Reasonable values for `forwardErrorCorrection` are:
- 0: feedback data is send once,
- 1: after the data is send, 1 copy of the same data is send again, or
- 2: after the data is send, 2 copies of the same data are send again.

Forward Error Correction may be useful in case of sporadic transmission errors of single bits (parity errors), but are less useful in case of error bursts. The default value of the `forwardErrorCorrection` parameter is 0.

A basic example is as follows.
```
#include <Arduino.h>
#include <RSbus.h>

extern RSbusHardware rsbusHardware;  // This object is defined in rs_bus.cpp
RSbusConnection rsbus;               // Per RS-Bus address we need a dedicated object
unsigned long T_last;                // Holds the time of the previous message
uint8_t value;                       // The value that will send over the RS-Bus

void setup() {
  rsbusHardware.attach(0, 2);        // RS-bus Tx uses USART 0, RS-bus Rx uses Arduino Pin 2
  rsbus.address = 123;               // Any address between 1.. 128
  rsbus.forwardErrorCorrection = 1;  // Each feedback message is send twice
}

void loop() {
  if ((millis() - T_last) > 1000) {  // Every second 8 bits
    T_last = millis();  
    if (rsbusHardware.rsSignalIsOK) rsbus.send8bits(value);
    value ++;
    if (value == 255) value = 0;
  }
  if (rsbus.feedbackRequested) rsbus.send8bits(value);
  rsbusHardware.checkPolling();     // Listen to RS-Bus polling messages for our turn
  rsbus.checkConnection();          // Check if the buffer contains data, and give this to the ISR and USART
}
```

## How are retransmissions implemented? ##

Retransmission after a parity or pulse count error is implemented as follows. After `checkPolling()` detects the error, it clears (depending on the value of `parityErrorHandling` and `pulseCountErrorHandling`) the flag `rsSignalIsOK`. This flag is checked by `checkConnection()`, and if it is `false` it resets the internal connection state machine to its initial state, and sets the `feedbackRequested` flag. This flag should be checked by the main sketch. An example, with only the relevant lines of code, is given below.

```
...
extern RSbusHardware rsbusHardware;  // This object is defined in rs_bus.cpp
RSbusConnection rsbus;               // Per RS-Bus address we need a dedicated object
...

void setup() {
  ...
}

void loop() {
  ...
  // The following line (re)initialises the RS-bus Master station to the current value of the feedback bits.
  // Such (re)initialisation is needed after the feedback decoder has (re)started, after the RS-bus master has
  // (re)started, after the RS-bus signal reappears, after a parity error or after a pulse count error.  
  if (rsbus.feedbackRequested) rsbus.send8bits(value);
  rsbusHardware.checkPolling();     // Listen to RS-Bus polling messages for our turn
  rsbus.checkConnection();          // Check if the buffer contains data, and give this to the ISR and USART
}
```

### Note on previous versions of this library ###
- Pre-Arduino versions of this library included Forward Error Correction is an option. The default value was off.
- Arduino versions of this library included Forward Error Correction since Version 2.1
- Retransmission after pulse count errors has been included in all Arduino versions of this library. The `pulseCountErrorHandling` parameter, which allows to tailor this behaviour, was included with version 2.2.
- Retransmission after parity error, including the `parityErrorHandling` parameter, was included with version 2.2.
