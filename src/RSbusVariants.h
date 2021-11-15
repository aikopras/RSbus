//************************************************************************************************
//
// file:      rsbusVariants.h
//
// purpose:   User editable file to choose another RS-Bus ISR approach
//
// This source file is subject of the GNU general public license 3,
// that is available at the world-wide-web at http://www.gnu.org/licenses/gpl.txt
//
// history:   2021-10-16 ap V1.0 Initial version
//
//
// For different ATmega controllers different RS-bus routines exist
//
// The default approach (V2)
// =========================
// The default approach is the software-based approach, where an interrupt is raised after each
// RS-bus transition. The advantage of this approach is that it works with all controllers, but
// the disadvantage is that it puts more load on the CPU and may therefore interfere with other
// timing sensitive code. No pre-compiler directive is needed for this case.
//
// RSBUS_USES_RTC (V2)
// ===================
// An alternative approach is to use the Real Time Clock (RTC) of the modern ATMegaX and DxCore
// processors (such as 4808, 4809, 128DA48 etc). This code puts less load on the CPU but has as
// disadvantage that the RS-Bus input signal MUST be connected to pin PA0. 
//
// RSBUS_USES_TCBx (V2)
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
// #define RSBUS_USES_RTC           // DxCore and MegaCoreX
// #define RSBUS_USES_SW_4MS        // Compatable with previous versions of the RS-bus library 

// DxCore only:
// #define RSBUS_USES_TCB0          // The default version AP_DCC_Library also uses TCB0 
// #define RSBUS_USES_TCB1          // The default version of the servo library also uses TCB1
// #define RSBUS_USES_TCB2          // The default version DxCore uses TCB2 for millis() / micros()
// #define RSBUS_USES_TCB3          // Only available on 48 and 64 pin DxCore processors
// #define RSBUS_USES_TCB4          // Only available on 64 pin DxCore processors


//************************************************************************************************
// Check if the selected alternative runs on this hardware
//************************************************************************************************
#if defined(RSBUS_USES_RTC)
  #ifndef RTC
  #error "The selected RS-bus code does not run on this hardware"
  #endif
#elif defined(RSBUS_USES_TCB0)
  #ifndef TCB0_CNT
  #error "The selected RS-bus code does not run on this hardware"
  #endif
#elif defined(RSBUS_USES_TCB1)
  #ifndef TCB1_CNT
  #error "The selected RS-bus code does not run on this hardware"
  #endif
#elif defined(RSBUS_USES_TCB2)
  #ifndef TCB2_CNT
  #error "The selected RS-bus code does not run on this hardware"
  #endif
#elif defined(RSBUS_USES_TCB3)
  #ifndef TCB3_CNT
  #error "The selected RS-bus code does not run on this hardware"
  #endif
#elif defined(RSBUS_USES_TCB4)
  #ifndef TCB4_CNT
  #error "The selected RS-bus code does not run on this hardware"
  #endif
#endif


 
