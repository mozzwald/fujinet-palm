/***************************************************************

  Copyright info:

	This is free software; you can redistribute it and/or modify
	it as you like.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.


  FileName:
		PalmTools.h

  Description:
		This is a common header file for the PalmTools

  History:
		11-6-98  RM - Created by Ron Marianetti
****************************************************************/

// -------------------------------------------------
// Pragmas to control compilation
//---------------------------------------------------
// Visual C compiler warnings
//#pragma warning ( disable : 4103; disable : 4121; disable : 4214)
//#pragma warning ( disable : 4068;) // unknown pragma
//#pragma warning ( disable : 4514;) // unused inline function removed

// Structure packing on byte boundaries for GCC compiler
#pragma pack(1)




// -------------------------------------------------
// Defines for PalmOS headers
//---------------------------------------------------
#define CMD_LINE_BUILD


// Define the Host CPU type automatically, if possible
// JCM - added defined(i386), necessary for Linux
#ifndef CPU_TYPE
  #if defined (_M_IX86) || defined (_X86_) || defined (i386)
	#define	CPU_TYPE			1					  // CPU_X86
	#define	EMULATION_LEVEL		1					  // EMULATION_WINDOWS
  #endif
#endif




// Include Palm Equates
#include <PalmTypes.h>

// --------------------------------------------------------------
// Byte Swapping functions
// --------------------------------------------------------------
#if EMULATION_LEVEL == EMULATION_NONE

  #define HToPW(w)	  (w)
  #define HToPDw(dw)  (dw)
  #define PToHW(w)	  (w)
  #define PToHDw(dw)  (dw)

#else
  #if (CPU_TYPE == CPU_68K)
	#define HToPW(w)	  (w)
	#define HToPDw(dw)	  (dw)
	#define PToHW(w)	  (w)
	#define PToHDw(dw)	  (dw)

  #elif (CPU_TYPE == CPU_x86)

	#define HToPW(w)	  (UInt16)( ((w & 0x00FF) << 8) | ((w & 0xFF00) >> 8) )
	#define HToPDw(dw)	  (UInt32)( ((dw & 0x000000FF) << 24) | ((dw & 0x0000FF00) << 8) \
							|((dw & 0x00FF0000) >> 8)  | ((dw & 0xFF000000) >> 24) )
	#define PToHW(w)	  HToPW(w)
	#define PToHDw(dw)	  HToPDw(dw)

  #else

	#error  "CPU type unknown"

  #endif

#endif
