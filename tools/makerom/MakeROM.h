/***************************************************************

  Project:
	PrcDump - Utility to dump the contents of a PalmOS .prc file

  Copyright info:

	This is free software; you can redistribute it and/or modify
	it as you like.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.


  FileName:
		PrcDump.h

  Description:
		This is the main header file for the PrcDump utility

  History:
		11-6-98  RM - Created by Ron Marianetti
****************************************************************/

// Include standard C headers
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <assert.h>

#include <extras.h>
#include <unistd.h>

#include "PalmTools.h"

#include <SystemPublic.h>

#include "PalmMemEquates.h"


// --------------------------------------------------------------
// Constants/defines
// --------------------------------------------------------------
#define	appProgName	  "MakeROM"


// --------------------------------------------------------------
// We maintain the ROM Tokens we need to add to the ROM in
//  an array of these structures
// --------------------------------------------------------------
typedef struct
  {
	Boolean	ptr;	  // if true, store token as a pointer and put
					  //  data at next free space after last ROMBlock
	UInt32	token;
	UInt16	len;
	void*	dataP;	  // pointer to malloc'ed data
  } AppROMTokenType;



// --------------------------------------------------------------
// Globals
// --------------------------------------------------------------
extern UInt32			  CardBase ;
extern UInt32			  HdrOffset;
extern Boolean			  OptResetAt0;

extern char*			  ROMBootDbNameP;
extern char*			  ROMDBNameP[];
extern UInt32			  NumROMDBs;

extern UInt32			  ROMBlock[];
extern UInt32			  RAMBlock[];

// Pointer to ROM image and it's size.
extern UInt32			  ROMImageSize;
extern void*			  ROMImageP;

// List of tokens for the ROM Image
extern UInt32			  NumROMTokens;
extern AppROMTokenType	  ROMTokens[];


// Pointer to ROM heap
extern Mem3HeapHeaderType*	PrvHeapP;
extern UInt8*				PrvFreeP;

// --------------------------------------------------------------
// Device functions for dealing with the memory card
// --------------------------------------------------------------
void*	DevMemLocalIDToPtr (UInt32 localID);
UInt32	DevMemPtrToLocalID (void* p);
UInt32	DevMemHeapInit (void* heapP, UInt32 size);
void*	DevMemPtrNew (UInt32 size, UInt8 ownerID, Boolean atEnd);
UInt32	DevMemPtrSize (void* p);


// --------------------------------------------------------------
// App Utility functions
// --------------------------------------------------------------
UInt32	AppReadFile (char* filenameP, void** dataPP, UInt32* sizeP);
UInt32	AppReadPrcResource (char* filenameP, UInt32 resType, UInt16 resID,
						  void** dataPP, UInt32* sizeP);
void	AppPrintType ( UInt32 type);

UInt16	Crc16CalcBigBlock(void* bufP, UInt32 count, UInt16 crc);

// --------------------------------------------------------------
// UInt8 Swapping functions
// --------------------------------------------------------------
inline void PToH(UInt8& /*v*/)
  {}

inline void PToH(Int8& /*v*/)
  {}

inline void PToH(UInt16& v)
  { v = PToHW (v); }

inline void PToH(Int16& v)
  { v = PToHW (v); }

inline void PToH(UInt32& v)
  { v = PToHDw (v); }

inline void PToH(Int32& v)
  { v = PToHDw (v); }


void PToH (CardHeaderType& p);
void PToH (RecordListType& list);
void PToH (DatabaseHdrType& db);




void PToH (StorageHeaderType& store);
