#include "app_identity.h"
#include "result_store.h"
#include "image_read.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct database { struct handle records[32]; unsigned count; UInt32 type,creator; };
static struct database config_db, result_db;
static int results_exist, fail_alloc, fail_record, fail_write, fail_pref, fail_release;
static unsigned char prefs[20];static int have_pref;
LocalID DmFindDatabase(UInt16 card,const char *name) {
  assert(card==0);
  if(!strcmp(name,VC_CONTROL_DB))return 1;
  assert(!strcmp(name,VC_RESULTS_DB));return results_exist ? 2:0;
}
Err DmCreateDatabase(UInt16 card,const char *name,UInt32 creator,UInt32 type,Boolean resource) {
  assert(card==0 && !resource && !strcmp(name,VC_RESULTS_DB));
  results_exist=1;result_db.creator=creator;result_db.type=type;return 0;
}
Err DmDatabaseInfo(UInt16 card,LocalID id,char *a,UInt16 *b,UInt16 *c,UInt32 *d,UInt32 *e,UInt32 *f,UInt32 *g,LocalID *h,LocalID *i,UInt32 *type,UInt32 *creator) {
  struct database *db=id==1?&config_db:&result_db;
  (void)card;(void)a;(void)b;(void)c;(void)d;(void)e;(void)f;(void)g;(void)h;(void)i;
  *type=db->type;*creator=db->creator;return 0;
}
DmOpenRef DmOpenDatabase(UInt16 card,LocalID id,UInt16 mode) {(void)card;(void)mode;return id==1?&config_db:&result_db;}
Err DmCloseDatabase(DmOpenRef db) {(void)db;return 0;}
UInt16 DmNumRecords(DmOpenRef db) {return (UInt16)db->count;}
MemHandle DmQueryRecord(DmOpenRef db,UInt16 index) {return index<db->count?&db->records[index]:0;}
MemHandle DmNewRecord(DmOpenRef db,UInt16 *index,UInt32 size) {
  struct handle *h;
  if(fail_record)return 0;
  assert(db==&result_db && db->count<32);
  *index=(UInt16)db->count++;h=&db->records[*index];h->size=size;h->bytes=calloc(1,size);assert(h->bytes);return h;
}
Err DmReleaseRecord(DmOpenRef db,UInt16 index,Boolean dirty) {(void)db;(void)index;(void)dirty;return fail_release?97:0;}
Err DmRemoveRecord(DmOpenRef db,UInt16 index) {
  assert(index==db->count-1);free(db->records[index].bytes);--db->count;return 0;
}
Err DmGetLastErr(void) {return 99;}
Err DmWrite(void *p,UInt32 offset,const void *src,UInt32 size) {
  if(fail_write){memcpy((char *)p+offset,src,size/2);return 98;}
  memcpy((char *)p+offset,src,size);return 0;
}
MemPtr MemPtrNew(UInt32 size){return fail_alloc?0:malloc(size);}
Err MemPtrFree(MemPtr p){free(p);return 0;}
MemPtr MemHandleLock(MemHandle h){return h->bytes;}
Err MemHandleUnlock(MemHandle h){(void)h;return 0;}
UInt32 MemHandleSize(MemHandle h){return (UInt32)h->size;}
Int16 PrefGetAppPreferences(UInt32 creator,UInt16 id,void *p,UInt16 *size,Boolean saved) {
  assert(creator==VC_CREATOR && id==2 && saved);
  if(!have_pref)return noPreferenceFound;
  assert(*size>=20);memcpy(p,prefs,20);*size=20;return 1;
}
void PrefSetAppPreferences(UInt32 creator,UInt16 id,Int16 version,const void *p,UInt16 size,Boolean saved) {
  assert(creator==VC_CREATOR && id==2 && saved && version==1 && size==20);
  if(!fail_pref){memcpy(prefs,p,20);have_pref=1;}
}
static void initialize(struct vc_result *r) {
  memset(r,0,sizeof(*r));r->expected_size=16;r->expected_crc=0xecbb4b55UL;
  r->reference_crc=r->expected_crc;r->batch_words=1;r->kind=VC_SYNTH_PASS;r->started=1;
}
int main(void) {
  unsigned char config[68]={0},actual[16]={0},snapshot[VC_HEADER+4];
  struct vc_result r;
  config_db.type='VCTL';config_db.creator=VC_CREATOR;config_db.count=1;
  config_db.records[0].bytes=config;config_db.records[0].size=68;
  memcpy(config,"VCC1",4);vc_put32(config+4,1);config[8]=1;
  vc_put32(config+24,16);vc_put32(config+28,0xecbb4b55UL);
  vc_put32(config+64,visor_image_local_crc(config,64));
  initialize(&r);assert(!vc_store_begin(&r));assert(r.sequence==1 && result_db.count==1);
  memcpy(snapshot,result_db.records[0].bytes,sizeof(snapshot));
  r.status=VC_PASS;r.completed=16;r.crc=r.expected_crc;r.ended=2;
  fail_record=1;assert(vc_store_append(&r,actual,actual));fail_record=0;
  assert(result_db.count==1 && !memcmp(snapshot,result_db.records[0].bytes,sizeof(snapshot)));
  fail_write=1;assert(vc_store_append(&r,actual,actual));fail_write=0;
  assert(result_db.count==1 && !memcmp(snapshot,result_db.records[0].bytes,sizeof(snapshot)));
  fail_release=1;assert(vc_store_append(&r,actual,actual));fail_release=0;
  assert(result_db.count==1);
  fail_alloc=1;assert(vc_store_append(&r,actual,actual));fail_alloc=0;
  assert(result_db.count==1);
  assert(!vc_store_append(&r,actual,actual));assert(result_db.count==2);
  assert(!memcmp(snapshot,result_db.records[0].bytes,sizeof(snapshot)));
  /* Simulated app exit/crash and relaunch: append, never overwrite prior runs. */
  initialize(&r);assert(!vc_store_begin(&r));assert(r.sequence==2 && result_db.count==3);
  initialize(&r);assert(!vc_store_begin(&r));assert(r.sequence==3 && result_db.count==4);
  fail_pref=1;initialize(&r);assert(vc_store_begin(&r));fail_pref=0;
  assert(result_db.count==4);
  config[64]^=1;initialize(&r);assert(vc_store_begin(&r));config[64]^=1;
  assert(result_db.count==4);
  config_db.type='WRNG';initialize(&r);assert(vc_store_begin(&r));config_db.type='VCTL';
  result_db.creator='WRNG';initialize(&r);assert(vc_store_begin(&r));result_db.creator=VC_CREATOR;
  assert(result_db.count==4);
  /* New pattern metadata must not replace the original control identity or
   * reset the existing session sequence. A wrong reference fails closed. */
  initialize(&r);r.kind=VC_CONST_ONES;r.expected_size=8;r.expected_crc=0x12345678;
  assert(vc_store_begin_reference(&r,16,0));
  assert(!vc_store_begin_reference(&r,16,0xecbb4b55UL));
  assert(r.sequence==5 && result_db.count==5);
  assert(vc_get32(result_db.records[4].bytes+68)==8);
  assert(vc_get32(result_db.records[4].bytes+72)==0x12345678);
  assert(!memcmp(snapshot,result_db.records[0].bytes,sizeof(snapshot)));
  while(result_db.count)DmRemoveRecord(&result_db,(UInt16)(result_db.count-1));
  puts("Palm storage mock: append-only runs, crash markers, low memory, failed/partial writes, journal and ownership failures PASS");return 0;
}
