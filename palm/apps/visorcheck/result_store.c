#include "result_store.h"
#include <string.h>
enum { storeBadConfig=0x8301, storeSequence=0x8302, storeMemory=0x8303,
       storeEncode=0x8304, storeWrongDatabase=0x8305 };
static Err check_database(LocalID id, UInt32 wanted_type) {
  UInt32 type=0, creator=0;
  Err error=DmDatabaseInfo(0,id,0,0,0,0,0,0,0,0,0,&type,&creator);
  return error ? error : (type!=wanted_type || creator!='FNVC') ? storeWrongDatabase : 0;
}
Err vc_store_append(const struct vc_result *r, const unsigned char *actual,
                    const unsigned char *expected) {
  LocalID id=DmFindDatabase(0,"VisorCheckResults");
  DmOpenRef db; MemHandle h; MemPtr ptr;
  UInt16 index=dmMaxRecordIndex; Err error;
  unsigned long capacity=VC_HEADER+r->completed*5+4, length;
  unsigned char *bytes;
  if(!id) {
    error=DmCreateDatabase(0,"VisorCheckResults",'FNVC','VRSL',false);
    if(error) return error;
    id=DmFindDatabase(0,"VisorCheckResults");
  }
  error=check_database(id,'VRSL');
  if(error) return error;
  db=DmOpenDatabase(0,id,dmModeReadWrite);
  if(!db) return DmGetLastErr();
  bytes=MemPtrNew(capacity);
  if(!bytes) { DmCloseDatabase(db); return storeMemory; }
  length=vc_encode(bytes,capacity,r,actual,expected);
  if(!length) { MemPtrFree(bytes); DmCloseDatabase(db); return storeEncode; }
  h=DmNewRecord(db,&index,length);
  if(!h) { error=DmGetLastErr(); MemPtrFree(bytes); DmCloseDatabase(db); return error; }
  ptr=MemHandleLock(h);
  if(!ptr) error=storeMemory;
  else { error=DmWrite(ptr,0,bytes,length); MemHandleUnlock(h); }
  /* A failed/partial record is removed, leaving the earlier STARTED marker.
   * Never remove a previously completed run or its STARTED marker. */
  { Err release_error=DmReleaseRecord(db,index,!error); if(!error) error=release_error; }
  if(error) DmRemoveRecord(db,index);
  MemPtrFree(bytes);
  { Err close_error=DmCloseDatabase(db); if(!error) error=close_error; }
  return error;
}
Err vc_store_begin_reference(struct vc_result *r, unsigned long reference_size,
                             unsigned long reference_crc) {
  LocalID id=DmFindDatabase(0,"VisorCheckControl");
  DmOpenRef db; MemHandle h; const unsigned char *bytes;
  unsigned char saved[20], verify[20];
  UInt16 size=sizeof(saved); Int16 version; UInt32 sequence;
  if(!id) return storeBadConfig;
  { Err error=check_database(id,'VCTL'); if(error) return error; }
  db=DmOpenDatabase(0,id,dmModeReadOnly);
  if(!db) return DmGetLastErr();
  if(DmNumRecords(db)!=1) { DmCloseDatabase(db); return storeBadConfig; }
  h=DmQueryRecord(db,0);
  if(!h) { DmCloseDatabase(db); return storeBadConfig; }
  bytes=MemHandleLock(h);
  if(!bytes || !vc_config(bytes,MemHandleSize(h),r->session,r->sha,
                         reference_size,reference_crc)) {
    if(bytes) MemHandleUnlock(h);
    DmCloseDatabase(db); return storeBadConfig;
  }
  MemHandleUnlock(h); DmCloseDatabase(db);
  version=PrefGetAppPreferences('FNVC',2,saved,&size,true);
  if(version!=noPreferenceFound && (version!=1 || size!=sizeof(saved))) return storeSequence;
  sequence=(version==1 && !memcmp(saved,r->session,16)) ? vc_get32(saved+16) : 0;
  if(sequence==0xffffffffUL) return storeSequence;
  memcpy(saved,r->session,16); vc_put32(saved+16,sequence+1);
  PrefSetAppPreferences('FNVC',2,1,saved,sizeof(saved),true);
  size=sizeof(verify);
  if(PrefGetAppPreferences('FNVC',2,verify,&size,true)!=1 ||
      size!=sizeof(verify) || memcmp(saved,verify,sizeof(saved))) return storeSequence;
  r->sequence=sequence+1;
  return vc_store_append(r,0,0);
}

Err vc_store_begin(struct vc_result *r) {
  return vc_store_begin_reference(r, r->expected_size, r->expected_crc);
}
