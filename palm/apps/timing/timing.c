/* Prism-only RAM diagnostic. No ROM image, firmware, reset or CS1 changes. */
#include <PalmOS.h>
#include "core.h"
#include "../visorbus/hs_card.h"
#define CREATOR 'FNTM'
#define DBNAME "FN Timing P3 Log"
extern UInt32 vt_pc(void);
extern void vt_loads(const void *,UInt16 *,UInt32 *);
static struct vt_record last;
static Boolean baseline_ok;
static const Char *message="Read-only query";
static UInt32 be32(const UInt8 *p){return ((UInt32)p[0]<<24)|((UInt32)p[1]<<16)|((UInt32)p[2]<<8)|p[3];}
static int receipt(struct vt_record *r){UInt8 b[128];UInt16 n=128;Int16 ver;
 ver=PrefGetAppPreferences('FNRA',1,b,&n,true);
 if(ver!=1||n!=128||be32(b)!=0x46525032UL||be32(b+4)!=1||!be32(b+96)||be32(b+24)||be32(b+104)!=7)return VT_IDENTITY;
 r->v[V_CARD]=be32(b+16);r->v[V_GEN]=be32(b+12);r->v[V_SOURCE]=be32(b+28);r->v[V_RUNTIME]=be32(b+32);
 return !r->v[V_CARD]||r->v[V_CARD]>3?VT_IDENTITY:0;
}
static int guard(void *ctx,const struct vt_record *r){struct vt_record now;UInt8 b=0;UInt16 a;UInt32 base=0,size=0;int e;(void)ctx;
 vt_init(&now);e=receipt(&now);if(e)return e;
 if(now.v[V_CARD]!=r->v[V_CARD]||now.v[V_GEN]!=r->v[V_GEN]||now.v[V_SOURCE]!=r->v[V_SOURCE]||now.v[V_RUNTIME]!=r->v[V_RUNTIME])return VT_IDENTITY;
 for(a=0;a<=2;++a){e=VisorCardAttrGet((UInt16)r->v[V_CARD],a,&b);if(e||!b)return e?e:VT_IDENTITY;}
 e=VisorCardAttrGet((UInt16)r->v[V_CARD],6,&b);if(e||b)return e?e:VT_IDENTITY;
 e=VisorCardAttrGet((UInt16)r->v[V_CARD],15,&b);if(e||!b)return e?e:VT_IDENTITY;
 e=VisorCardAttrGet((UInt16)r->v[V_CARD],3,&base);if(!e)e=VisorCardAttrGet((UInt16)r->v[V_CARD],4,&size);
 return e?e:(base!=r->v[V_BASE]||size!=r->v[V_SIZE])?VT_IDENTITY:0;
}
static int get_time(void *ctx,VTU32 *v){struct vt_record *r=ctx;return VisorCardAttrGet((UInt16)r->v[V_CARD],5,v);}
static int set_time(void *ctx,VTU32 v){struct vt_record *r=ctx;return VisorCardAttrSet((UInt16)r->v[V_CARD],5,&v);}
static int pending(struct vt_record *r){UInt8 b[VT_BYTES];UInt16 n=VT_BYTES;Int16 v=PrefGetAppPreferences(CREATOR,2,b,&n,true);if(v==noPreferenceFound)return 0;if(v!=1||vt_decode(r,b,n))return -1;return (r->v[V_FLAGS]&VT_PENDING)?1:0;}
static int journal(void *ctx,const struct vt_record *r,int is_pending){UInt8 b[VT_BYTES],verify[VT_BYTES];UInt16 n=VT_BYTES;(void)ctx;(void)is_pending;
 vt_encode(r,b);PrefSetAppPreferences(CREATOR,2,1,b,VT_BYTES,true);
 return PrefGetAppPreferences(CREATOR,2,verify,&n,true)!=1||n!=VT_BYTES||MemCmp(b,verify,n)?VT_LOG:0;
}
static int loads(void *ctx,VTU32 address,VTU32 *w0,VTU32 *w1,VTU32 *value){ErrExceptionType frame;MemPtr *volatile list;volatile int error=0;UInt16 words[2]={0,0};UInt32 l=0;(void)ctx;
 list=VisorCardCatchListP();if(!list)return VT_BAD;frame.err=0;frame.nextP=(ErrExceptionPtr)*list;*list=(MemPtr)&frame;
 if(ErrSetJump(frame.state)==0)vt_loads((const void *)address,words,&l);else error=VT_BAD;
 *list=(MemPtr)frame.nextP;if(!error){*w0=words[0];*w1=words[1];*value=l;}return error;
}
static int ram(struct vt_record *r){UInt16 card=0,index,k;LocalID id=0;UInt32 type=0,creator=0;DmOpenRef db;MemHandle h;MemPtr p;int good=0;
 if(SysCurAppDatabase(&card,&id)||card)return VT_BAD;
 if(DmDatabaseInfo(card,id,0,0,0,0,0,0,0,0,0,&type,&creator)||type!='appl'||creator!=CREATOR)return VT_BAD;
 db=DmOpenDatabase(0,id,dmModeReadOnly);if(!db)return VT_BAD;index=DmFindResource(db,'code',1,0);h=index==0xffff?0:DmGetResourceIndex(db,index);
 p=h?MemHandleLock(h):0;
 if(p){r->v[V_PC]=vt_pc();r->v[V_CODEBASE]=(UInt32)p;r->v[V_CODESIZE]=MemHandleSize(h);r->v[V_HEAP]=MemHandleHeapID(h);r->v[V_HEAPFLAGS]=MemHeapFlags((UInt16)r->v[V_HEAP]);r->v[V_CURDB]=id;
 r->v[V_R0]=vt_crc(p,(unsigned)r->v[V_CODESIZE]);
 for(k=0;k<MemNumRAMHeaps(0);++k)if(MemHeapID(0,k)==r->v[V_HEAP])r->v[V_RAM]=1;
 good=r->v[V_RAM]&&MemHandleCardNo(h)==0&&!(r->v[V_HEAPFLAGS]&1)&&r->v[V_PC]>=r->v[V_CODEBASE]&&r->v[V_PC]-r->v[V_CODEBASE]<r->v[V_CODESIZE];MemHandleUnlock(h);}
 if(h)DmReleaseResource(h);DmCloseDatabase(db);if(good)r->v[V_FLAGS]|=VT_RAM;return good?0:VT_BAD;
}
static int snapshot(struct vt_record *r){UInt32 base=0,size=0;int e;vt_init(r);
 e=ram(r);if(!e)e=FtrGet('hsEx',0,&r->v[V_HSVER]);if(!e)e=receipt(r);
 FtrGet(sysFtrCreator,sysFtrNumROMVersion,&r->v[V_ROMVER]);FtrGet(sysFtrCreator,sysFtrNumProcessorID,&r->v[V_CPU]);
 FtrGet(sysFtrCreator,sysFtrNumOEMCompanyID,&r->v[V_OEMCO]);FtrGet(sysFtrCreator,sysFtrNumOEMDeviceID,&r->v[V_OEMDEV]);
 if(!e)e=VisorCardAttrGet((UInt16)r->v[V_CARD],3,&base);if(!e)e=VisorCardAttrGet((UInt16)r->v[V_CARD],4,&size);
 r->v[V_BASE]=base;r->v[V_SIZE]=size;
 if(!e&&(!base||(base&1)||size<0x20000UL||size>0x1000000UL||(size&(size-1))||base>0xffffffffUL-size))e=VT_BAD;
 if(!e)e=guard(0,r);if(!e)e=get_time(r,&r->v[V_ORIG]);r->v[V_APPLIED]=r->v[V_ORIG];r->v[V_ERROR]=e;return e;
}
static int append(struct vt_record *r){LocalID id;DmOpenRef db;UInt16 index,attrs=0;UInt32 type=0,creator=0;MemHandle h;MemPtr p;Err e;UInt8 b[VT_BYTES];
 r->v[V_SECONDS]=TimGetSeconds();id=DmFindDatabase(0,DBNAME);
 if(!id){e=DmCreateDatabase(0,DBNAME,CREATOR,'DATA',false);if(e)return e;id=DmFindDatabase(0,DBNAME);}
 e=DmDatabaseInfo(0,id,0,&attrs,0,0,0,0,0,0,0,&type,&creator);if(e||type!='DATA'||creator!=CREATOR||(attrs&1))return VT_LOG;
 db=DmOpenDatabase(0,id,dmModeReadWrite);if(!db)return VT_LOG;index=DmNumRecords(db);if(index>=32){DmCloseDatabase(db);return VT_LOG;}r->v[V_SEQ]=index+1;vt_encode(r,b);
 h=DmNewRecord(db,&index,VT_BYTES);p=h?MemHandleLock(h):0;if(!p){if(h)DmReleaseRecord(db,index,false);DmCloseDatabase(db);return VT_LOG;}
 e=DmWrite(p,0,b,VT_BYTES);MemHandleUnlock(h);{Err re=DmReleaseRecord(db,index,e==0);if(!e)e=re;}if(e)DmRemoveRecord(db,index);DmCloseDatabase(db);return e;
}
static void hex(Coord y,const Char *label,UInt32 value){Char s[48],n[10];StrCopy(s,label);StrIToH(n,value);StrCat(s,n);WinDrawChars(s,StrLen(s),2,y);}
static void draw(void){RectangleType a;RctSetRectangle(&a,0,18,160,112);WinEraseRectangle(&a,0);WinDrawChars(message,StrLen(message),2,19);
 hex(32,"API original ",last.v[V_ORIG]);hex(43,"Requested ",last.v[V_REQUEST]);hex(54,"Applied ",last.v[V_APPLIED]);hex(65,"Restored ",last.v[V_RESTORED]);
 hex(76,"Words ",(last.v[V_W0]<<16)|last.v[V_W1]);hex(87,"MOVE.L ",last.v[V_LONG]);hex(98,"Error ",last.v[V_ERROR]);hex(109,"Restore err ",last.v[V_RESTOREERR]);hex(120,"Flags ",last.v[V_FLAGS]);}
static int capture_ready(void *ctx,const struct vt_record *r){(void)ctx;(void)r;return FrmAlert(1100)==1?0:VT_BAD;}
static Boolean prior_baseline(const struct vt_record *now){LocalID id=DmFindDatabase(0,DBNAME);DmOpenRef db;UInt16 i,attr=0;UInt32 type=0,creator=0;Boolean ok=false;if(!id)return false;
 if(DmDatabaseInfo(0,id,0,&attr,0,0,0,0,0,0,0,&type,&creator)||type!='DATA'||creator!=CREATOR||(attr&1))return false;
 db=DmOpenDatabase(0,id,dmModeReadOnly);if(!db)return false;
 for(i=0;i<DmNumRecords(db);++i){MemHandle h=DmQueryRecord(db,i);UInt8 *p=h?MemHandleLock(h):0;struct vt_record r;
 if(p&&!vt_decode(&r,p,(unsigned)MemHandleSize(h))&&r.v[V_EVENT]==1&&r.v[V_STAGE]==7&&r.v[V_ERROR]==0&&r.v[V_FLAGS]==14&&r.v[V_ORIG]==330&&r.v[V_RESTORED]==330&&r.v[V_W0]==0xfeed&&r.v[V_W1]==0xbeef&&r.v[V_LONG]==0xfeedbeefUL&&r.v[V_R0]==0xea66e7f3UL&&r.v[V_CARD]==now->v[V_CARD]&&r.v[V_GEN]==now->v[V_GEN]&&r.v[V_SOURCE]==now->v[V_SOURCE]&&r.v[V_RUNTIME]==now->v[V_RUNTIME]&&r.v[V_BASE]==now->v[V_BASE]&&r.v[V_SIZE]==now->v[V_SIZE])ok=true;
 if(p)MemHandleUnlock(h);}
 DmCloseDatabase(db);return ok;
}
static void run(int slower){struct vt_record saved;struct vt_ops o={guard,get_time,set_time,journal,loads,&last,capture_ready};int e;
 if(pending(&saved)!=0){message="Restore pending; blocked";draw();return;}
 if(slower&&!baseline_ok){message="Baseline first";draw();return;}
 e=snapshot(&last);if(!e)e=vt_run(&last,&o,slower);
 if(last.v[V_FLAGS]&VT_PENDING)journal(&last,&last,1);
 last.v[V_ERROR]=e;{int log=append(&last);if(log){last.v[V_LOGERR]=log;last.v[V_ERROR]=log;}}
 if(!slower)baseline_ok=last.v[V_ERROR]==0;
 message=last.v[V_ERROR]?"FAIL; keep screen":"PASS / setting restored";draw();
}
UInt32 PilotMain(UInt16 cmd,MemPtr param,UInt16 flags){EventType e;UInt16 error;struct vt_record saved;(void)param;(void)flags;if(cmd!=sysAppLaunchCmdNormalLaunch)return 0;
 FrmGotoForm(1000);do{EvtGetEvent(&e,evtWaitForever);if(!SysHandleEvent(&e)&&!MenuHandleEvent(0,&e,&error)){
 if(e.eType==frmLoadEvent)FrmSetActiveForm(FrmInitForm(e.data.frmLoad.formID));
 else if(e.eType==frmOpenEvent){FrmDrawForm(FrmGetActiveForm());snapshot(&last);baseline_ok=prior_baseline(&last);last.v[V_EVENT]=0;last.v[V_LOGERR]=append(&last);message=pending(&saved)?"Pending restore; blocked":"Prism only; idle / query";draw();}
 else if(e.eType==ctlSelectEvent&&e.data.ctlSelect.controlID==1001)run(0);
 else if(e.eType==ctlSelectEvent&&e.data.ctlSelect.controlID==1002)run(1);
 else if(e.eType==ctlSelectEvent&&e.data.ctlSelect.controlID==1003){int n=pending(&saved);if(n==1){struct vt_ops o={guard,get_time,set_time,journal,loads,&saved,0};saved.v[V_EVENT]=3;saved.v[V_ERROR]=vt_restore(&saved,&o);last=saved;append(&last);message=last.v[V_ERROR]?"Restore blocked / failed":"Restore verified";}else message=n?"Invalid journal; blocked":"No pending restore";draw();}
 else FrmHandleEvent(FrmGetActiveForm(),&e);
 }}while(e.eType!=appStopEvent);FrmCloseAllForms();return 0;}
