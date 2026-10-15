/***************************************************************

  Project:
	MakeROM - Utility to dump the contents of a PalmOS .prc file

  Copyright info:

	This is free software; you can redistribute it and/or modify
	it as you like.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.


  FileName:
		Utility.cpp

  Description:
		This file contains utility functions for the MakeROM program

  History:
		11-6-98  RM - Created by Ron Marianetti
****************************************************************/
// --------------------------------------------------------------
// Local includes
// --------------------------------------------------------------
#include "MakeROM.h"


// Global pointer to the ROM heap initialized by DevMemHeapInit
Mem3HeapHeaderType*	  PrvHeapP = 0;

// Global pointer to the free space in the heap
UInt8*				  PrvFreeP = 0;



/***************************************************************
 *	Function:	 PToH (XXX)
 *
 *	Summary:	 UInt8 Swapping Functions for structures we use
 *				  in this app
 *
 *	Parameters:
 *	  XXX	 INOUT  Reference to structure to be byte swapped
 *
 *	Returns:
 *	  void
 *
 *	Called By:
 *	  Routines that need to convert a structure between host
 *		byte order and PalmOS byte order
 *
 *	Notes:
 *	  These are basically copied out of ByteSwapping.cpp. We decided
 *	not to use ByteSwapping.cpp as-is because it relies on new private
 *	header files for PalmOS.
 *
 *	History:
 *	  15-Dec-98 RM	Created by Ron Marianetti
 *
 ****************************************************************/
void PToH (CardHeaderType& cardHdr)
{
  PToH (cardHdr.initStack);
  PToH (cardHdr.resetVector);
  PToH (cardHdr.signature);
  PToH (cardHdr.hdrVersion);
  PToH (cardHdr.flags);
  PToH (cardHdr.version);
  PToH (cardHdr.creationDate);
  PToH (cardHdr.numRAMBlocks);
  PToH (cardHdr.blockListOffset);
  PToH (cardHdr.readWriteParmsOffset);
  PToH (cardHdr.readWriteParmsSize);
  PToH (cardHdr.readOnlyParmsOffset);
  PToH (cardHdr.bigROMOffset);
  PToH (cardHdr.checksumBytes);
  PToH (cardHdr.checksumValue);
  PToH (cardHdr.readWriteWorkingOffset);
  PToH (cardHdr.readWriteWorkingSize);
  PToH (cardHdr.halDispatch);
}


void PToH (StorageHeaderType& store)
{
  PToH (store.signature);
  PToH (store.version);
  PToH (store.flags);
  PToH (store.creationDate);
  PToH (store.backupDate);
  PToH (store.heapListOffset);
  PToH (store.initCodeOffset1);
  PToH (store.initCodeOffset2);
  PToH (store.databaseDirID);
  PToH (store.rsvSpace);
  PToH (store.dynHeapSpace);
  PToH (store.firstRAMBlockSize);

  PToH (store.nvParams.splashScreenPtr);
  PToH (store.nvParams.hardResetScreenPtr);
  PToH (store.nvParams.localeLanguage);
  PToH (store.nvParams.localeCountry);

  PToH (store.crc);
}

void PToH (RecordListType& list)
{

  PToH (list.nextRecordListID);
  PToH (list.numRecords);

}


void PToH (DatabaseHdrType& hdr)
{
  PToH (hdr.attributes);
  PToH (hdr.version);
  PToH (hdr.creationDate);
  PToH (hdr.modificationDate);
  PToH (hdr.lastBackupDate);
  PToH (hdr.modificationNumber);
  PToH (hdr.appInfoID);
  PToH (hdr.sortInfoID);
  PToH (hdr.type);
  PToH (hdr.creator);
  PToH (hdr.uniqueIDSeed);
  PToH (hdr.recordList);
}

/***************************************************************
 *	Function:	  DevMemLocalIDToPtr
 *
 *	Summary:	  Converts a local ID to a pointer within the current
 *		ROM image.
 *
 *
 *	Parameters:
 *	  localID	  IN	localID in host byte order
 *
 *	Returns:
 *	  pointer in host byte order
 *
 *	History:
 *	  24-Dec-98 RM Ceated by Ron Marianetti
 *
 ****************************************************************/
void*
DevMemLocalIDToPtr (UInt32 localID)
{
  return (void*)(localID - (UInt32)(ROMBlock[0]) + (UInt32)ROMImageP);
}


/***************************************************************
 *	Function:	  DevMemPtrToLocalID
 *
 *	Summary:	  Converts a pointer within the current
 *		ROM image to a localID
 *
 *
 *	Parameters:
 *	  p			  IN	pointer to convert
 *
 *	Returns:
 *	  localID in host byte order
 *
 *	History:
 *	  24-Dec-98 RM Ceated by Ron Marianetti
 *
 ****************************************************************/
UInt32
DevMemPtrToLocalID (void* p)
{
  return ((UInt32)p - (UInt32)ROMImageP + ROMBlock[0]);
}



/***************************************************************
 *	Function:	  PrvInitChunkHdr
 *
 *	Summary:	  Initializes a memory chunk header with given
 *					params.
 *
 *	Parameters:
 *	  hdrP		  IN	pointer to chunk header
 *	  actSize	  IN	actual size
 *	  free		  IN	if true, make a free chunk
 *	  sizeAdj	  IN	size adjustment  (N/A if free)
 *	  owner		  IN	owner (N/A if free)
 *
 *	Returns:
 *	  0 if no err
 *
 *	History:
 *	  24-Dec-98 RM Ceated by Ron Marianetti
 *
 ****************************************************************/
static void
PrvInitChunkHdr (UInt8*  hdrP, UInt32 actSize, Boolean free,
				  UInt8 sizeAdj, UInt16 ownerID)
{

  // First, put in the size
  *((UInt32*)hdrP) = HToPDw (actSize);

  // If free, set the free bit and zero out other fields.
  if (free)
	{
	  *hdrP = 0x80;
	  hdrP += sizeof (UInt32);

	  // In free chunks, the hOffset field is the offset/2 to the
	  //   next free chunk which in our case is the 0 UInt32 at the
	  //   end of the heap.
	  //
	  // NOTE: The arithmetic below is a little misleading - since we've
	  //  already added 4 to hdrP in the previous line, the offset
	  //  to the last UInt32 in the heap is now (heapEnd - hdrP) instead
	  //  of (heapEnd - hdrP - 4).
	  //
	  Mem3HeapHeaderType*	heapP = (PrvHeapP);
	  UInt8*	heapEnd;
	  heapEnd = (UInt8*)heapP + PToHDw(heapP->size);
	  *((UInt32*)hdrP) = PToHDw ((heapEnd - hdrP)/2);

	  // No ownerID
	  *(hdrP + 4) = 0;
	}

  // Otherwise, put in the size adjustment and owner, and lockCount
  else
	{
	  *hdrP = sizeAdj;
	  hdrP += sizeof (UInt32);
	  *hdrP++ = (UInt8)((15 << 4) | ownerID);

	  // next 3 bytes of hOffset are 0 for pointers
	  *hdrP++ = 0;
	  *hdrP++ = 0;
	  *hdrP++ = 0;

	}

}

/***************************************************************
 *	Function:	  DevMemHeapInit
 *
 *	Summary:	  Initializes a heap structure at the given
 *	  location and size.
 *
 *	Parameters:
 *	  heapSpaceP  IN	where to place heap
 *	  size		  IN	size of space
 *
 *	Returns:
 *	  0 if no err
 *
 *	History:
 *	  24-Dec-98 RM Ceated by Ron Marianetti
 *
 ****************************************************************/
UInt32
DevMemHeapInit (void* heapSpaceP, UInt32 size)
{
  Mem3HeapHeaderType*	heapP;
  UInt32	err = 0;
  UInt8*	  freeP;

  // Err if we already have a heap
  assert (!PrvHeapP);

  // Pointer to heap space
  heapP = (Mem3HeapHeaderType*)heapSpaceP;
  memset (heapP, 0, sizeof (*heapP));
  // Save the heap pointer in a global for DevMemPtrNew()
  PrvHeapP = heapP;

  // First chunk header goes here
  freeP = (UInt8*)heapP + sizeof (*heapP);

  // init the heap structure
  heapP->flags = HToPW ((UInt16)(memHeapFlagVers3 | memHeapFlagReadOnly));
  heapP->size = HToPDw (size);
  heapP->firstFreeChunkOffset = HToPDw (((UInt32)freeP - (UInt32)heapP)/2);


  // Create the first free chunk and put a UInt32 of 0 at the end
  //   to look like a chunk header with a size of 0 to signify the
  // end of the heap
  UInt32	  chunkSize;
  UInt8* heapEndP;
  heapEndP = (UInt8*)heapSpaceP + size;
  chunkSize = heapEndP - sizeof (UInt32) - freeP;

  PrvInitChunkHdr (freeP, chunkSize, true /*free*/, 0 /*sizeAdj*/,
					0 /*owner*/);


  // Put a UInt32 of 0 at the end of the heap
  *((UInt32*)(freeP + chunkSize)) = 0;



  return err;
}




/***************************************************************
 *	Function:	  DevMemPtrNew
 *
 *	Summary:	  Allocate a new pointer
 *
 *	Parameters:
 *	  size		  IN	desired size
 *	  owner		  IN	owner ID
 *	  atEnd		  IN	if true, allocate pointer at end of heap
 *						if false, allocate at start of heap
 *
 *	Returns:
 *	  0 if no err
 *
 *
 *  Notes:
 *	  Here is the structure of a chunk header, but we can't use this
 *  directly because bitfields are non-portable
 *
 *	typedef struct {
 *		UInt32	free		:1;		// set if free chunk
 *		UInt32	moved		:1;		// used by MemHeapScramble
 *		UInt32	unused2		:1;		// unused
 *		UInt32	unused3		:1;		// unused
 *		UInt32	sizeAdj		:4;		// size adjustment
 *		UInt32	size		:24;	// actual size of chunk
 *
 *		UInt32	lockCount	:4;		// lock count
 *		UInt32	owner		:4;		// owner ID
 *		Int32	hOffset		:24;	// signed handle offset/2
 *  								// used in free chunks to point to next free chunk
 *		} MemChunkHeaderType;
 *
 *
 *	History:
 *	  24-Dec-98 RM Ceated by Ron Marianetti
 *
 ****************************************************************/
void*
DevMemPtrNew (UInt32 size, UInt8 ownerID, Boolean atEnd)
{
  Mem3HeapHeaderType*	heapP = (PrvHeapP);
  UInt32		err = 0;
  UInt8*		freeP;
  UInt8*		newChunkP;
  void*			dataP = 0;
  UInt32		actSize;
  UInt8			sizeAdj = 0;
  UInt32		freeSpace;

  // Must have a heap pointer global
  assert (heapP);

  // next free chunk goes here
  freeP = (UInt8*)heapP +  2 * PToHDw (heapP->firstFreeChunkOffset);


  // Calculate the actual size required of the new chunk
  actSize = size + memChunkHdrSize;
  if (actSize & 0x01)
	{
	  actSize++;
	  sizeAdj = 1;
	}


  // Make sure there's room in the heap for this chunk and it's header
  freeSpace = DevMemPtrSize (freeP + memChunkHdrSize); // pass data pointer
  if (freeSpace < actSize)
	{
	  printf ("\nNo more space in ROM heap!");
	  err = 1;
	  goto Exit;
	}


  // -----------------------------------------------------------------
  // If allocating at the end, calculate the new chunk pointer by
  //  skipping to the end of the free chunk
  // -----------------------------------------------------------------
  if (atEnd)
	{
	  newChunkP = freeP + memChunkHdrSize + freeSpace - actSize;
	}
  else
	{
	  newChunkP = freeP;
	  freeP += actSize;

	  heapP->firstFreeChunkOffset =
		HToPDw ((2*PToHDw (heapP->firstFreeChunkOffset) + actSize)/2);
	}
  freeSpace -= actSize;

  // Update the new free block
  PrvInitChunkHdr (freeP, freeSpace + memChunkHdrSize, true /*free*/, 0, 0);
  PrvFreeP = freeP;


  // -----------------------------------------------------------------
  // Init the chunk header
  // -----------------------------------------------------------------
  PrvInitChunkHdr (newChunkP, actSize, false /*free*/, sizeAdj, ownerID);
  dataP = newChunkP + memChunkHdrSize;



Exit:
  if (err) dataP = 0;
  return dataP;
}





/***************************************************************
 *	Function:	  DevMemPtrSize
 *
 *	Summary:	  Returns the size of a memory chunk in our ROM image.
 *
 *	Parameters:
 *	  localID	  IN	localID in host byte order
 *
 *	Returns:
 *	  pointer in host byte order
 *
 *	History:
 *	  24-Dec-98 RM Ceated by Ron Marianetti
 *
 ****************************************************************/
UInt32
DevMemPtrSize (void* dataP)
{
  UInt8*		chunkP;
  UInt32			dataSize;

  chunkP = (UInt8*)dataP - memChunkHdrSize;

  // The following is a hack to get the size out of the chunk header
  //  which we're forced to do because the chunk header is defined using
  //  non-portable bitfields
  dataSize = *((UInt32*)chunkP);
  dataSize = PToHDw (dataSize) & 0x00FFFFFF;	// get rid of flags

  // Subtract the size of the chunk header and the size adjustment
  dataSize -= memChunkHdrSize;

  // Size adjustment is the lower nibble of the first byte
  dataSize -= (*chunkP & 0x0F);

  return dataSize;
}


/***************************************************************
 *	Function:	  AppPrintType
 *
 *	Summary:	  Prints a 4 character type/creator
 *
 *	Parameters:
 *	  type	 IN  4 byte type
 *
 *	Returns:
 *	  void
 *
 *	Called By:
 *	  main() when user passes in hex option on command line
 *
 *	Notes:
 *
 *
 *	History:
 *	  11-6-98 RM	Created by Ron Marianetti
 *
 ****************************************************************/
void  AppPrintType ( UInt32 type)
{
  printf("%c%c%c%c", (type & 0xFF000000) >> 24,
	(type & 0x00FF0000) >> 16, (type & 0x0000FF00) >> 8,
	(type & 0x000000FF));
}



/***************************************************************
 *	Function:	  AppReadFile
 *
 *	Summary:	  Allocates memory and reads a file into it.
 *
 *
 *	Parameters:
 *	  filenameP	  IN	Name of file to read
 *	  *dataPP	  OUT	Pointer to malloc'ed buffer with file in it
 *	  *sizeP	  OUT	size of buffer (and file)
 *
 *	Returns:
 *	  0 if no error
 *
 *	Called By:
 *	  main() when parsing the -tokBin option
 *	  PrvReadROMImage() when reading in ROM image file
 *
 *	History:
 *	  15-Dec-98 RM Ceated by Ron Marianetti
 *
 ****************************************************************/
UInt32
AppReadFile (char* filenameP, void** dataPP, UInt32* sizeP)
{
  FILE*	fsP = 0;
  char* errMsgP = 0;
  UInt32	err = 1;
  UInt32	fileSize = 0;
  void*	dataP = 0;


  //----------------------------------------------------------
  // Open the file
  //------------------------------------------------------------
  fsP = fopen (filenameP, "rb");
  if (!fsP) {
	errMsgP = "\nERROR: Could not find file: '%s'";
	goto Exit;
	}


  // Get the size
  errMsgP = "ERROR: %s: while reading file";
  err = fseek (fsP, 0, SEEK_END);
  if (err)  goto Exit;

  fileSize = ftell (fsP);
  err = fseek (fsP, 0, SEEK_SET);
  if (err) goto Exit;


  // Allocate a buffer for the file
  dataP = malloc (fileSize);
  if (!dataP) { err = 1; errMsgP = "ERROR: Out of memory"; goto Exit; }


  // Read it in
  if (fread (dataP, 1, fileSize, fsP) != fileSize)
	{ err = 1; goto Exit; }


  err = 0;

Exit:
  // Clean up
  if (fsP) fclose (fsP);


  if (err)
	{
	  if (errMsgP) printf(errMsgP, filenameP);
	  printf("\n");

	  if (dataP) free (dataP);
	  dataP = 0;
	  fileSize = 0;
	}

  // Return params
  *dataPP = dataP;
  *sizeP = fileSize;

  return err;
}



/***************************************************************
 *	Function:	  AppReadPrcResource
 *
 *	Summary:	  Allocates memory and reads resource out of
 *			a PRC file into it.
 *
 *
 *	Parameters:
 *	  filenameP	  IN	Name of file to read
 *	  resType	  IN	resource type
 *	  resID		  IN	resource ID
 *	  *dataPP	  OUT	Pointer to malloc'ed buffer with file in it
 *	  *sizeP	  OUT	size of buffer (and file)
 *
 *	Returns:
 *	  0 if no error
 *
 *	Called By:
 *	  main() when parsing the -tokPrc option
 *
 *	History:
 *	  15-Dec-98 RM Ceated by Ron Marianetti
 *
 ****************************************************************/
UInt32
AppReadPrcResource (char* filenameP, UInt32 resType, UInt16 resID,
					void** dataPP, UInt32* sizeP)
{
  UInt32			  err = 1;
  UInt8*		  fileBufP = 0;
  UInt32			  fileSize;
  DatabaseHdrType hdr;
  UInt8*		  p;
  RsrcEntryType*  resEntryP = 0;
  void*			  resDataP = 0;
  UInt32			  resSize = 0;


  // First read the PRC file in
  err = AppReadFile (filenameP, (void**)&fileBufP, &fileSize);
  if (err) goto Exit;


  // Get the header
  p = fileBufP;
  hdr = *(DatabaseHdrType*)fileBufP;
  p += sizeof (DatabaseHdrType) - sizeof (UInt16) /*firstEntry*/;


  // Convert header to host order
  PToH (hdr);

  // Error if not a resource database
  if ((hdr.attributes & dmHdrAttrResDB) == 0)
	{
	  printf ("\nERROR: %s not a resource database", filenameP);
	  goto Exit;
	}


  // ------------------------------------------------------------
  // Loop through till we find the resource we want
  // ------------------------------------------------------------
  UInt32	  numRecords;
  numRecords = hdr.recordList.numRecords;
  resEntryP = (RsrcEntryType*)p;

  while (numRecords)
	{
	  PToH (resEntryP->type);
	  PToH (resEntryP->id);
	  PToH (resEntryP->localChunkID);

	  if (resEntryP->type == resType && resEntryP->id == resID) break;

	  // On to next one
	  resEntryP++;
	  numRecords--;
	  if (numRecords == 0)
		{
		  printf ("\nERROR: Resource ");
		  AppPrintType (resType);
		  printf (", id %d not found", resID);
		  err = 1;
		  goto Exit;
		}
	}


  // ------------------------------------------------------------
  // Malloc a block just for this resource
  // ------------------------------------------------------------
  if (numRecords > 1)
	resSize = PToHDw (resEntryP[1].localChunkID)
			  - resEntryP[0].localChunkID;
  else
	resSize = fileSize - resEntryP[0].localChunkID;

  resDataP = malloc (resSize);
  if (!resDataP)
	{
	  printf ("\nERROR: Out of memory");
	  err = 1;
	  goto Exit;
	}

  // Copy the data in
  memmove (resDataP, fileBufP + resEntryP[0].localChunkID, resSize);



Exit:
  // Clean up
  if (fileBufP) free (fileBufP);

  if (err)
	{
	  printf ("\nERROR: Could not read in resource ");
	  AppPrintType (resType);
	  printf (" from file '%s'", filenameP);

	  if (resDataP) free (resDataP);
	  resDataP = 0;
	  resSize = 0;
	}

  // Return results
  *dataPP = resDataP;
  *sizeP = resSize;
  return err;


}
