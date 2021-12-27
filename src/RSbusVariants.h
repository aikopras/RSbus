//************************************************************************************************
//
// file:      rsbusVariants.h
//
// purpose:   User editable file to choose another RS-Bus ISR approach
//            For traditional ATMega processors the default is: RSBUS_USES_SW
//            For MegaCoreX and DxCore processors with 40 pins or more
//            the default is: RSBUS_USES_SW_TCB3
//            Only modify this file if the default doesn't seem appropriate.
//            Cases where this may apply might be MegaCoreX and DxCore processors
//            that have 32 pins or less, and therefore don't implement TCB3.
//
// This source file is subject of the GNU general public license 3,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
// history:   2021-10-16 ap V1.0 Initial version
//            2021-12-13 ap V1.1 Restructured, and selects TCB3 as default when possible
//
//
// For different ATmega controllers different RS-bus routines exist
//
// RSBUS_USES_SW (V2)
// ==================
// The default approach is the software-based approach, where an interrupt is raised after each
// RS-bus transition. The advantage of this approach is that it works with all controllers, but
// the disadvantage is that it puts more load on the CPU and may therefore interfere with other
// timing sensitive code.
//
// RSBUS_USES_SW_TCBx (V2)
// =======================
// For AtMegaX and DxCore processors a more efficient and reliable approach is to replace the
// standard external pin interrupt (which is initialised using attachInterrupt()), with a TCB
// interrupt that gets triggered via the Event System. This is therefore the default approach
// for AtMegaX and DxCore processors that implement TCB3; which are processors with 40 pins
// or more. Examples are the 4809 and AVR128DA48.
// Compared to RSBUS_USES_SW, this approach has the following advantages:
// - TCB interrupts triggered via the Event System are faster than external pin interrupts
// - TCB can efficiently measure the precise duration of each RS-bus pulse, and thereby
//   improve reliability
// - Noise cancelation is possible if a TCB is configured as Event user. Short spikes on the
//   RS-Bus RX pin will than be filtered, and reliability will again be improved.
// TCB0 may already be in use by the AP-DCC-Lib, TCB1 by the servo lib and TCB2 by millis().
// Therefore TCB3 was selected as default.
//
// RSBUS_USES_RTC (V2)
// ===================
// An alternative approach is to use the Real Time Clock (RTC) of the modern ATMegaX and DxCore
// processors (such as 4808, 4809, 128DA48 etc). This code puts less load on the CPU but has as
// disadvantage that the RS-Bus input signal MUST be connected to pin PA0. 
//
// RSBUS_USES_HW_TCBx (V2)
// ====================
// A similar approach is to use one of the five TCBs of a DxCore processor as event counter.
// For that purpose the TCB should be used in the "Input capture on Event" mode. That mode exists
// on novel DxCore (such as the 128DA48) processors, but not on MegaCoreX (such as 4808, 4809)
// or earlier processors. Like the RTC approach, this approach puts limited load on the CPU, 
// but as opposed to the RTC approach we have (more) freedom in choosing the RS-bus input pin.
//  
// RSBUS_USES_SW_4MS (V1)
// ======================
// This is an older version of the default approach. Instead of checking every 2ms for a period
// of silence, we check every 4ms. This may be slightly more efficient, but doesn't allow the
// detection of parity errors.
//
//************************************************************************************************
// To use alternative RS-bus code, uncomment ONE of the following lines. 
// #define RSBUS_USES_SW            // Default for traditional Arduino's
// #define RSBUS_USES_RTC           // DxCore and MegaCoreX
// #define RSBUS_USES_SW_4MS        // Compatable with previous versions of the RS-bus library

// DxCore and MegaCoreX:
// #define RSBUS_USES_SW_TCB0       // The default version AP_DCC_Library also uses TCB0
// #define RSBUS_USES_SW_TCB1       // The default version of the servo library also uses TCB1
// #define RSBUS_USES_SW_TCB2       // The default version DxCore uses TCB2 for millis() / micros()
// #define RSBUS_USES_SW_TCB3       // Only available on 48 and 64 pin DxCore processors
// #define RSBUS_USES_SW_TCB4       // Only available on 64 pin DxCore processors

// DxCore only:
// #define RSBUS_USES_HW_TCB0       // The default version AP_DCC_Library also uses TCB0
// #define RSBUS_USES_HW_TCB1       // The default version of the servo library also uses TCB1
// #define RSBUS_USES_HW_TCB2       // The default version DxCore uses TCB2 for millis() / micros()
// #define RSBUS_USES_HW_TCB3       // Only available on 48 and 64 pin DxCore processors
// #define RSBUS_USES_HW_TCB4       // Only available on 64 pin DxCore processors


// If none of the above alternatives was selected, we use a default version
#if !defined(RSBUS_USES_SW)      && !defined(RSBUS_USES_SW_4MS)  && !defined(RSBUS_USES_RTC) && \
    !defined(RSBUS_USES_SW_TCB0) && !defined(RSBUS_USES_SW_TCB1) && !defined(RSBUS_USES_SW_TCB2) && \
    !defined(RSBUS_USES_SW_TCB3) && !defined(RSBUS_USES_SW_TCB4) && \
    !defined(RSBUS_USES_HW_TCB0) && !defined(RSBUS_USES_HW_TCB1) && !defined(RSBUS_USES_HW_TCB2) && \
    !defined(RSBUS_USES_HW_TCB3) && !defined(RSBUS_USES_HW_TCB4)

  // For DxCore processors with 40 pins or higher, the default is TCB3.
  // For MegaCoreX processors with 40 pins or higher, the default is TCB2.
  // For MegaCoreX processors with 28 or 32 pins, the default is RTC.
  // TCB0 is used by the AP_DCC_Lib
  // TCB1 by the servo lib
  // On DxCore, the default for millis() is TCB2 (but can easily be changed)
  // On MegaCoreX with 4 timers, millis() uses TCB3 => we use TCB2
  // On MegaCoreX with 3 timers, millis() uses TCB2 => we use RTC

  #if defined(__AVR_DA__) || defined(__AVR_DB__)
    #if defined(TCB3_CNT)
      #define RSBUS_USES_SW_TCB3
    #endif
  #elif defined(MEGACOREX)
    #if defined(TCB3_CNT)
      #define RSBUS_USES_SW_TCB2
    # else
      #define RSBUS_USES_RTC
    #endif
  #endif

  #if !defined(RSBUS_USES_SW_TCB2) && !defined(RSBUS_USES_SW_TCB3) && !defined(RSBUS_USES_RTC)
    #define RSBUS_USES_SW
  #endif
#endif


//************************************************************************************************
// Check if the selected alternative runs on this hardware
//************************************************************************************************
#if defined(RSBUS_USES_RTC)
  #ifndef RTC
  #error "The selected RS-bus code does not run on this hardware"
  #endif
#elif defined(RSBUS_USES_SW_TCB0) || defined(RSBUS_USES_HW_TCB0)
  #ifndef TCB0_CNT
  #error "The selected RS-bus code does not run on this hardware"
  #endif
#elif defined(RSBUS_USES_SW_TCB1) || defined(RSBUS_USES_HW_TCB1)
  #ifndef TCB1_CNT
  #error "The selected RS-bus code does not run on this hardware"
  #endif
#elif defined(RSBUS_USES_SW_TCB2) || defined(RSBUS_USES_HW_TCB2)
  #ifndef TCB2_CNT
  #error "The selected RS-bus code does not run on this hardware"
  #endif
#elif defined(RSBUS_USES_SW_TCB3) || defined(RSBUS_USES_HW_TCB3)
  #ifndef TCB3_CNT
  #error "The selected RS-bus code does not run on this hardware"
  #endif
#elif defined(RSBUS_USES_SW_TCB4) || defined(RSBUS_USES_HW_TCB4)
  #ifndef TCB4_CNT
  #error "The selected RS-bus code does not run on this hardware"
  #endif
#endif


 
