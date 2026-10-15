/***************************************************************

  Project:
	MakeROM	-	Utility for creating a Palm Memory Card image

  Copyright info:

	This is free software; you can redistribute it and/or modify
	it as you like.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.


  FileName:
		PalmMemEquates.h

  Description:
		This header file contains the equates for the Palm Memory
		& Data managers necessary for creating a memory card image.

  History:
		11-6-98  RM - Created by Ron Marianetti
****************************************************************/

#ifndef	__PALMMEMEQUATES_H__
#define	__PALMMEMEQUATES_H__



/********************************************************************
 * Structure of a Master Pointer chunk
 * The first master pointer chunk is always immediately following the
 *  heap header. As more movable chunks are created, new master
 *  pointer chunks are allocated dynamically and linked together through
 *  the nextTblOffset field and prevTblOffset field.
 ********************************************************************/
typedef struct
  {
	UInt16          numEntries;		// number of master pointer entries
	UInt32          nextTblOffset;	// offset from start of heap to next
									// table
	// MemPtr       mstrP[numEntries];  // array of pointers to movable
	// chunks
  }
MemMstrPtrTableType;
typedef MemMstrPtrTableType* MemMstrPtrTablePtr;



/********************************************************************
 * Structure of a version 2 Memory Manager Chunk
 *
 ********************************************************************/
typedef struct
  {
	UInt32          free:1;			// set if free chunk
	UInt32          moved:1;		// used by MemHeapScramble
	UInt32          unused2:1;		// unused
	UInt32          unused3:1;		// unused
	UInt32          sizeAdj:4;		// size adjustment
	UInt32          size:24;		// actual size of chunk

	UInt32          lockCount:4;	// lock count
	UInt32          owner:4;		// owner ID
	Int32           hOffset:24;		// signed handle offset/2
  }
MemChunkHeaderType;
typedef MemChunkHeaderType* MemChunkHeaderPtr;

#define	memChunkHdrSize			8


/********************************************************************
 * Structure of a version 3 Heap which is not in the public includes yet...
 *
 * A heap starts with a HeapHeader
 * Followed by the offset table (numHandles)
 * Followed by movable chunks
 * Followed by non-movable chunks
 ********************************************************************/
typedef struct
  {
	UInt16          flags;					// heap flags;
	UInt32          size;					// size of heap
	UInt32          firstFreeChunkOffset;	// offset/2 to first free chunk
	MemMstrPtrTableType mstrPtrTbl;			// Master pointer table
  }
Mem3HeapHeaderType;

// Flags Field
#define memHeapFlagReadOnly		0x0001		// heap is read-only (ROM based)
#define memHeapFlagVers2		0x8000		// version 2 heap (> 64K)
#define memHeapFlagVers3		0x4000		// version 3 heap (has free list)




/************************************************************
 * Structure of a Card Header.
 * There is 1 card header for in every card that has ROM. The
 *  card header is stored at sysCardHeaderOffset into the card.
 *
 * RAM only cards will not have a card header
 *************************************************************/
#define	memMaxNameLen				32		// size of name and manuf fields
											// including null
typedef struct CardHeaderType
  {
	UInt32          initStack;				// initial stack pointer
	UInt32          resetVector;			// reset vector
	UInt32          signature;				// must be sysCardSignature
	UInt16          hdrVersion;				// header version
	UInt16          flags;					// card flags;
	UInt8           name[memMaxNameLen];	// card name
	UInt8           manuf[memMaxNameLen];	// card manufacturer's name
	UInt16          version;				// card version
	UInt32          creationDate;			// card creation date
	UInt16          numRAMBlocks;			// number of RAM blocks on card
	UInt32          blockListOffset;		// offset to RAM block list
	UInt32          readWriteParmsOffset;	// v2: offset to read/write
											// system data if any (in ROM)
	UInt32          readWriteParmsSize;		// v2: size of read/write system
											// data if any (in ROM)
	UInt32          readOnlyParmsOffset;	// v2: offset to read-only system
											// data (in ROM)
	UInt32          bigROMOffset;			// v2: in SmallROM header: where SmallROM
											//  expects bigROM to live
											//  in BigROM header: where BigROM expects
											//  itself to live
	UInt32          checksumBytes;			// v2: size of card image in bytes (for
											// checksum)
	UInt16          checksumValue;			// v2: checksum of card image (from
											// Crc16CalcBlock)
	UInt32          readWriteWorkingOffset;	// v3: offset to read/write
											// working area if any (in
											// ROM)
	UInt32          readWriteWorkingSize;	// v3: size of read/write working
											// area if any (in ROM)

	UInt32          halDispatch;			// v4: pointer to HAL dispatch code

	UInt8           reserved[130];			// to bring us to 0x100 alignment
  }
CardHeaderType;
typedef CardHeaderType* CardHeaderPtr;

#define	memCardHeaderFlagRAMOnly	0x0001	// RAM only card
#define	memCardHeaderFlag328		0x0010	// ROM Supports 68328 processor
#define	memCardHeaderFlagEZ			0x0020	// ROM SUpports 68EZ328 processor
#define	memCardHeaderFlag230K		0x1000	// SmallROM supports 230Kbps



/************************************************************
 * This structure defines a section within the RAM storage header
 *  on Card#0 used to hold non-volatile System information. We store
 *  System information that can not be conveniently stored or accessed
 *  from a Database in this area because:
 *	1.) it can be accessed earlier during the boot-up process and
 *  2.) It can be accessed from an interrupt routine.
 *************************************************************/
typedef struct SysNVParamsType
  {
	UInt32          rtcHours;			// Real-Time clock hours - add to value
										// in DragonBall RTC register to get
										// actual date & time.
	UInt32          rtcHourMinSecCopy;	// Copy of latest value in
										// rtcHourMinSec reg of
										// DBall. Used as default RTC value on Reset.

  // Following Fields in 3.1 only
	UInt8           swrLCDContrastValue;	// Contrast Value for LCD on
											// EZ-based products
											// that use the software contrast PWM (such as Sumo)

  // Following Fields in 3.5 only
	UInt8           swrLCDBrightnessValue;	// Brightness value for screens
											// with adjustable brightness.

	// Note that in the ROM store, these next four fields contain the default
	// settings for card 0's RAM store, when it has to be initialized.
	UInt32          splashScreenPtr;		// MemPtr to splash screen bitmap
	UInt32          hardResetScreenPtr;		// MemPtr to hard reset screen
											// bitmap.
	UInt16          localeLanguage;			// Language for locale.
	UInt16          localeCountry;			// Country for locale.
  }
SysNVParamsType;
typedef SysNVParamsType* SysNVParamsPtr;


/************************************************************
 * Structure of a Storage Header.
 * There is 1 of these for every "store" on a memory card. A
 *  "store" can be all the RAM on a card or all the ROM on a card.
 *
 * The RAM storage header is stored at sysRAMHeader offset into the
 *	 card. and the ROM storage header is stored at sysROMHeader offset
 *  into the card.
 *************************************************************/
typedef struct
  {
	UInt32          signature;				// must be sysStoreSignature
	UInt16          version;				// version of header
	UInt16          flags;					// flags
	UInt8           name[memMaxNameLen];	// name of store
	UInt32          creationDate;			// creation date
	UInt32          backupDate;				// last backup date
	UInt32          heapListOffset;			// offset to heap list for store
	UInt32          initCodeOffset1;		// init code for store, if any
	UInt32          initCodeOffset2;		// second init code for store, if any
	LocalID         databaseDirID;			// local ID of database dir.
	UInt32          rsvSpace;				// where first heap starts.
	UInt32          dynHeapSpace;			// how big the dynamic heap area is
											// (always 0 for ROM stores)
	UInt32          firstRAMBlockSize;		// Copy of firstRAMBlock size from
											// cardinfo
	// Used to determine if we're rebooting
	// with a different amount of RAM.

	// The following field not used on ROM cards
	SysNVParamsType  nvParams;

	// Filler bytes - reserved for future use. Size adjusted to
	// keep total size of storage header at 0x100 bytes.
	UInt8           reserved[176 - sizeof (SysNVParamsType)];

	// CRC value
	UInt32          crc;					// crc to check validity
  }
StorageHeaderType;
typedef StorageHeaderType* StorageHeaderPtr;

#define	memStoreHeaderFlagRAMOnly	0x0001	// RAM store



// A Heap list for each store on a card (either RAM or ROM) gives a list of
// heaps' starting offsets for that store
typedef struct
  {
	UInt16          numHeaps;				// Number of heaps in store
	UInt32          heapOffset[1];			// offset to heap
  }
HeapListType;



// =========================================================================
// Data Manager Equates
// =========================================================================

/************************************************************
 * Structure of a Record entry
 *************************************************************/
typedef struct
  {
	LocalID         localChunkID;			// local chunkID of a record
	UInt8           attributes;				// record attributes;
	UInt8           uniqueID[3];			// unique ID of record; should
	// not be 0 for a legal record.
  }
RecordEntryType;
typedef RecordEntryType* RecordEntryPtr;



/************************************************************
 * Structure of a Resource entry
 *************************************************************/
typedef struct
  {
	UInt32          type;					// resource type
	UInt16          id;						// resource id
	LocalID         localChunkID;			// resource data chunk ID
  }
RsrcEntryType;
typedef RsrcEntryType* RsrcEntryPtr;

// Attributes field
#define	dmRsrcAttrUnused		0x0000		// to be defined...





/************************************************************
 * Structure of a record list extension. This is used if all
 *  the database record/resource entries of a database can't fit into
 *  the database header.
 *************************************************************/
typedef struct
  {
	LocalID         nextRecordListID;		// local chunkID of next list
	UInt16          numRecords;				// number of records in this list
	UInt16          firstEntry;				// array of Record/Rsrc entries
	// starts here
  }
RecordListType;
typedef RecordListType* RecordListPtr;



/************************************************************
 * Structure of a Database Header
 *************************************************************/
typedef struct
  {
	UInt8           name[dmDBNameLength];	// name of database
	UInt16          attributes;				// database attributes
	UInt16          version;				// version of database

	UInt32          creationDate;			// creation date of database
	UInt32          modificationDate;		// latest modification date
	UInt32          lastBackupDate;	  		// latest backup date
	UInt32          modificationNumber;		// modification number of
											// database

	LocalID         appInfoID;				// application specific info
	LocalID         sortInfoID;				// app specific sorting info

	UInt32          type;					// database type
	UInt32          creator;				// database creator

	UInt32          uniqueIDSeed;			// used to generate unique IDs.
											// Note that only the low order
											// 3 bytes of this is used (in
											// RecordEntryType.uniqueID).
											// We are keeping 4 bytes for
											// alignment purposes.

	RecordListType  recordList;				// first record list
  }
DatabaseHdrType;



/************************************************************
 * Directory of all available databases - maintained by the
 *  Database Manager. This directory also includes resource
 *  databases. There is one of these directories for each store
 *  (either RAM or ROM store) in a memory card.
 *************************************************************/
typedef struct
  {
	LocalID         nextDatabaseListID;		// continuation list, if any
	UInt16          numDatabases;			// # of database ID's in this list
	LocalID         databaseID[1];			// ChunkID of each Database
  }
DatabaseDirType;

typedef DatabaseDirType* DatabaseDirPtr;


#define	dmDynOwnerID			0x00		// Dynamic heap chunks
#define	dmMgrOwnerID			0x01		// Management chunks
#define	dmRecOwnerID			0x02		// Record chunks
#define	dmOrphanOwnerID			0x03		// Orphaned record chunks



// =========================================================================
// System Manager Equates
// =========================================================================

#define	sysCardSignature		0xFEEDBEEFL		// card signature
												// long word
#define	sysStoreSignature		0xFEEDFACEL		// store signature
												// long word

#endif	/* __PALMMEMEQUATES_H__ */
