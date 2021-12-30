////////////////////////////////////////////////////////////////////////////////
//
//  File           : fs3_common.c
//  Description    : This file contains implementationsand data common to the
//                   client and server of the FS3 filesystem.
//
//  Author         : Patrick McDaniel
//  Last Modified  : 
//

// Includes

// Project Includes
#include <fs3_common.h>

// Definitions

//
// Global data

unsigned long FS3ControllerLLevel = 0;     // Controller log level (global)
unsigned long FS3DriverLLevel = 0;         // Driver log level (global)
unsigned long FS3SimulatorLLevel = 0;      // Driver log level (global)
unsigned long FS3CacheLLevel = 0;          // Cache log level
unsigned long FS3ExtendedDebugLLevel = 0;  // Extended debugging level

//
// Implementation

