#ifndef FS3_COMMON_INCLUDED
#define FS3_COMMON_INCLUDED

////////////////////////////////////////////////////////////////////////////////
//
//  File          : fs3_common.h
//  Description   : This file contains definitions common to the client and 
//                  server executable of the FS3 filesystem.
//
//  Author        : Patrick McDaniel
//  Last Modified : Thu 21 Oct 2021 07:03:31 AM EDT
//

// Include Files
// Project Include Files
#include <fs3_controller.h>

//
// Global Data 
extern unsigned long FS3ControllerLLevel;     // Controller log level
extern unsigned long FS3DriverLLevel;         // Driver log level
extern unsigned long FS3SimulatorLLevel;      // Driver log level
extern unsigned long FS3CacheLLevel;          // Cache log level
extern unsigned long FS3ExtendedDebugLLevel;  // Extended debugging level


#endif
