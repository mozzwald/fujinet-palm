/* Setup install/remove use stack locals only. No retained callbacks or globals.
 * Ordinary execution is permitted only from a verified card0 appl/FNRA clone. */
#include <PalmOS.h>
#include "policy.h"
#include "hs_setup.h"
#define RP_DB_NAME "FN CS1 B Install Log"
#define RP_PREF 1
extern UInt32 fnram_pc(void);
extern UInt32 fnram_loop(void);
extern void fnram_loop_end(void);

static RPU32 rp_crc(const UInt8 *p,UInt32 n) {
 UInt32 crc=0xffffffffUL,i;UInt16 b;
 for(i=0;i<n;++i){crc^=p[i];for(b=0;b<8;++b)crc=(crc>>1)^((crc&1)?0xedb88320UL:0);}
 return ~crc;
}
static int rp_read_receipt(struct rp_record *r) {
 UInt8 data[RP_FIELDS*4];UInt16 n=sizeof(data);Int16 ver;
 ver=PrefGetAppPreferences(RP_CREATOR,RP_PREF,data,&n,true);
 if(ver!=RP_VERSION)return RP_BAD_RECEIPT;
 return rp_decode(r,data,n);
}
static int rp_save(void *ctx,const struct rp_record *record) {
 struct rp_record r;UInt8 bytes[RP_FIELDS*4],verify[RP_FIELDS*4];
 LocalID id;DmOpenRef db;MemHandle h;MemPtr p;UInt16 index,n;Err e;
 (void)ctx;r=*record;r.v[RP_SECONDS]=TimGetSeconds();rp_encode(&r,bytes);
 id=DmFindDatabase(0,RP_DB_NAME);
 if(!id){e=DmCreateDatabase(0,RP_DB_NAME,RP_CREATOR,'DATA',false);if(e)return (UInt16)e;id=DmFindDatabase(0,RP_DB_NAME);}
 /* Never append to an unrelated same-name database. */
 {UInt32 type=0,creator=0;UInt16 attrs=0;e=DmDatabaseInfo(0,id,0,&attrs,0,0,0,0,0,0,0,&type,&creator);
 if(e||(attrs&dmHdrAttrResDB)||type!='DATA'||creator!=RP_CREATOR)return RP_COLLISION;}
 db=DmOpenDatabase(0,id,dmModeReadWrite);if(!db)return (UInt16)DmGetLastErr();
 if(DmNumRecords(db)>=256){DmCloseDatabase(db);return RP_LOG_ERROR;}
 index=dmMaxRecordIndex;h=DmNewRecord(db,&index,sizeof(bytes));
 if(!h){e=DmGetLastErr();DmCloseDatabase(db);return (UInt16)e;}
 p=MemHandleLock(h);if(!p){DmReleaseRecord(db,index,false);DmCloseDatabase(db);return RP_LOG_ERROR;}
 e=DmWrite(p,0,bytes,sizeof(bytes));MemHandleUnlock(h);
 {Err release=DmReleaseRecord(db,index,e==0);if(!e)e=release;}
 if(e)DmRemoveRecord(db,index);
 if(!e){UInt16 attrs=0;DmDatabaseInfo(0,id,0,&attrs,0,0,0,0,0,0,0,0,0);
 attrs|=dmHdrAttrBackup;e=DmSetDatabaseInfo(0,id,0,&attrs,0,0,0,0,0,0,0,0,0);}
 DmCloseDatabase(db);if(e)return (UInt16)e;
 /* A failed PDB append cannot publish a success preference. */
 PrefSetAppPreferences(RP_CREATOR,RP_PREF,RP_VERSION,bytes,sizeof(bytes),true);
 n=sizeof(verify);if(PrefGetAppPreferences(RP_CREATOR,RP_PREF,verify,&n,true)!=RP_VERSION||
 n!=sizeof(verify)||MemCmp(bytes,verify,n))return RP_LOG_ERROR;
 return 0;
}
static int rp_inspect(void *ctx,RPU32 id,struct rp_info *i) {
 UInt16 attrs=0,version=0,index,openCount=0,k,count;UInt32 type=0,creator=0;
 DmOpenRef db;MemHandle h;MemPtr p;Err e;(void)ctx;MemSet(i,sizeof(*i),0);
 if(!id)return RP_BAD_SOURCE;
 e=DmDatabaseInfo(0,(LocalID)id,i->name,&attrs,&version,0,0,0,0,0,0,&type,&creator);
 if(e)return (UInt16)e;
 i->id=id;i->card=0;i->attrs=attrs;i->version=version;i->type=type;i->creator=creator;
 db=DmOpenDatabase(0,(LocalID)id,dmModeReadOnly);if(!db)return (UInt16)DmGetLastErr();
 e=DmOpenDatabaseInfo(db,0,&openCount,0,0,0);if(e){DmCloseDatabase(db);return (UInt16)e;}
 i->busy=openCount>1;
 index=DmFindResource(db,'code',1,0);if(index==0xffff){DmCloseDatabase(db);return RP_BAD_SOURCE;}
 h=DmGetResourceIndex(db,index);if(!h){DmCloseDatabase(db);return RP_BAD_SOURCE;}
 p=MemHandleLock(h);if(!p){DmReleaseResource(h);DmCloseDatabase(db);return RP_BAD_SOURCE;}
 i->base=(UInt32)p;i->size=MemHandleSize(h);i->heap=MemHandleHeapID(h);
 i->heapflags=MemHeapFlags((UInt16)i->heap);i->crc=rp_crc(p,i->size);
 count=MemNumRAMHeaps(0);for(k=0;k<count;++k)if(MemHeapID(0,k)==i->heap)i->ram=1;
 if(MemHandleCardNo(h)!=0)i->ram=0;
 MemHandleUnlock(h);DmReleaseResource(h);DmCloseDatabase(db);return 0;
}
static RPU32 rp_find(void *ctx,const char *name) {(void)ctx;return DmFindDatabase(0,name);}
static int rp_copy(void *ctx,RPU32 source,RPU32 *dest) {
 LocalID id=0;Err e;(void)ctx;
 e=RPDatabaseCopy(0,(LocalID)source,0,RP_TEMP_NAME,0,0,&id);*dest=id;return (UInt16)e;
}
static int rp_publish(void *ctx,RPU32 id) {
 UInt16 attrs=dmHdrAttrResDB|dmHdrAttrBackup;UInt32 type=RP_APP_TYPE,creator=RP_CREATOR;
 (void)ctx;
 return (UInt16)DmSetDatabaseInfo(0,(LocalID)id,RP_RUNTIME_NAME,&attrs,0,0,0,0,0,0,0,&type,&creator);
}
static int rp_erase(void *ctx,RPU32 id) {(void)ctx;return (UInt16)DmDeleteDatabase(0,(LocalID)id);}
static void rp_ops_init(struct rp_ops *o) {
 o->inspect=rp_inspect;o->find=rp_find;o->copy=rp_copy;o->publish=rp_publish;
 o->erase=rp_erase;o->save=rp_save;o->ctx=0;
}
static UInt32 rp_setup_install(const RPInstallParams *p,UInt16 flags) {
 struct rp_record r,old;struct rp_ops o;UInt32 generation=1;
 if(!p||p->setupCardNo!=0||!p->cardNo)return RP_BAD_SOURCE;
 if(!rp_read_receipt(&old))generation=old.v[RP_GENERATION]+1;
 if(!generation)generation=1;
 rp_clear(&r);r.v[RP_GENERATION]=generation;r.v[RP_MODULE]=p->cardNo;
 r.v[RP_RESET]=p->isReset;r.v[RP_LAUNCH_FLAGS]=flags;rp_ops_init(&o);
 return rp_install(&o,&r,p->setupDbID,fnram_pc());
}
static UInt32 rp_setup_remove(const RPRemoveParams *p) {
 struct rp_record r;struct rp_info i;struct rp_ops o;int e;
 if(!p)return RP_BAD_SOURCE;e=rp_read_receipt(&r);if(e)return e;
 rp_ops_init(&o);e=rp_inspect(0,r.v[RP_SOURCE_DB],&i);if(e)return e;
 /* Runtime clone shares the code but its PC is in a different resource. */
 if(i.type!=RP_SETUP_TYPE||i.creator!=RP_SETUP_CREATOR||!rp_in_ram(&i,fnram_pc()))return RP_BAD_SOURCE;
 return rp_remove(&o,&r,p->cardNo);
}
extern UInt32 va_runtime(UInt16,MemPtr,UInt16);
UInt32 PilotMain(UInt16 cmd,MemPtr param,UInt16 flags) {
 if(cmd==0x8000)return rp_setup_install((const RPInstallParams *)param,flags);
 if(cmd==0x8001)return rp_setup_remove((const RPRemoveParams *)param);
 if(cmd==sysAppLaunchCmdNormalLaunch){
  UInt16 card=0;LocalID id=0;struct rp_info info;
  if(SysCurAppDatabase(&card,&id)||card||rp_inspect(0,id,&info)||
     info.type!=RP_APP_TYPE||info.creator!=RP_CREATOR||!rp_in_ram(&info,fnram_pc()))return RP_BAD_RUNTIME;
  return va_runtime(cmd,param,flags);
 }
 return 0;
}
