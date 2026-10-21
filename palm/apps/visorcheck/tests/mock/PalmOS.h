#ifndef MOCK_PALMOS_H
#define MOCK_PALMOS_H
#include <stdint.h>
#include <stddef.h>
typedef uint32_t UInt32;
typedef uint16_t UInt16;
typedef int16_t Int16;
typedef int Err;
typedef unsigned long LocalID;
typedef void *MemPtr;
typedef unsigned char Boolean;
struct handle { unsigned char *bytes; unsigned long size; };
typedef struct handle *MemHandle;
struct database;
typedef struct database *DmOpenRef;
#define true 1
#define false 0
#define noPreferenceFound (-1)
#define dmMaxRecordIndex 65535
#define dmModeReadOnly 1
#define dmModeReadWrite 3
LocalID DmFindDatabase(UInt16,const char *);
Err DmCreateDatabase(UInt16,const char *,UInt32,UInt32,Boolean);
Err DmDatabaseInfo(UInt16,LocalID,char *,UInt16 *,UInt16 *,UInt32 *,UInt32 *,UInt32 *,UInt32 *,LocalID *,LocalID *,UInt32 *,UInt32 *);
DmOpenRef DmOpenDatabase(UInt16,LocalID,UInt16);
Err DmCloseDatabase(DmOpenRef);
UInt16 DmNumRecords(DmOpenRef);
MemHandle DmQueryRecord(DmOpenRef,UInt16);
MemHandle DmNewRecord(DmOpenRef,UInt16 *,UInt32);
Err DmReleaseRecord(DmOpenRef,UInt16,Boolean);
Err DmRemoveRecord(DmOpenRef,UInt16);
Err DmGetLastErr(void);
Err DmWrite(void *,UInt32,const void *,UInt32);
MemPtr MemPtrNew(UInt32);
Err MemPtrFree(MemPtr);
MemPtr MemHandleLock(MemHandle);
Err MemHandleUnlock(MemHandle);
UInt32 MemHandleSize(MemHandle);
Int16 PrefGetAppPreferences(UInt32,UInt16,void *,UInt16 *,Boolean);
void PrefSetAppPreferences(UInt32,UInt16,Int16,const void *,UInt16,Boolean);
#endif
