/***************************************************************
 *
 *  Project:
 *	MakeROM - Utility to Create a PalmOS ROM Image file.
 *
 *  Copyright info:
 *
 *	This is free software; you can redistribute it and/or modify
 *	it as you like.
 *
 *	This program is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 *
 *
 *  FileName:
 *	  Main.cpp
 *
 *  Description:
 *	  This is the main source file for the MakeROM utility
 *
 *  ToDo:
 *	 - option to set 328 and EZ flags in card header flags field
 *	 - option to print out current ROM tokens
 *
 *  Example command line arguments:
 *	-op create -romBootDB SmallROM.tdres.prc -base 0 -hdr 0x10C00000 \
 *		-romBlock 0x10C00000 0x00008000  -chRomTokens 0x10C06000 0x2000 \
 *		-chBigRomOffset  0x10C08000
 *
 *	-op create -romBootDB SmallROM.tdres.prc -romDB Preferences.prc \
 *		-base 0 -hdr 0x10C00000 -romBlock 0x10C00000 0x00008000  \
 *		-chBigRomOffset  0x10C08000 -chRomTokens 0 0
 *
 *  -op create -romDB phoneApp.prc -romBlock 0x00C00000 0x6000
 *
 *  -op create -romDB InOmniRemote.pdb -romDB InphoneApp.prc -romBlock 0x08000000 0x8000 -hdr 0x08000000
 *
 *  History:
 *		15-Dec-98  RM - Created by Ron Marianetti (rmarianetti@handspring.com)
 *****************************************************************/
// --------------------------------------------------------------
// Local includes
// --------------------------------------------------------------
#include "MakeROM.h"


// Private Defines
#define	 prvMaxDBs			200					// Max # of databases in ROM
#define	 prvDefHdrOffset	0x00C00000			// Default card header offset
#define	 prvMaxROMTokens	32					// Max # of tokens in ROM


// The operation to perform
typedef enum
  {
	prvOpNone,
	prvOpInfo,
	prvOpPatch,
	prvOpJoin,
	prvOpSplit,
	prvOpCreate,
	prvOpBreak
  } PrvOpEnum;

// --------------------------------------------------------------
// Globals
// --------------------------------------------------------------
// Command line options collected here
CardHeaderType	  CardHdr =
  {
	0x03000,						// initStack
	0x0,							// resetVector
	sysCardSignature,				// sysCardSignature
	4,								// hdrVersion
	0x0020,							// flags DOLATER... option to set these
	"PalmCard",						// name
	"Handspring, Inc.",				// manuf
	1,								// version
	0,								// creation date
	0,								// numRAMBlocks
	0,								// blockListOffset
	0x00000000,						// readWriteParmsOffset
	0x00000000,						// readWriteParmsSize
	0,								// readOnlyParmsOffset
	prvDefHdrOffset + 0x08000,		// bigROMOffset
	0x1000,							// checksum bytes
	0,								// checksum value
	// Rest initialized to 0 by compiler
  };


StorageHeaderType ROMStoreHdr =
  {
	sysStoreSignature,				// signature
	1,								// version
	0,								// flags
	"ROM Store",					// name
	0,								// creation date
	0,								// backup date
	0,								// heapListOffset
	0,								// initodeOffset1,
	0,								// initCodeOffset2,
	0,								// databaseDirID
	0,								// rsvSpace
	0,								// dynHeapSpace
	0,								// firstRAMBlockSize,

	// SysNVParams
	{
	  0,							// rtcHours
	  0,							// rtcHourMinSecCopy
	  0,							// swrLCDContrastValue
	  0,							// swrLCDBrightnessValue
	  0,							// splashScreenPtr
	  0,							// hardResetScreenPtr
	  0,							// localeLanguage
	  0x17							// localeCountry
	},

	// Reserved
	{0},							// reserved
	0								// crc
  };

char*			  OutNameP = "out.bin";

// NOTE: All of the data within a card ROM image should be card base relative so
//  that the card can be installed at any base address. The only exception to this is
//  the reset vector which must be absolute. The other exception (due to an oversight in
//  the design) is for pointer type ROM tokens which unfortunately encode the ROM token
//  pointer using an absolute address rather than a card relative address.
// Hence, the only place we use CardBase is when calculating the reset vector and when
//  putting in pointer type ROM tokens.
UInt32			  CardBase = 0x10000000;		// -base

// Offset to the card header from from card base address
UInt32			  HdrOffset = prvDefHdrOffset;	// -hdr
Boolean			  OptResetAt0 = false;			// -chReset0
Boolean			  OptZCrDate  = false;			// -chZCrDate
Boolean			  OptNoSpaces = false;			// -noSpaces
Boolean			  OptNoForceReadOnly = false;	// -noForceReadOnly
Boolean			  OptAutoSize = false;			// -autoSize
Boolean			  OptCopyPrevention = false;	// -copyPrevention

// ROM databases to put in ROM
char*			  ROMBootDbNameP = 0;			// -romBootDB
char*			  ROMHalDbNameP = 0;			// -romHalDB
char*			  ROMPalmHalDbNameP = 0;		// -romPalmHalDB
char*			  ROMDBNameP [prvMaxDBs];		// -romDB
UInt32			  NumROMDBs = 0;

// NOTE: we only ever cretae 1 block of ROM and 1 of RAM even though PalmOS
//  can support more than 1 of each.
UInt32			  ROMBlock[2] = {prvDefHdrOffset, 0x200000};	// -romBlock
UInt32			  RAMBlock[2] = {0, 0};				// -ramBlock

// Pointer to ROM image and it's size.
UInt32			  ROMImageSize = 0;
void*			  ROMImageP = 0;

// List of tokens for the ROM Image
UInt32			  NumROMTokens = 0;
AppROMTokenType	  ROMTokens [prvMaxROMTokens];

// Max size of the readOnlyParms (ROM tokens) area of the ROM, used for error
// checking during creation but not actually stored in the card header
UInt32			  ReadOnlyParmsSize = 0x2000;

// Prototypes
static UInt32  PrvDoBreak (Boolean justPrintOptions);



/***************************************************************
 *	Function:	  PrvPrintROMInfo
 *
 *	Summary:	  Prints out info on current ROM image
 *
 *	Parameters:
 *
 *	Returns:
 *	  0 if no error
 *
 *	Called By:
 *	  main() after parsing the -op info option
 *
 *	History:
 *	  24-Dec-98 RM Ceated by Ron Marianetti
 *
 ****************************************************************/
static UInt32
PrvPrintROMInfo (void)
{
  unsigned int	i;
  UInt32	  ramBlockOffset=0, ramBlockSize=0;

  //=================================================================
  // Print general info
  //=================================================================
  printf ("\nGeneral Info: ");
  printf ("\n  %-20s: 0x%08lX", "cardBase", CardBase);
  printf ("\n  %-20s: 0x%08lX", "cardHdrOffset", HdrOffset);
  printf ("\n  %-20s: 0x%08lX", "romBlockOffset", ROMBlock[0]);
  printf ("\n  %-20s: 0x%08lX", "romBlockSize", ROMBlock[1]);


  //=================================================================
  // Print info on the card header
  //=================================================================
  printf ("\n\nCardHeader Info: ");
  printf ("\n  %-20s: 0x%08lX", "initStack", CardHdr.initStack);
  printf ("\n  %-20s: 0x%08lX", "resetVector", CardHdr.resetVector);
  printf ("\n  %-20s: %d", "hdrVersion", (int)CardHdr.hdrVersion);
  printf ("\n  %-20s: 0x%04X", "flags", (int)CardHdr.flags);
  printf ("\n  %-20s: %s", "name", CardHdr.name);
  printf ("\n  %-20s: %s", "manuf", CardHdr.manuf);
  printf ("\n  %-20s: 0x%04lX", "version", CardHdr.version);
  printf ("\n  %-20s: 0x%08lX", "creationDate", CardHdr.creationDate);
  printf ("\n  %-20s: %d", "numRAMBlocks", CardHdr.numRAMBlocks);
  printf ("\n  %-20s: 0x%08lx", "blockListOffset", CardHdr.blockListOffset);

  UInt32*	blockP;
  blockP = (UInt32*) ((UInt32)ROMImageP + (CardHdr.blockListOffset - ROMBlock[0]));
  for (i=0; i<CardHdr.numRAMBlocks; i++)
	{

	  ramBlockOffset = PToHDw (*blockP); blockP++;
	  ramBlockSize = PToHDw (*blockP); blockP++;

	  printf ("\n    %-20s: 0x%08lX", "offset", ramBlockOffset);
	  printf ("\n    %-20s: 0x%08lX", "size", ramBlockSize);
	}

  if (CardHdr.hdrVersion >= 2)
	{
	  printf ("\n  %-20s: 0x%08lx", "readWriteParmsOffset", CardHdr.readWriteParmsOffset);
	  printf ("\n  %-20s: 0x%08lx", "readWriteParmsSize", CardHdr.readWriteParmsSize);
	  printf ("\n  %-20s: 0x%08lx", "readOnlyParmsOffset", CardHdr.readOnlyParmsOffset);
	  printf ("\n  %-20s: 0x%08lx", "bigROMOffset", CardHdr.bigROMOffset);
	  printf ("\n  %-20s: 0x%08lx", "checksumBytes", CardHdr.checksumBytes);
	  printf ("\n  %-20s: 0x%04lx", "checksumvalue", CardHdr.checksumValue);
	}


  //=================================================================
  // Print info on the ROM store
  //=================================================================
  printf ("\n\nROMStore Info: ");
  printf ("\n  %-20s: %d", "version", ROMStoreHdr.version);
  printf ("\n  %-20s: 0x%04lX", "flags", ROMStoreHdr.flags);
  printf ("\n  %-20s: %s", "name", ROMStoreHdr.name);
  printf ("\n  %-20s: 0x%08lX", "creationDate", ROMStoreHdr.creationDate);
  printf ("\n  %-20s: 0x%08lX", "backupDate", ROMStoreHdr.backupDate);
  printf ("\n  %-20s: 0x%08lX", "heapListOffset", ROMStoreHdr.heapListOffset);

  HeapListType*	heapListP;
  heapListP = (HeapListType*) ((UInt32)ROMImageP
                               + (ROMStoreHdr.heapListOffset - ROMBlock[0]));
  for (i=0; i < PToHW (heapListP->numHeaps); i++)
	{
	  printf ("\n    %s %d offset     : 0x%08lX", "heap", i,
	                                      PToHDw (heapListP->heapOffset[i]));
	}

  printf ("\n  %-20s: 0x%08lX", "initCodeOffset1", ROMStoreHdr.initCodeOffset1);
  printf ("\n  %-20s: 0x%08lX", "initCodeOffset2", ROMStoreHdr.initCodeOffset2);
  printf ("\n  %-20s: 0x%08lX", "databaseDirID", ROMStoreHdr.databaseDirID);


  //=================================================================
  // Print info on the ROM tokens
  //=================================================================
  if (NumROMTokens > 0)
	{
	  printf ("\n\nROM Token info: ");
	  for (i=0; i < NumROMTokens; i++)
		{
		  printf ("\n    %s %d%-11s: ", "token",  i, "");
		  AppPrintType(ROMTokens[i].token);
		  printf (", %d %s", ROMTokens[i].len, "bytes");
		}
	}

  //=================================================================
  // Print command line options for building this ROM
  //=================================================================
  printf ("\n\nCommand line options to build:");
  printf ("\n  -base 0x%08lX", CardBase);
  printf ("\n  -hdr 0x%08lX", HdrOffset);
  printf ("\n  -chName \"%s\"", CardHdr.name);
  printf ("\n  -chManuf \"%s\"", CardHdr.manuf);
  printf ("\n  -chVersion 0x%04X", (int)CardHdr.version);
  printf ("\n  -chStack 0x%08lX", CardHdr.initStack);
  printf ("\n  -chRomTokens 0x%08lX 0x%08lX", CardHdr.readOnlyParmsOffset,
				  ReadOnlyParmsSize);
  printf ("\n  -romName \"%s\"", ROMStoreHdr.name);
  printf ("\n  -romBlock 0x%08lX 0x%08lX", ROMBlock[0], ROMBlock[1]);
  printf ("\n  -ramBlock 0x%08lX 0x%08lX", ramBlockOffset, ramBlockSize);

  PrvDoBreak (true /*justPrintOptions*/);


  printf ("\n");
  return 0;
}




/***************************************************************
 *	Function:	  PrvParseROMImage
 *
 *	Summary:	  Reads in an existing ROM image file and initializes
 *	  our global card header, ROM Store header, and other card settings
 *	  from that image.
 *
 *	  NOTE: This routine only works for parsing ROM images where the
 *	  CardHeader is first (the normal case) even though technically a ROM
 *	  image could be built with the card header later.
 *
 *	Parameters:
 *	  filenameP	  IN	Name of image file
 *
 *	Returns:
 *	  0 if no error
 *
 *	Called By:
 *	  main() when parsing the -patch option
 *
 *	History:
 *	  15-Dec-98 RM Ceated by Ron Marianetti
 *
 ****************************************************************/
static UInt32
PrvParseROMImage (char* filenameP)
{
  char* errMsgP = "";
  UInt32	err = 1;
  UInt16	hdrVersion;


  //----------------------------------------------------------
  // Read in the rom image file
  //------------------------------------------------------------
  err = AppReadFile (filenameP, &ROMImageP, &ROMImageSize);
  if (err) goto Exit;


  //----------------------------------------------------------
  // Validate the image by checking the signature and version
  //----------------------------------------------------------
  CardHeaderType*	cardP;
  err = 1;
  cardP = (CardHeaderType*)ROMImageP;
  if (PToHDw (cardP->signature) != sysCardSignature)
	{
	  printf ("\nERROR: %s: Invalid Card header", filenameP);
	  goto Exit;
	}
  hdrVersion = PToHW (cardP->hdrVersion);
  if (hdrVersion < 1 || hdrVersion > 4)
	{
	  printf ("\nERROR: %s: Unsupported ROM image version (%ld)", filenameP,
			  (long)hdrVersion);
	  goto Exit;
	}

  StorageHeaderType*  storeP;
  storeP = (StorageHeaderType*)(cardP + 1);
  if (PToHDw (storeP->signature) != sysStoreSignature)
	{
	  printf ("\nERROR: %s: Invalid ROM Store header", filenameP);
	  goto Exit;
	}


  //----------------------------------------------------------
  // Copy out the card header fields and ROM Store into ours
  //----------------------------------------------------------
  CardHdr = *cardP;
  PToH (CardHdr);

  ROMStoreHdr = *storeP;
  PToH (ROMStoreHdr);

  // Guess the card base and card header offset from the block list offset
  //  and reset vector. These are merely intelligient guesses and can be overridden
  //  through command line options (-base and -hdr, etc.). T
  // These  guesses assume the reset vector and blockList offset are both within
  //  4K of the card header (always true the way we normally build cards).
  //
  // The reset vector gives us the absolute address of the reset code and by
  //  subtracting the card header offset, we can get the card base address. The
  //  blockListOffset effectively gives us the card header offset since it's so close
  //  to the card header.
  HdrOffset = (CardHdr.blockListOffset & 0xFFFFF000);
  if (CardHdr.resetVector)
	CardBase = (CardHdr.resetVector & 0xFFFFF000) - HdrOffset;
  else
	{
	  // HACK!! assume that if upper nibble is non-zero in the HdrOffset that the cardBase
	  //  is at 0 (this is true in the EZ Roms). If not, then card base is 0x10000000 (this
	  //  is true for all other ROMs).
	  if (HdrOffset & 0xF0000000)
		CardBase = 0;
	  else
		CardBase = 0x10000000;
	}


  // We only support images where the card header as the first thing in the ROM so
  //  the ROMBlock offset (i.e. offset from cardbase to the start of ROM for the card)
  //  is the same as the card header offset.
  ROMBlock[0] = HdrOffset;
  ROMBlock[1] = ROMImageSize;


  // Get the RAM Block info
  if (CardHdr.numRAMBlocks > 1)
	{
	  printf ("\nERROR: %s: Don't support cards with more than 1 RAM Block",
		  filenameP);
	  goto Exit;
	}
  UInt32*  dwP;
  dwP = (UInt32*)DevMemLocalIDToPtr (CardHdr.blockListOffset);
  RAMBlock[0] = PToHDw (*dwP);
  dwP++;
  RAMBlock[1] = PToHDw (*dwP);


  // Guess the size of the read-only area. We only support parsing the the ROM tokens
  //  when given a small ROM image where the read-only area follows the card header
  //  and ends at the end of the ROM
  if (CardHdr.readOnlyParmsOffset && CardHdr.readOnlyParmsOffset > HdrOffset)
	ReadOnlyParmsSize = (UInt32)DevMemPtrToLocalID ((UInt8*)ROMImageP + ROMImageSize)
					  - CardHdr.readOnlyParmsOffset;
  else
	ReadOnlyParmsSize = 0;

  //----------------------------------------------------------
  // Get the ROM Tokens out of the image
  //----------------------------------------------------------
  if (hdrVersion >= 2 && ReadOnlyParmsSize)
	{
	  UInt8*			tokenP;
	  AppROMTokenType*	dstTokenP;
	  tokenP = (UInt8*)DevMemLocalIDToPtr (CardHdr.readOnlyParmsOffset);

	  do
		{
		  UInt32	token;

		  // Get the token type
		  token = *((UInt32*)tokenP);
		  tokenP += sizeof (UInt32);
		  PToH (token);
		  if (token == 0xFFFFFFFF) break;

		  // Find a new slot to save it in
		  if (NumROMTokens >= prvMaxROMTokens-1)
			{
			  printf ("\nERROR: %s: Too many ROM Tokens", filenameP);
			  goto Exit;
			}
		  dstTokenP = &(ROMTokens[NumROMTokens++]);

		  // Save it in our private structure
		  dstTokenP->token = token;
		  dstTokenP->len = *((UInt16*)tokenP);
		  tokenP += sizeof (UInt16);
		  PToH (dstTokenP->len);

		  // Allocate space for the data
		  if (dstTokenP->len)
			{
			  dstTokenP->dataP = (UInt8*)malloc (dstTokenP->len);
			  if (!dstTokenP->dataP)
				{
				  printf ("\nERROR: Out of memory");
				  goto Exit;
				}
			  memmove (dstTokenP->dataP, tokenP, dstTokenP->len);
			  tokenP += dstTokenP->len;
			}
		  else
			dstTokenP->dataP = 0;

		  // Tokens are padded to word boundary
		  if (((UInt32)tokenP) & 0x01) tokenP++;

		 } while (tokenP);

	  } // if (CardHdr.readOnlyParmsOffset)



  err = 0;

Exit:

  if (err)
	{
	  if (errMsgP) printf(errMsgP, err);
	  else printf ("\nERROR %d: (unknown)", err);
	  printf("\n");
	}

  return err;
}




/***************************************************************
 *	Function:	  PrvWriteOutROM
 *
 *	Summary:	  Writes out current ROM image and any current
 *		ROM tokens to output file.
 *
 *	Parameters:
 *	  fsP		  IN	output file
 *
 *	Returns:
 *	  0 if no error
 *
 *	Called By:
 *	  main() after parsing the -patch option
 *
 *	History:
 *	  15-Dec-98 RM Ceated by Ron Marianetti
 *
 ****************************************************************/
static UInt32
PrvWriteOutROM (FILE* fsP)
{
  UInt32				  err;
  UInt32				  idx;
  CardHeaderType*	  cardP;
  UInt8*			  dstTokenP;
  AppROMTokenType*	  srcTokenP;
  UInt32				  tokenSpaceDevP;
  UInt8*			  tokenSpaceHostP;
  UInt8*			  readOnlyP = 0;


  // Card header at top of ROM
  cardP = (CardHeaderType*)ROMImageP;


  // ----------------------------------------------------
  // Update the creation date of the card header
  // ----------------------------------------------------

  // Get current time (since 1/1/1970) and convert to PalmOS time (1/1/1904)
  UInt32           theTime;
  UInt32           dTime;

  theTime = time (0);
  dTime = (UInt32) (66L * (365.25252 * 24 * 60 * 60));
  theTime += dTime;

  if (OptZCrDate)
	CardHdr.creationDate  = 0;
  else
	CardHdr.creationDate = theTime;


  // <chg 31-jan-00 dia> If we set checksumBytes to -1 above, make it
  // checksum the whole ROM...
  if (CardHdr.checksumBytes == (UInt32) -1)
    {
      CardHdr.checksumBytes = ROMBlock[1];
    }


  // ----------------------------------------------------
  // Update the card header in existing ROM image
  // ----------------------------------------------------
  *cardP = CardHdr;
  PToH (*cardP);


  // ----------------------------------------------------
  // Update the storage header in existing ROM image
  // ----------------------------------------------------
  StorageHeaderType* storeP;
  storeP = (StorageHeaderType*)(cardP+1);
  *storeP = ROMStoreHdr;
  PToH (*storeP);




  // Err if there are ROM tokens but no space to output them
  if (NumROMTokens && !ReadOnlyParmsSize)
	{
	  printf ("\nERROR: Can not add ROM Tokens to this ROM image. Most likely "
	          "\n this is a big ROM where the read-ony area is before the card"
			  "\n header. Add the ROM tokens to the small ROM instead.");
	  err = 1;
	  goto Exit;
	}


  // ----------------------------------------------------
  // Update the read-only area with the inline tokens and
  //  pointers to where the pointer tokens will be.
  // NOTE: ROM Tokens are normally only specified when building
  //  the small ROM (when CardHdr.readOnlyParmsOffset > ROMBlock[0])
  // ----------------------------------------------------
  if (CardHdr.readOnlyParmsOffset && ReadOnlyParmsSize)
	{

	  // Source token
	  srcTokenP = &ROMTokens[0];

	  // Where first token goes
	  readOnlyP = (UInt8*)DevMemLocalIDToPtr (CardHdr.readOnlyParmsOffset);
	  dstTokenP = readOnlyP;

	  // Error check and make sure it's specified within our ROM image buffer bounds
	  if (readOnlyP < (UInt8*)ROMImageP
		  || (readOnlyP + ReadOnlyParmsSize) > ((UInt8*)ROMImageP + ROMImageSize))
		{
		  printf ("\nERROR: The specified ROM token area (offset: 0x%08lX, "
				  "size: 0x%08lX is not within the specified ROM area "
				  " (offset: 0x%08lX, size: 0x%08lX",
				  CardHdr.readOnlyParmsOffset, ReadOnlyParmsSize,
				  ROMBlock[0], ROMBlock[1]);
		  err = 1;
		  goto Exit;
		}


	  // Space at the end of the read-only area for pointer token data
	  tokenSpaceDevP = CardBase + CardHdr.readOnlyParmsOffset + ReadOnlyParmsSize;

	  // host pointer to the token space
	  tokenSpaceHostP = (UInt8*)DevMemLocalIDToPtr (CardHdr.readOnlyParmsOffset
					  + ReadOnlyParmsSize);

	  // Prefill the read-only area with FF's
	  memset (dstTokenP, 0xFF, ReadOnlyParmsSize);

	  for (idx=0; idx<NumROMTokens; idx++, srcTokenP++)
		{
		  *((UInt32*)dstTokenP) = HToPDw (srcTokenP->token);
		  dstTokenP += sizeof (UInt32);

		  // Pointer tokens - data goes at end of read-only area
		  if (srcTokenP->ptr)
			{
			  *((UInt16*)dstTokenP) = HToPW (4);	  // length
			  dstTokenP += sizeof (UInt16);

			  tokenSpaceDevP -= srcTokenP->len;
			  tokenSpaceHostP -= srcTokenP->len;
			  *((UInt32*)dstTokenP) = HToPDw (tokenSpaceDevP); // value
			  dstTokenP += sizeof (UInt32);

			  // error if token pointer space runs into token area
			  if (tokenSpaceHostP <= dstTokenP)
				{
				  printf ("\nERROR: Size of read only ROM Token area not large enough for"
						  " given ROM tokens");
				  err = 1;
				  goto Exit;
				}

			  // Write out the pointer data
			  memmove (tokenSpaceHostP, srcTokenP->dataP, srcTokenP->len);
			  free (srcTokenP->dataP);
			  srcTokenP->dataP = 0;
			}

		  // Other tokens go inline
		  else
			{
			  *((UInt16*)dstTokenP) = HToPW (srcTokenP->len);
			  dstTokenP += sizeof (UInt16);
			  memmove (dstTokenP, srcTokenP->dataP, srcTokenP->len);
			  dstTokenP += srcTokenP->len;
			  // Pad to word boundary
			  if (((UInt32)dstTokenP) & 0x01) dstTokenP++;

			  // Free the token data
			  free (srcTokenP->dataP);
			  srcTokenP->dataP = 0;
			}

		} // for (..;i<NumROMTOkens; ..)

	// end token
	*((UInt32*)dstTokenP) = 0xFFFFFFFF;

	} // if (CardHdr.readOnlyParmsOffset



  // ----------------------------------------------------
  // Update the checksum value. The checksum is required for the ROM
  //  to work in the PalmOS Emulator
  //
  // The checksum is the cumulative checksum of the ROM image before
  // the stored checksum value and the ROM image following the checksum
  // value.  First, calculate the first part.
  // ----------------------------------------------------
  if (CardHdr.checksumBytes)
	{
	  UInt32	chunkSize;
	  UInt16	checksumValue;

	  chunkSize = (UInt8*)&CardHdr.checksumValue - (UInt8*)&CardHdr;
	  checksumValue = Crc16CalcBigBlock (ROMImageP, chunkSize, 0);

	  // Now calculate the second part.
	  checksumValue = Crc16CalcBigBlock (
		  (UInt8*)ROMImageP + chunkSize + sizeof (CardHdr.checksumValue),
		  CardHdr.checksumBytes - chunkSize - sizeof (CardHdr.checksumValue),
		  checksumValue);

	  // STore checksum in ROM image
	  cardP->checksumValue = HToPW (checksumValue);
	}


  // ================================================================
  // Write out the current ROM image up to the read only area
  // ================================================================
  if (fwrite ((UInt8*)ROMImageP, ROMImageSize, 1, fsP) != 1)
	{
	  err = 1;
	  printf ("\nERROR: while writing to output file '%s'", OutNameP);
	  goto Exit;
	}
  err = 0;


Exit:
  return err;
}



/***************************************************************
 *	Function:	  PrvDoSplit
 *
 *	Summary:	  MemHandle the -op split operation which splits a
 *	  combined ROM into small and bigROMs.
 *
 *	Parameters:
 *	  inNameP	  IN	Name of image file to split
 *	  offset	  IN	where to split file.
 *	  outNameP	  IN	root name for small and big rom output files
 *
 *	Returns:
 *	  0 if no error
 *
 *	Called By:
 *	  main() when parsing the -patch option
 *
 *	History:
 *	  15-Dec-98 RM Ceated by Ron Marianetti
 *
 ****************************************************************/
static UInt32
PrvDoSplit (char* inNameP, UInt32 offset, char* outRootNameP)
{
  void*	  bufP = 0;
  UInt32	  bufSize;
  UInt32	  err = 0;
  FILE*	  fsP = 0;
  char*	  outNameP = 0;


  // Read in the source file
  err = AppReadFile (inNameP, &bufP, &bufSize);
  if (err) goto Exit;

  // Error check
  if (bufSize < offset)
	{
	  printf ("\nERROR: offset for split operation is greater than size of input file");
	  err = 1;
	  goto Exit;
	}


  //--------------------------------------------------------
  // Write out the small ROM portion
  //--------------------------------------------------------
  outNameP = (char*)malloc (strlen (outRootNameP) + 32);
  if (!outNameP) goto ExitNoMem;
  sprintf (outNameP, "%s1.bin", outRootNameP);

  fsP = fopen (outNameP, "wb");
  if (!fsP)
	{
	  printf ("\nERROR: Could not create output file '%s'", outNameP);
	  goto Exit;
	}

  if (fwrite ((UInt8*)bufP, offset, 1, fsP) != 1)
	{
	  err = 1;
	  printf ("\nERROR: while writing to output file '%s'", outNameP);
	  goto Exit;
	}
  fclose (fsP);
  fsP = 0;



  //--------------------------------------------------------
  // Write out the big ROM portion
  //--------------------------------------------------------
  sprintf (outNameP, "%s2.bin", outRootNameP);
  fsP = fopen (outNameP, "wb");
  if (!fsP)
	{
	  printf ("\nERROR: Could not create output file '%s'", outNameP);
	  goto Exit;
	}

  if (fwrite ((UInt8*)bufP+offset, bufSize-offset, 1, fsP) != 1)
	{
	  err = 1;
	  printf ("\nERROR: while writing to output file '%s'", outNameP);
	  goto Exit;
	}
  fclose (fsP);
  fsP = 0;
  err = 0;
  goto Exit;


ExitNoMem:
  printf ("\nERROR: out of memory");
  err = 1;


Exit:
  if (err)
	{
	  printf ("\nERROR: %ld while performing split operation", err);
	}

  if (bufP) free (bufP);
  if (fsP) fclose (fsP);
  if (outNameP) free (outNameP);
  return err;

}


/***************************************************************
 *	Function:	  PrvDoJoin
 *
 *	Summary:	  MemHandle the -op join operation which joins a small
 *	  and big ROM into 1 file.
 *
 *	Parameters:
 *	  smRomP		IN	Name of small ROM
 *	  bigRomP		IN	Name of big ROM
 *	  bigROMOffset	IN	offset within final image to put big ROM
 *	  outNameP		IN	Name of output file
 *
 *	Returns:
 *	  0 if no error
 *
 *	Called By:
 *	  main() when parsing the -patch option
 *
 *	History:
 *	  15-Dec-98 RM Ceated by Ron Marianetti
 *
 ****************************************************************/
static UInt32
PrvDoJoin (char* smRomP, char* bigRomP, UInt32 bigRomOffset, char* outNameP)
{

  void*	  bufP = 0;
  UInt32	  bufSize;
  UInt32	  err = 0;
  FILE*	  fsP = 0;


  // Read in the small ROM file
  err = AppReadFile (smRomP, &bufP, &bufSize);
  if (err) goto Exit;

  // Error check
  if (bufSize > bigRomOffset)
	{
	  printf ("\nERROR:Join: size of small ROM is bigger than offset for big ROM");
	  err = 1;
	  goto Exit;
	}


  //--------------------------------------------------------
  // Write out the small ROM portion
  //--------------------------------------------------------
  fsP = fopen (outNameP, "wb");
  if (!fsP)
	{
	  printf ("\nERROR: Could not create output file '%s'", outNameP);
	  goto Exit;
	}

  if (fwrite ((UInt8*)bufP, bufSize, 1, fsP) != 1)
	goto ExitWriteErr;

  free (bufP);
  bufP = 0;

  // If there's space between, write out FF's
  if (bigRomOffset > bufSize)
	{
	  bufP = malloc (bigRomOffset - bufSize);
	  if (!bufP) goto ExitNoMem;
	  memset (bufP, 0xFF, bigRomOffset - bufSize);

	  if (fwrite ((UInt8*)bufP, bigRomOffset - bufSize, 1, fsP) != 1)
		goto ExitWriteErr;

	  free (bufP);
	  bufP = 0;
	}


  //--------------------------------------------------------
  // Write out the big ROM portion
  //--------------------------------------------------------
  // Read in the big ROM file
  err = AppReadFile (bigRomP, &bufP, &bufSize);
  if (err) goto Exit;

  if (fwrite ((UInt8*)bufP, bufSize, 1, fsP) != 1)
	goto ExitWriteErr;

  err = 0;
  goto Exit;


ExitWriteErr:
  printf ("\nERROR: while writing to output file '%s'", outNameP);
  err = 1;
  goto Exit;

ExitNoMem:
  printf ("\nERROR: out of memory");
  err = 1;


Exit:
  if (err)
	{
	  printf ("\nERROR: %ld while performing split operation", err);
	}

  if (bufP) free (bufP);
  if (fsP) fclose (fsP);
  return err;
}



/***************************************************************
 *	Function:	  PrvExportDatabase
 *
 *	Summary:	  Creates a file in the local directory corresponding
 *		  to given database in current ROM image.
 *
 *	Parameters:
 *	  hdrP		IN	  pointer to database hdr in current ROM image.
 *
 *	Returns:
 *	  0 if no error
 *
 *	Called By:
 *	  PrvDoBreak()
 *
 *	History:
 *	  24-Dec-98 RM Ceated by Ron Marianetti
 *
 ****************************************************************/
static UInt32
PrvExportDatabase (DatabaseHdrType*	hdrP)
{
  FILE*			  fsP = 0;
  char			  outNameP[dmDBNameLength+16];
  char*           spaceP;
  UInt32			  err = 0;
  UInt32			  rIdx;
  UInt32			  localID;
  DatabaseHdrType hdr, fileHdr;
  UInt32			  numRecords;
  RecordListType* hostListP = 0;
  UInt32			  size;
  UInt32			  recListFileOffset;
  Boolean		  recordDB = false;


  // Make a local copy in host byte order
  hdr = *hdrP;
  PToH (hdr);

  // Make a copy of header for updated appInfo and sortInfo
  fileHdr = *hdrP;

  // Clear the read-only bit when we write it out
  fileHdr.attributes = HToPW ( PToHW (fileHdr.attributes) & ~dmHdrAttrReadOnly);

  // See if it's a record or resource database
  if (! (hdr.attributes & dmHdrAttrResDB))
	recordDB = true;



  // --------------------------------------------------------------
  //  Create the file
  // --------------------------------------------------------------
  if (recordDB)
	sprintf (outNameP, "%s.pdb", hdrP->name);
  else
	sprintf (outNameP, "%s.prc", hdrP->name);

  /* <chg 2-Sep-1999 BP> optionally allow spaces, for building the 1.0 ROM */
  if (OptNoSpaces)
	{
	  // <chg 09-aug-99 dia> Get rid of spaces!
	  spaceP = strchr (outNameP, ' ');
	  while (spaceP != NULL)
		{
		  *spaceP = '_';
		  spaceP = strchr (outNameP, ' ');
		}
	}

  fsP = fopen (outNameP, "wb");
  if (!fsP) goto ExitWriteErr;

  // Print progress
  printf ("\nWriting out database: \"%s\"...", outNameP);

  // Write out the header
  if (fwrite (hdrP, sizeof (DatabaseHdrType)- sizeof (RecordListType),
				1, fsP) != 1)
	goto ExitWriteErr;


  // --------------------------------------------------------------
  // Write out a filler resource/record list
  // --------------------------------------------------------------

  // Determine how many resources there are
  RecordListType* listP;
  listP = (RecordListType*)&hdrP->recordList;
  numRecords = 0;
  while (listP)
	{
	  numRecords += PToHW (listP->numRecords);

	  // Next record list
	  localID = PToHDw (listP->nextRecordListID);
	  if (!localID) break;
	  listP = (RecordListType*)DevMemLocalIDToPtr (localID);
	}

  // Allocate one now and write it out
  if (recordDB)
	{
	  size = sizeof (RecordListType) + numRecords * sizeof (RecordEntryType);
	}
  else
	{
	  size = sizeof (RecordListType) + numRecords * sizeof (RsrcEntryType);
	}

  hostListP = (RecordListType*) malloc (size);
  if (!hostListP) goto ExitNoMem;
  memset (hostListP, 0, size);

  hostListP->numRecords = HToPW (numRecords);
  recListFileOffset = ftell (fsP);
  if (fwrite (hostListP, size, 1, fsP) != 1)
	goto ExitWriteErr;


  // --------------------------------------------------------------
  // Write out appInfo, if any
  // --------------------------------------------------------------
  if (hdrP->appInfoID)
	{
	  UInt8* dataP;
	  UInt32	  dataSize;

	  localID = PToHDw (hdrP->appInfoID);
	  dataP = (UInt8*)DevMemLocalIDToPtr (localID);
	  dataSize = DevMemPtrSize (dataP);

	  // Fix up appInfoID with offset to it
	  fileHdr.appInfoID = HToPDw (ftell (fsP));

	  // Write out the record
	  if (fwrite (dataP, dataSize, 1, fsP) != 1)
		goto ExitWriteErr;

	}

  // --------------------------------------------------------------
  // Write out sortInfo, if any
  // --------------------------------------------------------------
  if (hdrP->sortInfoID)
	{
	  UInt8* dataP;
	  UInt32	  dataSize;

	  localID = PToHDw (hdrP->sortInfoID);
	  dataP = (UInt8*)DevMemLocalIDToPtr (localID);
	  dataSize = DevMemPtrSize (dataP);

	  // Fix up sortInfo with offset to it
	  fileHdr.sortInfoID = HToPDw (ftell (fsP));

	  // Write out the record
	  if (fwrite (dataP, dataSize, 1, fsP) != 1)
		goto ExitWriteErr;

	}



  // --------------------------------------------------------------
  // Write out each of the records now and fix up the record/record list
  //  as we go along
  // --------------------------------------------------------------
  if (recordDB)
	{
	  RecordEntryType*  hostEntryP;

	  hostEntryP = (RecordEntryType*)&hostListP->firstEntry;
	  listP = (RecordListType*)&hdrP->recordList;
	  while (listP)
		{
		  RecordEntryType*	srcEntryP;
		  srcEntryP = (RecordEntryType*)&listP->firstEntry;
		  for (rIdx=0; rIdx<PToHW (listP->numRecords); rIdx++)
			{
			  UInt8*	  dataP;
			  UInt32		  dataSize;

			  *hostEntryP = *srcEntryP;
			  hostEntryP->localChunkID = HToPDw (ftell (fsP));

			  localID = PToHDw (srcEntryP->localChunkID);
			  if (localID)
				{
				  dataP = (UInt8*)DevMemLocalIDToPtr (localID);
				  dataSize = DevMemPtrSize (dataP);

				  // Write out the record
				  if (fwrite (dataP, dataSize, 1, fsP) != 1)
					goto ExitWriteErr;
				}

			  srcEntryP++;
			  hostEntryP++;
			}

		  // Next record list
		  localID = listP->nextRecordListID;
		  if (!localID) break;
		  listP = (RecordListType*)DevMemLocalIDToPtr (localID);
		}
	}


  // --------------------------------------------------------------
  // Write out each of the resources now and fix up the record/record list
  //  as we go along
  // --------------------------------------------------------------
  else
	{
	  RsrcEntryType*  hostEntryP;

	  hostEntryP = (RsrcEntryType*)&hostListP->firstEntry;
	  listP = (RecordListType*)&hdrP->recordList;
	  while (listP)
		{
		  RsrcEntryType*	srcEntryP;
		  srcEntryP = (RsrcEntryType*)&listP->firstEntry;
		  for (rIdx=0; rIdx<PToHW (listP->numRecords); rIdx++)
			{
			  UInt8*	  dataP;
			  UInt32		  dataSize;

			  *hostEntryP = *srcEntryP;
			  hostEntryP->localChunkID = HToPDw (ftell (fsP));

			  localID = PToHDw (srcEntryP->localChunkID);
			  dataP = (UInt8*)DevMemLocalIDToPtr (localID);
			  dataSize = DevMemPtrSize (dataP);

			  // Write out the resource
			  if (fwrite (dataP, dataSize, 1, fsP) != 1)
				goto ExitWriteErr;

			  srcEntryP++;
			  hostEntryP++;
			}

		  // Next record list
		  localID = listP->nextRecordListID;
		  if (!localID) break;
		  listP = (RecordListType*)DevMemLocalIDToPtr (localID);
		}
	}




  // --------------------------------------------------------------
  // Go back and write out the fixed up record list and file header
  // --------------------------------------------------------------
  // Write out the header
  fseek (fsP, 0, SEEK_SET);
  if (fwrite (&fileHdr, sizeof (DatabaseHdrType)- sizeof (RecordListType),
				1, fsP) != 1)
	goto ExitWriteErr;

  fseek (fsP, recListFileOffset, SEEK_SET);
  if (fwrite (hostListP, size, 1, fsP) != 1)
	goto ExitWriteErr;

  err = 0;
  goto Exit;


ExitWriteErr:
  if (outNameP)
	printf ("\nERROR: while writing to output file '%s'", outNameP);
  err = 1;
  goto Exit;

ExitNoMem:
  printf ("\nERROR: out of memory");
  err = 1;


Exit:
  if (err)
	{
	  printf ("\nERROR: %ld while performing break operation", err);
	}

  if (hostListP) free (hostListP);
  if (fsP) fclose (fsP);
  return err;
}






/***************************************************************
 *	Function:	  PrvDoBreak
 *
 *	Summary:	  Write out all the PRC files within the current ROM image
 *	  out to the current directory.
 *
 *	Parameters:
 *	  justPrintOptions	IN	if true, simply print out command line options
 *							  required to create a ROM from the current set
 *							  of databases
 *
 *	Returns:
 *	  0 if no error
 *
 *	Called By:
 *	  main() when parsing the -patch option
 *
 *	History:
 *	  15-Dec-98 RM Ceated by Ron Marianetti
 *
 ****************************************************************/
static UInt32
PrvDoBreak (Boolean justPrintOptions)
{
  void*	  bufP = 0;
  FILE*	  fsP = 0;
  UInt32	  err = 0;
  UInt32	  dbIdx;
  UInt32	  localID;

  // err if no image
  if (!ROMImageP)
	{
	  printf ("\nERROR: No image specified to break apart");
	  err = 1;
	  goto Exit;
	}

  //==================================================================
  // Locate the Directory of databases
  //=================================================================
  DatabaseDirType*	  dirP;
  dirP = (DatabaseDirType*) DevMemLocalIDToPtr (ROMStoreHdr.databaseDirID);

  while (dirP)
	{

	  // ===========================================================
	  // Loop through the databases in this list
	  // ===========================================================
	  for (dbIdx=0; dbIdx < PToHW (dirP->numDatabases); dbIdx++)
		{
		  DatabaseHdrType*	hdrP;

		  localID = PToHDw (dirP->databaseID[dbIdx]);
		  hdrP = (DatabaseHdrType*)DevMemLocalIDToPtr (localID);

		  // If just printing options, print options now
		  if (justPrintOptions)
			  printf ("\n  -romDB \"%s.prc\"", hdrP->name);

		  // Otherwise, Write it out
		  else
			  err = PrvExportDatabase (hdrP);
		  if (err) goto Exit;


		} // for (dbIdx=0; dbIdx < PToHW (dirP->numDatabases); dbIdx++)



	  //==============================================================
	  // Next database list, if present
	  //==============================================================
	  if (dirP->nextDatabaseListID)
		{
		  localID = PToHDw (dirP->nextDatabaseListID);
		  dirP = (DatabaseDirType*) DevMemLocalIDToPtr (localID);
		}
	  else
		dirP = 0;
	}


Exit:
  if (err)
	{
	  printf ("\nERROR: %ld while performing break operation", err);
	}

  if (bufP) free (bufP);
  if (fsP) fclose (fsP);
  return err;
}




/***************************************************************
 *	Function:	  PrvImportDatabase
 *
 *	Summary:	  Imports a PRC file from the host into card image.
 *
 *	Parameters:
 *	  nameP	  IN  name of PRC file on host
 *	  bootDB  IN  true if this is the boot database that we should look for
 *					boot=10000, 10001, 10002, & 19000 and put offsets into the
 *					card header for them. Note, boot=19000 is the halDispatch.
 *
 *
 *	Returns:
 *	  localID of database header, or 0 on error
 *
 *	Called By:
 *	  PrvDoCreate()
 *
 *	History:
 *	  24-Dec-98 RM Ceated by Ron Marianetti
 *
 ****************************************************************/
static UInt32
PrvImportDatabase (char* nameP, Boolean bootDB)
{
  void*				  bufP = 0;
  UInt32			  bufSize = 0;

  DatabaseHdrType*	  srcHdrP;
  DatabaseHdrType*	  dstHdrP;
  UInt32			  hdrLocalID = 0;
  UInt32			  hdrSize;

  UInt32			  rIdx;

  UInt8*			  dstDataP;

  UInt32			  err = 0;
  UInt32			  localID;
  UInt32			  numRecords;
  Boolean			  recordDB = false;
  UInt32			  firstOffset;


  // --------------------------------------------------------------
  //  Read in the PRC file now
  // --------------------------------------------------------------
  err =	AppReadFile (nameP, &bufP, &bufSize);
  if (err) goto Exit;

  // See how big the header chunk has to be
  srcHdrP = (DatabaseHdrType*)bufP;
  numRecords = PToHW (srcHdrP->recordList.numRecords);


  // See if it's a record or resource database
  if ((PToHW (srcHdrP->attributes) & dmHdrAttrResDB) == 0)
	recordDB = true;

  if (recordDB)
	{
	  hdrSize = sizeof (*srcHdrP) + numRecords * sizeof (RecordEntryType);
	  if (numRecords)
		{
		  RecordEntryType*  srcEntryP;
		  srcEntryP = (RecordEntryType*)&(srcHdrP->recordList.firstEntry);
		  firstOffset = PToHDw (srcEntryP->localChunkID);
		}
	  else
		firstOffset = 0;
	}
  else
	{
	  hdrSize = sizeof (*srcHdrP) + numRecords * sizeof (RsrcEntryType);
	  if (numRecords)
		{
		  RsrcEntryType*  srcEntryP;
		  srcEntryP = (RsrcEntryType*)&(srcHdrP->recordList.firstEntry);
		  firstOffset = PToHDw (srcEntryP->localChunkID);
		}
	  else
		firstOffset = 0;
	}


  // --------------------------------------------------------------
  // Allocate the header chunk and get it's local ID
  // --------------------------------------------------------------
  dstHdrP = (DatabaseHdrType*) DevMemPtrNew (hdrSize, dmMgrOwnerID,
											  true /*atEnd*/);
  if (!dstHdrP) goto ExitNoDevMem;
  hdrLocalID = DevMemPtrToLocalID (dstHdrP);

  // Fill in the header
  memmove (dstHdrP, bufP, hdrSize);

  // <chg 12-Jul-00 JRP> Added OptNoForceReadOnly to bypass setting the
  // read-only bit in PRC headers.

  if (!OptNoForceReadOnly)
	{
	  // Set the read-only bit in the header since this database is going into ROM
	  dstHdrP->attributes = HToPW (PToHW (dstHdrP->attributes) | dmHdrAttrReadOnly);
	}

  // <chg 13-Jul-00 JRP> Added OptCopyPrevention to force setting the
  // copy prevention bit in PRC headers.

  if (OptCopyPrevention)
	{
	  dstHdrP->attributes = HToPW (PToHW (dstHdrP->attributes) | dmHdrAttrCopyPrevention);
	}


  // ==================================================================
  // MemHandle the appInfo and sortInfo blocks
  // ==================================================================
  if (srcHdrP->appInfoID)
	{
	  UInt32	  recSize;
	  UInt32	  appInfoOffset;

	  appInfoOffset = PToHDw (srcHdrP->appInfoID);

	  // Get the size
	  if (srcHdrP->sortInfoID)
		recSize = PToHDw (srcHdrP->sortInfoID) - appInfoOffset;
	  else
		{
		  if (firstOffset)
			recSize = firstOffset - appInfoOffset;
		  else
			recSize = bufSize - appInfoOffset;
		}


	  // Allocate a chunk
	  dstDataP = (UInt8*) DevMemPtrNew (recSize, dmRecOwnerID, false);
	  if (!dstDataP) goto ExitNoDevMem;

	  // Copy the data in
	  memmove (dstDataP, appInfoOffset + (UInt8*)bufP, recSize);

	  // Put in the local ID of the appInfo into the header
	  localID = DevMemPtrToLocalID (dstDataP);
	  dstHdrP->appInfoID = HToPDw (localID);
	}

  if (srcHdrP->sortInfoID)
	{
	  UInt32	  recSize;
	  UInt32	  sortInfoOffset;

	  sortInfoOffset = PToHDw (srcHdrP->sortInfoID);

	  // Get the size
	  if (firstOffset)
		recSize = firstOffset - sortInfoOffset;
	  else
		recSize = bufSize - sortInfoOffset;


	  // Allocate a chunk
	  dstDataP = (UInt8*) DevMemPtrNew (recSize, dmRecOwnerID, false);
	  if (!dstDataP) goto ExitNoDevMem;

	  // Copy the data in
	  memmove (dstDataP, sortInfoOffset + (UInt8*)bufP, recSize);

	  // Put in the local ID of the appInfo into the header
	  localID = DevMemPtrToLocalID (dstDataP);
	  dstHdrP->appInfoID = HToPDw (localID);
	}



  // ==================================================================
  // MemHandle Record DBs
  // ==================================================================
  if (recordDB)
	{
	  RecordEntryType*  dstEntryP;
	  RecordEntryType*  srcEntryP;

	  // Get pointer to the first entry in the record list
	  dstEntryP = (RecordEntryType*)&(dstHdrP->recordList.firstEntry);
	  srcEntryP = (RecordEntryType*)&(srcHdrP->recordList.firstEntry);


	  // --------------------------------------------------------------
	  // Now, allocate chunks for each of the records and update the
	  //  header appropriately.
	  // --------------------------------------------------------------
	  for (rIdx = 0; rIdx < numRecords; rIdx++)
		{
		  UInt32	  recSize;

		  // get the size
		  if (rIdx == numRecords-1)
			recSize = bufSize - PToHDw (srcEntryP->localChunkID);
		  else
			recSize = PToHDw ((srcEntryP+1)->localChunkID)
					- PToHDw (srcEntryP->localChunkID);

		  // Allocate a chunk
		  if (recSize)
			{
			  dstDataP = (UInt8*) DevMemPtrNew (recSize, dmRecOwnerID, false);
			  if (!dstDataP) goto ExitNoDevMem;

			  // Copy the data in
			  memmove (dstDataP, PToHDw (srcEntryP->localChunkID) + (UInt8*)bufP, recSize);

			  // Put in the local ID of the record into the record list
			  localID = DevMemPtrToLocalID (dstDataP);
			}
		  else
			localID = 0;

		  dstEntryP->localChunkID = HToPDw (localID);


		  // Next entries
		  srcEntryP++;
		  dstEntryP++;
		}

	}


  // ==================================================================
  // MemHandle resource DBs
  // ==================================================================
  else
	{
	  RsrcEntryType*  dstEntryP;
	  RsrcEntryType*  srcEntryP;

	  // Get pointer to the first entry in the record list
	  dstEntryP = (RsrcEntryType*)&(dstHdrP->recordList.firstEntry);
	  srcEntryP = (RsrcEntryType*)&(srcHdrP->recordList.firstEntry);



	  // --------------------------------------------------------------
	  // Now, allocate chunks for each of the resources and update the
	  //  header appropriately.
	  // --------------------------------------------------------------
	  for (rIdx = 0; rIdx < numRecords; rIdx++)
		{
		  UInt32	  recSize;

		  // get the size
		  if (rIdx == numRecords-1)
			recSize = bufSize - PToHDw (srcEntryP->localChunkID);
		  else
			recSize = PToHDw ((srcEntryP+1)->localChunkID)
					- PToHDw (srcEntryP->localChunkID);

		  // Allocate a chunk
		  dstDataP = (UInt8*) DevMemPtrNew (recSize, dmRecOwnerID, false);
		  if (!dstDataP) goto ExitNoDevMem;

		  // Copy the data in
		  memmove (dstDataP, PToHDw (srcEntryP->localChunkID) + (UInt8*)bufP, recSize);

		  // Put in the local ID of the resource into the resource list
		  localID = DevMemPtrToLocalID (dstDataP);
		  dstEntryP->localChunkID = HToPDw (localID);


		  // If this is the boot database, look for the special 'boot' resources
		  if (bootDB)
			{
			  if (srcEntryP->type == HToPDw('boot') && srcEntryP->id == HToPW(10000))
				{
				  CardHdr.resetVector = localID;
				  if (OptResetAt0)
					CardHdr.resetVector -= CardBase;

				  // -------------------------------------------------------------------------
				  // A typical code in the reset vector is structured like this:
				  //
				  // 10000000:	DC.L stack
				  //            DC.L resetVector
				  //			....
				  //
				  // 1000022A   JMP       *+$0042                     ; 0001026C    | 4EFA 0040
				  // ....		  ....
				  // 10000268   DC.L      00000268                                  | 0000 0268
				  // 1000026C   BRA.S     *+$0006                     ; 00010272    | 6004
				  // 1000026E   BRA.S     *+$0008                     ; 00010276    | 6006
				  //
				  // Notice the DC.L that appears at 100000268 in this example. We need to
				  //  manually fill this DC.L in with the offset of this address from the
				  //  start of the card header (0x268 in this case). We find this value by
				  //  following the target of the reset JMP instruction.
				  // -------------------------------------------------------------------------
				  //
				  UInt32		magicLocOffset, magicValue;

				  // Error check
				  if (dstDataP[0] != 0x4E || dstDataP[1] != 0xFA)
					{
					  printf ("\nERROR: Unexpected start of reset code - should be 0x4EFA...\n");
					  err = 1;
					  goto Exit;
					}

				  // Fill it in
				  magicLocOffset = (dstDataP[2] << 8) + dstDataP[3] + 2 - 4;
				  magicValue = magicLocOffset  + ((UInt32)dstDataP - (UInt32)ROMImageP);
				  *((UInt32*)(dstDataP + magicLocOffset)) = HToPDw (magicValue);
				}

			  if (srcEntryP->type == HToPDw('boot') && srcEntryP->id == HToPW(10001))
				{
				  ROMStoreHdr.initCodeOffset1 = localID;
				}
			  if (srcEntryP->type == HToPDw('boot') && srcEntryP->id == HToPW(10002))
				{
				  ROMStoreHdr.initCodeOffset2 = localID;
				}

			  if (srcEntryP->type == HToPDw('boot') && srcEntryP->id == HToPW(19000))
				{
				  // NOTE: The halDispatch is stored as an offset from the start
				  //  of the card header not from the card base address
				  CardHdr.halDispatch = (UInt32)dstDataP - (UInt32)ROMImageP;
				}
			}


		  // If this is the system resource file, look for the splash
		  //  and hard reset screens and store those in the NVParams
		  if (srcHdrP->creator == HToPDw ('psys'))
			{
			  if (srcEntryP->type == HToPDw('Tbsb')
				  && srcEntryP->id == HToPW (19000))
				{
				  ROMStoreHdr.nvParams.splashScreenPtr = localID;
				}
			  if (srcEntryP->type == HToPDw('Tbsb')
				  && srcEntryP->id == HToPW (19001))
				{
				  ROMStoreHdr.nvParams.hardResetScreenPtr = localID;
				}
			}


		  // Next entries
		  srcEntryP++;
		  dstEntryP++;
		}
	}



  err = 0;
  goto Exit;


ExitNoDevMem:
  printf ("\nERROR: out of memory in ROM image");
  err = 1;



Exit:
  if (err)
	{
	  hdrLocalID = 0;
	  printf ("\nERROR: %ld while performing import operation", err);
	}

  if (bufP) free (bufP);
  return hdrLocalID;
}




/***************************************************************
 *	Function:	  PrvDBCompare
 *
 *	Summary:	  Callback routine passed to qsort to sort the databases
 *	  in order by type/creator
 *
 *	Parameters:
 *	  db1		  IN	localID of database 1
 *	  db2		  IN	localID of database 2
 *
 *	Returns:
 *	  <0	db1 comes before db2
 *	  0		db1 == db2
 *	  >0	db1 comes after db2
 *
 *	Called By:
 *	  qsort() routine which is called from PrvDoCreate()
 *
 *	History:
 *	  27-Dec-98 RM Ceated by Ron Marianetti
 *
 ****************************************************************/
int PrvDBCompare( const void* db1IdP, const void* db2IdP )
{
  DatabaseHdrType*	db1P;
  DatabaseHdrType*	db2P;
  UInt32				type1, type2, cr1, cr2;
  UInt16				ver1, ver2;

  UInt32				localID1, localID2;

  localID1 = *((UInt32*)db1IdP);
  localID2 = *((UInt32*)db2IdP);

  // Get pointers to the two database headers
  db1P = (DatabaseHdrType*)DevMemLocalIDToPtr (PToHDw(localID1));
  db2P = (DatabaseHdrType*)DevMemLocalIDToPtr (PToHDw(localID2));

  // get the types, creators, and versions
  type1 = PToHDw (db1P->type);
  type2 = PToHDw (db2P->type);
  cr1 = PToHDw (db1P->creator);
  cr2 = PToHDw (db2P->creator);
  ver1 = PToHW (db1P->version);
  ver2 = PToHW (db2P->version);

  // Sort by type, then creator, then version
  if (type1 < type2) return -1;
  else if (type1 > type2) return 1;

  if (cr1 < cr2) return -1;
  else if (cr1 > cr2) return 1;

  if (ver1 > ver2) return -1;
  else if (ver1 < ver2) return 1;

  return 0;
}



/***************************************************************
 *	Function:	  PrvHStrToI
 *
 *	Summary:	  Convert a string containing hex characters into
 *	  a byte array. The caller must insure that the hexP array
 *	  is large enough to hold resulting conversion (i.e. strlen (strP) / 2).
 *
 *	Parameters:
 *	  strP		IN	  pointer to string with hex characters
 *
 *	Returns:
 *	  0 if no error
 *
 *	Called By:
 *	  main() when parsing the -tokHex option
 *
 *	History:
 *	  19-May-1999 Ceated by Ron Marianetti
 *
 ****************************************************************/
static int
PrvHStrToI (char* strP, UInt8* hexP)
{
  int	  byte;
  char	  c;

  while (*strP)
	{
	  c = *strP++;

	  // ----------------------------------------------------------
	  // First char of hex pair
	  // ----------------------------------------------------------
	  if (!isxdigit (c))
		return -1;

	  if (c >= 'a')
		byte = c - 'a' + 10;
	  else if (c >= 'A')
		byte = c - 'A' + 10;
	  else
		byte = c - '0';

	  byte <<= 4;


	  // ----------------------------------------------------------
	  // Second char of hex pair
	  // ----------------------------------------------------------
	  c = *strP++;
	  if (!isxdigit (c))
		return -1;

	  if (c >= 'a')
		byte += c - 'a' + 10;
	  else if (c >= 'A')
		byte += c - 'A' + 10;
	  else
		byte += c - '0';



	  // Write it out
	  *hexP++ = (UInt8)byte;
	}

  return 0;
}


/****************************************************************
 *	Function:	PrvCalcROMTokenSize
 *
 *	Summary:
 *	  calculates the total size of ROM token area, including the
 *		end-of-token marker
 *
 *	Parameters:
 *	  none
 *
 *	Returns:
 *	  size of ROM token area
 *
 *	Called by:
 *	  DoCreate()
 *
 *	Notes:
 *
 *
 *	History:
 *	  22-Sep-1999 	BP	Created
 *
 *****************************************************************/
static UInt32
PrvCalcROMTokenSize (void)
{
  unsigned int i;
  UInt32 tokenSize;

  tokenSize = 0;

  if (NumROMTokens == 0)
	return 0;

  // ----------------------------------------------------
  // add up size of all ROM tokens
  // ----------------------------------------------------
  for (i=0; i<NumROMTokens; i++)
	{
	  tokenSize += ROMTokens[i].len + sizeof (UInt32 /*token*/) + sizeof (UInt16 /*len*/);

	  // tokens must be aligned to word boundary
	  if (ROMTokens[i].len & (UInt16)0x01)
		tokenSize += 1;

	   // pointers have a stored pointer in addition to data
	  if (ROMTokens[i].ptr)
		tokenSize += sizeof(UInt32);
	}

  // ----------------------------------------------------
  // add in end-of-tokens block (0xFFFFFFFF)
  // ----------------------------------------------------
  tokenSize += sizeof(UInt32);

  return tokenSize;
}


/***************************************************************
 *	Function:	  PrvDoCreate
 *
 *	Summary:	  Create a ROM image from a set of PRC files
 *
 *	Parameters:
 *	  none
 *
 *	Returns:
 *	  0 if no error
 *
 *	Called By:
 *	  main() when parsing the -op create option
 *
 *	History:
 *	  24-Dec-98 RM Ceated by Ron Marianetti
 *
 ****************************************************************/
static UInt32
PrvDoCreate (void)
{
  UInt32				  err;
  CardHeaderType*	  cardP;
  UInt8*			  bP;
  UInt8*			  heapP;
  UInt16				  dirIdx, dbIdx;


  // ----------------------------------------------------
  // if automatically resizing ROM, recalculate ReadOnlyParmsSize
  // ----------------------------------------------------
  if (OptAutoSize)
	{
	  UInt32 oldTokenSize = ReadOnlyParmsSize;
	  ReadOnlyParmsSize = PrvCalcROMTokenSize();
	  ROMBlock[1] -= (ReadOnlyParmsSize - oldTokenSize);

	  // <chg 2000-06-26 BP>  If there are no tokens, set the offset to zero.
	  // Otherwise, you will point off the end of the ROM.
	  if (ReadOnlyParmsSize)
		CardHdr.readOnlyParmsOffset = (ROMBlock[0] + ROMBlock[1] - ReadOnlyParmsSize);
	  else
		  CardHdr.readOnlyParmsOffset = 0;
	}


  // ----------------------------------------------------
  // Allocate space for the card image
  // ----------------------------------------------------
  ROMImageP = malloc (ROMBlock[1]);
  if (!ROMImageP) goto ExitNoMem;
  memset (ROMImageP, 0, ROMBlock[1]);
  ROMImageSize = ROMBlock[1];


  // The format of the card header area is:
  //  CardHeaderType	  cardHdr;			(0x100 bytes)
  //  StorageHeaderType	  romStore;			(0x100) bytes
  //  UInt32				  romBlockList[2]	(usually 8 bytes)
  //  HeapListType		  heapList			(usually 10 bytes)
  //  HeapHeaderType	  romHeap			(usually 16 bytes)
  //  [chunks]

  // ----------------------------------------------------
  // Update out the card header in existing ROM image, we will fix it
  //  up later when we have the offset to the RAM block list.
  // ----------------------------------------------------
  cardP = (CardHeaderType*)ROMImageP;
  *cardP = CardHdr;
  PToH (*cardP);


  // ----------------------------------------------------
  // Update out the storage header in existing ROM image, we will fix it
  //  up later once we have a database directory local ID
  // ----------------------------------------------------
  StorageHeaderType* storeP;
  storeP = (StorageHeaderType*)(cardP+1);
  *storeP = ROMStoreHdr;
  PToH (*storeP);


  // ----------------------------------------------------
  // Write out the RAM block list and update blockListOffset in card header
  // ----------------------------------------------------
  bP = (UInt8*)(storeP+1);
  CardHdr.blockListOffset = DevMemPtrToLocalID (bP);
  *((UInt32*)bP) = PToHDw (RAMBlock[0]);
  bP += sizeof (UInt32);
  *((UInt32*)bP) = PToHDw (RAMBlock[1]);
  bP += sizeof (UInt32);

  // ----------------------------------------------------
  // Write out the heap list, put in an extra NULL UInt32 after this
  //  which is unnecssary but makes for easier verification by comparing
  //  with existing ROM images which also have this NULL in them.
  // ----------------------------------------------------
  HeapListType	  heapList;
  memset (&heapList, 0, sizeof (heapList));
  heapList.numHeaps = HToPW (1);
  heapP = bP + sizeof (HeapListType) + sizeof (UInt32);
  heapList.heapOffset[0] = HToPDw (DevMemPtrToLocalID (heapP));

  ROMStoreHdr.heapListOffset = DevMemPtrToLocalID (bP);
  memmove (bP, &heapList, sizeof (heapList));
  bP = heapP;


  // ----------------------------------------------------
  // Init the heap now, the size is the size of the ROM minus space
  //  for the card and storage headers and heap list at front and the
  //  read-only area (if present) at the end
  // ----------------------------------------------------
  UInt32	  heapSize;
  heapSize = ROMBlock[1] - ((UInt32)bP - (UInt32)ROMImageP);
  if (CardHdr.readOnlyParmsOffset && CardHdr.readOnlyParmsOffset > HdrOffset)
	heapSize -= ReadOnlyParmsSize;
  err = DevMemHeapInit (heapP, heapSize);
  if (err) goto Exit;


  // ----------------------------------------------------
  // Create a memory chunk for the database list
  // ----------------------------------------------------
  DatabaseDirType*	dirP;
  UInt32	  numDBs, size;
  numDBs = NumROMDBs;
  if (ROMBootDbNameP) numDBs++;
  if (ROMHalDbNameP) numDBs++;
  if (ROMPalmHalDbNameP) numDBs++;
  size = sizeof (DatabaseDirType) + (numDBs-1) * sizeof (UInt32);
  dirP = (DatabaseDirType*)DevMemPtrNew (size, dmMgrOwnerID, true /*atEnd*/);
  if (!dirP) goto ExitNoDevMem;

  // Put the local ID  into the storage header
  ROMStoreHdr.databaseDirID = DevMemPtrToLocalID (dirP);

  // Init the list
  dirP->nextDatabaseListID = 0;
  dirP->numDatabases = HToPW (numDBs);


  // ----------------------------------------------------
  // Loop through each of the DB's specified on the command line and
  //  add them to the heap
  // ----------------------------------------------------
  // First the boot database
  dirIdx = 0;
  UInt32	  localID;
  if (ROMBootDbNameP)
	{
	  localID = PrvImportDatabase (ROMBootDbNameP, true /*bootDB*/);
	  if (!localID) {err = 1; goto Exit;}

	  dirP->databaseID[dirIdx++] = HToPDw (localID);

	  // If we have a HalDB as well, copy the reset vector into the initStack
	  //  field as an offset from the card header
	  if (ROMHalDbNameP)
		CardHdr.initStack = CardHdr.resetVector - CardBase - HdrOffset;
	}

  if (ROMHalDbNameP)
	{
	  localID = PrvImportDatabase (ROMHalDbNameP, true /*bootDB*/);
	  if (!localID) {err = 1; goto Exit;}

	  dirP->databaseID[dirIdx++] = HToPDw (localID);
	}


  if (ROMPalmHalDbNameP)
	{
	  localID = PrvImportDatabase (ROMPalmHalDbNameP, true /*bootDB*/);
	  if (!localID) {err = 1; goto Exit;}

	  dirP->databaseID[dirIdx++] = HToPDw (localID);
	}


  for (dbIdx = 0; dbIdx < NumROMDBs; dbIdx++)
	{
	  localID = PrvImportDatabase (ROMDBNameP[dbIdx], false /*bootDB*/);
	  if (!localID) {err = 1; goto Exit;}

	  dirP->databaseID[dirIdx++] = HToPDw (localID);
	}


  // ----------------------------------------------------
  // Now, we must sort the databases by type then creator
  // ----------------------------------------------------
  qsort (&dirP->databaseID[0], numDBs, sizeof (UInt32), PrvDBCompare);


  // ----------------------------------------------------
  // Return now. The caller (main) will call PrvWriteOutROM when we
  //  return which will update the card header and
  //  ROM store area and add any ROM tokens which were specified as well.
  // ----------------------------------------------------
  err = 0;
  goto Exit;



ExitNoMem:
  printf ("\nERROR: out of memory");
  err = 1;
  goto Exit;

ExitNoDevMem:
  printf ("\nERROR: out of memory in ROM image");
  err = 1;


Exit:
  if (err)
	  printf ("\nERROR: %ld while creating ROM image", err);

  return err;

}



/***************************************************************
 *	Function:	  PrvResizeROM
 *
 *	Summary:	  Re-build ROM with
 *
 *	Parameters:
 *	  none
 *
 *	Returns:
 *	  0 if no error
 *
 *	Called By:
 *	  main() when parsing the -op create option
 *
 *	Notes:
 *	  This routine relies on DoCreate to calculate initial
 *	  sizes and offsets.
 *
 *	History:
 *	  22-Sep-1999 	BP	Created
 *
 *****************************************************************/
static UInt32 PrvResizeROM ()
{
  UInt32				  err;
  Mem3HeapHeaderType* heapHeader;
  UInt32				  heapSize;
  UInt32				  freeSize;
  UInt8*			  bP;

  err = 0;

  heapHeader = (PrvHeapP);
  heapSize = heapHeader->size;
  PToH(heapSize);


  // find size of free chunk in heap (last 24 bits of first UInt32 of header)
  bP = PrvFreeP;
  PToH ((UInt32&) *bP);
  freeSize = *(UInt32*) bP & 0x00FFFFFF;
  freeSize -= sizeof(MemChunkHeaderType);


  // reduce ROM size by free space
  ROMBlock[1] -= freeSize;
  heapSize -= freeSize;

  CardHdr.readOnlyParmsOffset = (UInt32) DevMemPtrToLocalID((UInt8*)heapHeader + heapSize);

  // free previously allocated structures for building ROM
  free(ROMImageP);
  PrvHeapP = 0;

  // now call DoCreate again to update ROM image
  err = PrvDoCreate();

  return err;
}



/***************************************************************
 *	Function:	  main
 *
 *	Summary:	  Entry point
 *
 *	Parameters:
 *	  argc	IN	  Number of arguments on the command line
 *	  argv	IN	  Command line arguments
 *
 *	Returns:
 *	  0 if no error
 *
 *	Called By:
 *	  shell
 *
 *	Notes:
 *
 *
 *	History:
 *	  15-Dec-98 RM Ceated by Ron Marianetti
 *
 ****************************************************************/

int main(int argc, char* argv[])
{
  int			  i;
  Boolean		  usageErr = false;
  int			  err = 0;
  char*			  errMsgP = 0;
  AppROMTokenType* tokenP;
  FILE*			  outFSP = 0;

  // Options
  char*			  infoFilenameP = 0;
  char*			  patchFilenameP = 0;
  char*			  joinSmNameP=0;
  char*			  joinBigNameP= 0;
  UInt32			  joinOffset = 0;
  char*			  splitNameP = 0;
  UInt32			  splitOffset = 0;
  char*			  breakNameP = 0;

  // The operation to perform
  PrvOpEnum		  op = prvOpNone;

  //==================================================
  // Init vars
  //==================================================




  //==================================================
  // Get Command line arguments
  //==================================================
  for (i=1; i<argc && !usageErr; i++)
	{

	  if (!strcmp (argv[i], "--help") || !strcmp (argv[i], "-h"))
		  goto Help;

	  // ------------------------------------------------------------
	  // Parse General options
	  // ------------------------------------------------------------
	  else if (!strcmp (argv[i], "-o"))
		{
		  if (i >= argc - 1) {usageErr = true; break;}
		  i++;

		  OutNameP = argv[i];
		}

	  else if (!strcmp (argv[i], "-noSpaces"))
		{
		  OptNoSpaces = true;
		}

	  else if (!strcmp (argv[i], "-noForceReadOnly"))
		{
		  OptNoForceReadOnly = true;
		}

	  else if (!strcmp (argv[i], "-autoSize"))
		{
		  OptAutoSize = true;
		}

	  else if (!strcmp (argv[i], "-copyPrevention"))
		{
		  OptCopyPrevention = true;
		}

	  // ------------------------------------------------------------
	  // Get the operation type
	  // ------------------------------------------------------------
	  else if (!strcmp (argv[i], "-op"))
		{
		  i++;
		  if (i >= argc || op != prvOpNone) {usageErr = true; break;}

		  if (!strcmp (argv[i], "info"))
			{
			  i++;
			  op = prvOpInfo;

			  // Read in the ROM Image now and init our CardHeader, ROM Store, etc.
			  //  from it.
			  if (i >= argc) {usageErr = true; break;}
			  infoFilenameP = argv[i];
			  err = PrvParseROMImage (infoFilenameP);
			  if (err) goto Exit;
			}

		  else if (!strcmp (argv[i], "patch"))
			{
			  i++;
			  op = prvOpPatch;

			  // Read in the ROM Image now and init our CardHeader, ROM Store, etc.
			  //  from it.
			  if (i >= argc) {usageErr = true; break;}
			  patchFilenameP = argv[i];
			  err = PrvParseROMImage (patchFilenameP);
			  if (err) goto Exit;
			}

		  else if (!strcmp (argv[i], "join"))
			{
			  i++;
			  op = prvOpJoin;

			  if (i+2 >= argc) {usageErr = true; break;}
			  joinSmNameP = argv[i++];
			  joinBigNameP = argv[i++];
			  joinOffset = strtoul (argv[i], 0, 0);
			}

		  else if (!strcmp (argv[i], "split"))
			{
			  i++;
			  op = prvOpSplit;

			  if (i+1 >= argc) {usageErr = true; break;}
			  splitNameP = argv[i++];
			  splitOffset = strtoul (argv[i], 0, 0);
			}

		  else if (!strcmp (argv[i], "create"))
			{
			  op = prvOpCreate;
			}

		  else if (!strcmp (argv[i], "break"))
			{
			  i++;
			  op = prvOpBreak;

			  if (i >= argc) {usageErr = true; break;}
			  breakNameP = argv[i];

			  err = PrvParseROMImage (breakNameP);
			  if (err) goto Exit;
			}

		  else
			{
			  usageErr = true; break;
			}
		}



	  // ------------------------------------------------------------
	  // Card Header options
	  // ------------------------------------------------------------
	  else if (!strcmp (argv[i], "-base"))
		{
		  if (i >= argc - 1) {usageErr = true; break;}
		  CardBase = strtoul (argv[++i], 0, 0);
		}

	  else if (!strcmp (argv[i], "-hdr"))
		{
		  if (i >= argc - 1) {usageErr = true; break;}
		  HdrOffset = strtoul (argv[++i], 0, 0);
		}

	  else if (!strcmp (argv[i], "-chRomTokens"))
		{
		  if (i >= argc - 1) {usageErr = true; break;}
		  CardHdr.readOnlyParmsOffset = strtoul (argv[++i], 0, 0);
		  if (i >= argc - 1) {usageErr = true; break;}
		  ReadOnlyParmsSize = strtoul (argv[++i], 0, 0);
		}


	  else if (!strcmp (argv[i], "-chBigRomOffset"))
		{
		  if (i >= argc - 1) {usageErr = true; break;}
		  CardHdr.bigROMOffset = strtoul (argv[++i], 0, 0);
		}


	  else if (!strcmp (argv[i], "-chName"))
		{
		  if (i >= argc - 1) {usageErr = true; break;}
		  memset (CardHdr.name, 0, sizeof (CardHdr.name));
		  strcpy ((char*)CardHdr.name, argv[++i]);
		}

	  else if (!strcmp (argv[i], "-chManuf"))
		{
		  if (i >= argc - 1) {usageErr = true; break;}
		  memset (CardHdr.manuf, 0, sizeof (CardHdr.manuf));
		  strcpy ((char*)CardHdr.manuf, argv[++i]);
		}

	  else if (!strcmp (argv[i], "-chVersion"))
		{
		  if (i >= argc - 1) {usageErr = true; break;}
		  CardHdr.version = (UInt16)strtoul (argv[++i], 0, 0);
		}

	  else if (!strcmp (argv[i], "-chStack"))
		{
		  if (i >= argc - 1) {usageErr = true; break;}
		  CardHdr.initStack = strtoul (argv[++i], 0, 0);
		}

	  else if (!strcmp (argv[i], "-chChecksum"))
		{
          // <chg 31-jan-00 dia> Allow size to be left out to specify whole ROM.
		  if (i >= argc - 1 || argv[i+1][0] == '-')
            {
              CardHdr.checksumBytes = (UInt32) -1;
            }
          else
            {
		      CardHdr.checksumBytes = strtoul (argv[++i], 0, 0);
            }
		}

	  else if (!strcmp (argv[i], "-chReset0"))
		{
		  OptResetAt0 = true;
		}

	  else if (!strcmp (argv[i], "-chZCrDate"))
		{
		  OptZCrDate = true;
		}

	  // ------------------------------------------------------------
	  // ROM Store options
	  // ------------------------------------------------------------
	  else if (!strcmp (argv[i], "-romName"))
		{
		  if (i >= argc - 1) {usageErr = true; break;}
		  memset (ROMStoreHdr.name, 0, sizeof (ROMStoreHdr.name));
		  strcpy ((char*)ROMStoreHdr.name, argv[++i]);
		}

	  else if (!strcmp (argv[i], "-romBootDB"))
		{
		  // Check for errors
		  if (op != prvOpCreate)
			{
			  printf ("\nERROR: -romBootDB is only valid with the -op create option");
			  usageErr = true;
			  break;
			}

		  if (i >= argc - 1) {usageErr = true; break;}
		  ROMBootDbNameP = argv[++i];
		}

	  else if (!strcmp (argv[i], "-romHalDB"))
		{
		  // Check for errors
		  if (op != prvOpCreate)
			{
			  printf ("\nERROR: -romHalDB is only valid with the -op create option");
			  usageErr = true;
			  break;
			}

		  if (i >= argc - 1) {usageErr = true; break;}
		  ROMHalDbNameP = argv[++i];
		}

	  else if (!strcmp (argv[i], "-romPalmHalDB"))
		{
		  // Check for errors
		  if (op != prvOpCreate)
			{
			  printf ("\nERROR: -romPalmHalDB is only valid with the -op create option");
			  usageErr = true;
			  break;
			}

		  if (i >= argc - 1) {usageErr = true; break;}
		  ROMPalmHalDbNameP = argv[++i];
		}

	  else if (!strcmp (argv[i], "-romDB"))
		{
		  // Check for errors
		  if (op != prvOpCreate)
			{
			  printf ("\nERROR: -romBootDB is only valid with the -op create option");
			  usageErr = true;
			  break;
			}

		  if (i >= argc - 1) {usageErr = true; break;}
		  if (NumROMDBs >= prvMaxDBs)
			{
			  printf ("\nERROR: Max number of databases (%d) exceeded", prvMaxDBs);
			  goto Exit;
			}

		  ROMDBNameP [NumROMDBs++] = argv[++i];
		}

	  else if (!strcmp (argv[i], "-romBlock"))
		{
		  if (i >= argc - 2) {usageErr = true; break;}
		  ROMBlock[0] = strtoul (argv[++i], 0, 0);
		  ROMBlock[1] = strtoul (argv[++i], 0, 0);
		}


	  // ------------------------------------------------------------
	  // RAM Store options
	  // ------------------------------------------------------------
	  else if (!strcmp (argv[i], "-ramBlock"))
		{
		  if (i >= argc - 2) {usageErr = true; break;}
		  RAMBlock[0] = strtoul (argv[++i], 0, 0);
		  RAMBlock[1] = strtoul (argv[++i], 0, 0);
		  CardHdr.numRAMBlocks = 1;
		}


	  // ------------------------------------------------------------
	  // Token Options
	  // ------------------------------------------------------------
	  else if (!strcmp (argv[i], "-tokStr"))
		{
		  if (i >= argc - 2) {usageErr = true; break;}

		  if (NumROMTokens >= prvMaxROMTokens-1)
			{
			  printf ("\nERROR: Too many tokens");
			  goto Exit;
			}
		  tokenP = &(ROMTokens [NumROMTokens++]);

		  tokenP->token = *((UInt32*)argv[++i]);
		  PToH (tokenP->token);
		  tokenP->dataP = _strdup (argv[++i]);
		  tokenP->len = (UInt16)strlen ((char*)tokenP->dataP);
		}


	  else if (!strcmp (argv[i], "-tokHex"))
		{
		  if (i >= argc - 2) {usageErr = true; break;}

		  if (NumROMTokens >= prvMaxROMTokens-1)
			{
			  printf ("\nERROR: Too many tokens");
			  goto Exit;
			}
		  tokenP = &(ROMTokens [NumROMTokens++]);

		  tokenP->token = *((UInt32*)argv[++i]);
		  PToH (tokenP->token);
		  tokenP->len = (UInt16)(strlen (argv[++i]) / 2);
		  tokenP->dataP = malloc (tokenP->len);
		  if (PrvHStrToI (argv[i], (UInt8*)tokenP->dataP))
			{
			  printf ("\nERROR: Invalid ROM token hex string: %s", argv[i]);
			  goto Exit;
			}
		}


	  // -tokBin <token> <filename>
	  else if (!strcmp (argv[i], "-tokBin"))
		{
		  if (i >= argc - 2) {usageErr = true; break;}

		  // Get next token space in array
		  if (NumROMTokens >= prvMaxROMTokens-1)
			{
			  printf ("\nERROR: Too many tokens");
			  goto Exit;
			}
		  tokenP = &(ROMTokens [NumROMTokens++]);


		  // Read the file into the token
		  tokenP->ptr = true;			  // this is a pointer token
		  tokenP->token = *((UInt32*)argv[++i]);
		  PToH (tokenP->token);
		  UInt32	  len;
		  err = AppReadFile (argv[++i], &tokenP->dataP, &len);

		  if (len > 0x0FFFF)
			{
			  printf ("\nERROR: Length of binary token file %s exceeds 64K",
				  argv[i]);
			  goto Exit;
			}
		  tokenP->len = (UInt16)len;
		  if (err) goto Exit;
		}


	  // -tokPrc <token> <prcName> <resType> <resID>
	  else if (!strcmp (argv[i], "-tokPrc"))
		{
		  UInt32	  resType;
		  char*	  prcNameP;
		  UInt16	  resID;

		  if (i >= argc - 4) {usageErr = true; break;}

		  // Get next token space in array
		  if (NumROMTokens >= prvMaxROMTokens-1)
			{
			  printf ("\nERROR: Too many tokens");
			  goto Exit;
			}
		  tokenP = &(ROMTokens [NumROMTokens++]);


		  // Get the token info and resource type and ID
		  tokenP->ptr = true;			  // this is a pointer token
		  tokenP->token = *((UInt32*)argv[++i]);
		  PToH (tokenP->token);
		  prcNameP = argv[++i];
		  resType = *((UInt32*)argv[++i]);
		  PToH (resType);
		  resID = (UInt16)strtoul (argv[++i], 0, 0);

		  // Get the resource out of the PRC now
		  UInt32	  len;
		  err = AppReadPrcResource (prcNameP, resType, resID,
					&tokenP->dataP, &len);

		  if (len > 0x0FFFF)
			{
			  printf ("\nERROR: Length of resource %s exceeds 64K",
				  argv[i]);
			  goto Exit;
			}
		  tokenP->len = (UInt16)len;
		  if (err) goto Exit;
		}



	  else
		usageErr = true;
	}


  // Exit if usage error
  if (usageErr || op == prvOpNone) goto Help;


  // Usage error if ROMBlock does not start at the card header
  if (ROMBlock[0] != HdrOffset)
	{
	  printf ("\nERROR: This implementation of MakeROM requires that the"
	          " Card Header is at the front of the ROM, i.e. the -hdr value "
			  " must be the same as the -romBlock value");
	  err = 1;
	  goto Exit;
	}


  //==================================================
  // Open up the output file for writing, if we need it
  //==================================================
  if (op != prvOpBreak && op != prvOpSplit && op != prvOpInfo)
	{
	  err = 1;
	  outFSP = fopen (OutNameP, "wb");
	  if (!outFSP)
		{
		  printf ("\nERROR: Could not create output file '%s'", OutNameP);
		  goto Exit;
		}
	}


  //==================================================
  // If printing info on existing ROM image
  //==================================================
  if (op == prvOpInfo)
	{
	  err = PrvPrintROMInfo ();
	}

  //==================================================
  // If simply patching an existing image, write out the
  //  updated image file. The exiting image was already read in
  //  when we parsed the -op patch command line option
  //==================================================
  if (op == prvOpPatch)
	{
	  err = PrvWriteOutROM (outFSP);
	}


  //==================================================
  // MemHandle the split operation
  //==================================================
  else if (op == prvOpSplit)
	{
	  err = PrvDoSplit (splitNameP, splitOffset, OutNameP);
	}

  //==================================================
  // MemHandle the join operation
  //==================================================
  else if (op == prvOpJoin)
	{
	  err = PrvDoJoin (joinSmNameP, joinBigNameP, joinOffset, OutNameP);
	}


  //==================================================
  // MemHandle the create operation
  //==================================================
  else if (op == prvOpCreate)
	{
	  err = PrvDoCreate ();
	  if (err) goto Exit;

	  if (OptAutoSize)
		err = PrvResizeROM ();
	  if (err) goto Exit;

	  // Write to the output file now
	  err = PrvWriteOutROM (outFSP);
	  if (err) goto Exit;
	}


  //==================================================
  // MemHandle the break operation
  //==================================================
  else if (op == prvOpBreak)
	{
	  err = PrvDoBreak (false /*justPrintOptions*/);
	}



  printf ("\n");
  goto Exit;


  //===================================================
  // Print Help
  //===================================================
Help:
  // If usage err, print offending option
  if (usageErr)
	printf ("ERROR: Bad Parameter: %s\n\n", argv[i]);


  printf ("PalmOS ROM image file generator");
  printf ("\nUsage: %s [options]", appProgName);
  printf (
	  "\n #==== General  Options ==============================="
	  "\n   -h, --help                    # print this help message"
	  "\n   -o <outFile>                  # [out.bin] output file name"
	  "\n   -noSpaces                     # replace spaces by underscores in"
	  "\n                                 #  PRC filenames when doing '-op break'"
	  "\n   -noForceReadOnly              # do not force each PRC to be marked"
	  "\n                                 #  read-only. otherwise, %s will set each"
	  "\n                                 #  database to be read-only."
	  "\n   -copyPrevention               # force each PRC to be marked with"
	  "\n                                 #  copy prevention"
	  "\n   -autoSize                     # use size parameter to -romBlock as"
	  "\n                                 #  optional maximum size and shrink ROM"
	  "\n                                 #  to fit input data."
	  "\n                                 #  '-chRomTokens' parameter not needed."
	  "\n "
	  "\n #==== Modes of operation (only 1 can be specified) ======="
	  "\n   -op info <romName>            # print info on existing ROM image"
	  "\n   -op patch <romName>           # name of existing ROM image file to patch."
	  "\n                                 #  MUST BE FIRST OPTION IF PRESENT!"
	  "\n   -op join <smallROM> <bigROM> <bigROMOffset> "
	  "\n                                 # join small and big ROMs into 1 image"
	  "\n   -op split <romName> <bigROMOffset>      "
	  "\n                                 # split <romName> into small and big ROMs"
	  "\n                                 #  with names of <outFile>1.bin and "
	  "\n                                 #  <outFile>2.bin "
	  "\n   -op create                    # create new ROM from set of PRC files"
	  "\n                                 #  using -romDB, -romBootDB options"
	  "\n   -op break <romName>           # write out set of PRC files within ROM "
	  "\n                                 #  image into current directory"
	  "\n "
	  "\n #==== Card Header Parameters ========================"
	  "\n   -base <start>                 # [0x%08lX] base address of card"
	  "\n   -hdr  <cardHdrOffset>         # [0x%08lX] offset to card header"
	  "\n   -chRomTokens <offset> <size>  # [0x%08lX 0x%08lX] offset to and max size"
	  "\n                                 #  for ROM Tokens area"
	  "\n   -chBigRomOffset <offset>      # [0x%08lX] offset to Big ROM from card base"
	  "\n   -chName <name>                # [\"%s\"] card name"
	  "\n   -chManuf <name>               # [\"%s\"] manufacturer name"
	  "\n   -chVersion <version>          # [%d] card version"
	  "\n   -chStack <initStack>          # [0x%08lX] initial stack pointer"
	  "\n   -chChecksum [<bytes>]         # [0x%08lX] # of bytes to checksum (if you specify "
	  "\n                                 #    no parameter, checksums the whole ROM."
	  "\n   -chReset0                     # Reset vector should point to alias of card"
	  "\n                                 #    header at 0x00000000"
      "\n   -chZCrDate                    # set creation date to 0 (for testing) "
	  "\n "
	  "\n #==== ROM Store Options ============================="
	  "\n   -romName <name>               # [\"%s\"] name of ROM store"
	  "\n   -romBootDB <filename>         # name of boot code PRC file"
	  "\n                                 #  (boot=10000 becomes reset vector)"
	  "\n                                 #  (boot=10001 becomes initCodeOffset1)"
	  "\n                                 #  (boot=10002 becomes initCodeOffset2)"
	  "\n   -romHalDB <filename>          # name of HAL PRC file"
	  "\n                                 #  (boot=10000 becomes reset vector)"
	  "\n                                 #  and -romBootDB's reset vector gets moved into"
	  "\n                                 #  initStack field of card header"
	  "\n   -romPalmHalDB <filename>      # replaces -romHalDB option for PalmOS 3.5"
	  "\n                                 #  (boot=19000 becomes halDispatch in CardHeader)"
	  "\n   -romDB <fileName> ...         # name of PRC file to put in ROM store"
	  "\n   -romBlock <offset> <size>     # [0x%08lX 0x%lX] offset and "
	  "\n                                 #   size of ROM block on card"
	  "\n "
	  "\n #==== RAM Store Options ============================="
	  "\n   -ramBlock <offset> <size> ... # [0x%lX 0x%lX] offset and size of RAM"
	  "\n                                 #   block on card"
	  "\n #==== ROM Token Options ============================="
	  "\n   -tokStr <type> <str>          # include string data as inline token"
	  "\n                                 #   ex: -tokStr \"snum\" \"1BX2NFP\" "
	  "\n   -tokHex <type> <str>          # include hex data as inline token"
	  "\n                                 #   ex: -tokHex \"snum\" \"FFAABBCC\" "
	  "\n   -tokBin <type> <filename>     # include binary file as pointer token"
	  "\n   -tokPrc <type> <filename> <resType <resID> "
	  "\n                                 # include resource from PRC file as "
	  "\n                                 #   pointer token"
	  "\n",
		  appProgName, (long)CardBase, (long)HdrOffset, (long)CardHdr.readOnlyParmsOffset,
		  (long)ReadOnlyParmsSize, (long)CardHdr.bigROMOffset, CardHdr.name,
		  CardHdr.manuf, (int)CardHdr.version, (long)CardHdr.initStack,
		  (long)CardHdr.checksumBytes,
		  ROMStoreHdr.name, (long)ROMBlock[0], (long)ROMBlock[1], (long)RAMBlock[0],
		  (long)RAMBlock[1]
	  );

  printf ("\n");
  err = 1;

Exit:
  // Clean up
  if (ROMImageP) free (ROMImageP);
  if (outFSP) fclose (outFSP);

  if (err)
	{
	  if (outFSP != 0)
		unlink(OutNameP);
	  printf("\nExiting with error %d", err);
	  if (errMsgP)
		printf("%s", errMsgP);
	  printf("\n");
	}

  return err;
}
