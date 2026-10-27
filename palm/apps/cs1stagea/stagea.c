/* RAM-only Stage A diagnostic. No timing setter, reset or ROM execution. */
#include <PalmOS.h>
#include "../visorbus/hs_card.h"
#include "visor_mailbox.h"
#ifdef VA_MODULE_INSTALL
#define CREATOR 'FNMC'
#define LOGNAME "FN CS1 Module B Log"
#define RECEIPT_CREATOR 'FNMC'
#else
#define CREATOR 'FNMA'
#define LOGNAME "FN CS1 Stage A Log"
#define RECEIPT_CREATOR 'FNRA'
#endif
enum { VA_RESULT_WORDS=32, VA_RESULT_BYTES=128 };
typedef UInt32 VAResult[VA_RESULT_WORDS];
typedef char va_result_width[(sizeof(VAResult)==VA_RESULT_BYTES)?1:-1];
extern UInt32 va_pc(void);
extern UInt16 va_read(UInt32,UInt16);
extern void va_write(UInt32,UInt16,UInt16);
static UInt32 be32(const UInt8 *p){return ((UInt32)p[0]<<24)|((UInt32)p[1]<<16)|((UInt32)p[2]<<8)|p[3];}
static UInt32 crc(const UInt8 *p,UInt32 n){UInt32 c=0xffffffffUL;UInt16 k;while(n--){c^=*p++;for(k=0;k<8;++k)c=(c>>1)^((c&1)?0xedb88320UL:0);}return ~c;}
static Err identity(UInt16 *card,UInt32 *generation){UInt8 b[128];UInt16 n=128;
 if(PrefGetAppPreferences(RECEIPT_CREATOR,1,b,&n,true)!=1||n!=128||be32(b)!=0x46525032UL||be32(b+4)!=1||!be32(b+96)||be32(b+24)||be32(b+104)!=7)return 1;
 *card=(UInt16)be32(b+16);*generation=be32(b+12);return !*card||*card>3?1:0;
}
static Err guard(UInt32 *r){UInt16 card=0,a;UInt32 gen=0,base=0,size=0;UInt8 present;Err e=identity(&card,&gen);if(e)return e;
 if(card!=r[4]||gen!=r[5]){r[29]=0x20000000UL;return 2;}
 for(a=0;a<=2;++a){present=0;e=VisorCardAttrGet(card,a,&present);if(e||!present){r[29]=(e?0x80000000UL:0x40000000UL)|((UInt32)a<<16)|(e?(UInt16)e:present);return e?e:2;}}
 present=1;e=VisorCardAttrGet(card,6,&present);if(e||present){r[29]=(e?0x80000000UL:0x40000000UL)|0x60000UL|(e?(UInt16)e:present);return e?e:2;}
 present=0;e=VisorCardAttrGet(card,15,&present);if(e||!present){r[29]=(e?0x80000000UL:0x40000000UL)|0xf0000UL|(e?(UInt16)e:present);return e?e:2;}
 a=3;e=VisorCardAttrGet(card,a,&base);if(!e){a=4;e=VisorCardAttrGet(card,a,&size);}
 if(e){r[29]=0x80000000UL|((UInt32)a<<16)|(UInt16)e;return e;}
 if(base!=r[6]||size!=r[7]){r[29]=0x10000000UL;return 2;}return 0;
}
static Err snapshot(UInt32 *r){UInt16 card=0,appcard=0,idx,k;UInt32 gen=0,type=0,creator=0;LocalID id=0;DmOpenRef db;MemHandle h;MemPtr p;Err e;
 MemSet(r,VA_RESULT_BYTES,0);r[0]=0x564d4131UL;r[1]=1;e=identity(&card,&gen);if(e)return e;r[4]=card;r[5]=gen;
 e=VisorCardAttrGet(card,3,&r[6]);if(!e)e=VisorCardAttrGet(card,4,&r[7]);if(!e)e=VisorCardAttrGet(card,5,&r[9]);if(e)return e;
 if(!r[6]||(r[6]&1)||r[7]<0x20000UL||r[7]>0x1000000UL||(r[7]&(r[7]-1))||r[6]>0xffffffffUL-r[7]||r[6]+r[7]>0xffffffffUL-0x2100UL||r[9]!=330)return 3;
 r[8]=r[6]+r[7];
 /* This candidate decodes physical A1..A23 directly. Require select-relative
  * offsets for the observed16MiB windows; other maps need explicit rebasing. */
 if(r[7]!=0x1000000UL||(r[6]&0xffffffUL)||(r[8]&0xffffffUL))return 3;
 e=guard(r);if(e)return e;
 if(SysCurAppDatabase(&appcard,&id)||appcard)return 4;
 if(DmDatabaseInfo(0,id,0,0,0,0,0,0,0,0,0,&type,&creator)||type!='appl'||creator!=CREATOR)return 4;
 db=DmOpenDatabase(0,id,dmModeReadOnly);if(!db)return 4;idx=DmFindResource(db,'code',1,0);h=idx==0xffff?0:DmGetResourceIndex(db,idx);p=h?MemHandleLock(h):0;
 if(p){r[10]=va_pc();r[11]=(UInt32)p;r[12]=MemHandleSize(h);r[13]=MemHandleHeapID(h);r[14]=MemHeapFlags((UInt16)r[13]);r[16]=id;r[17]=crc(p,r[12]);
 for(k=0;k<MemNumRAMHeaps(0);++k)if(MemHeapID(0,k)==r[13])r[15]=1;
 MemHandleUnlock(h);}if(h)DmReleaseResource(h);DmCloseDatabase(db);
 return !r[15]||(r[14]&1)||r[10]<r[11]||r[10]-r[11]>=r[12]?4:0;
}
static Err exercise(UInt32 *r){ErrExceptionType frame;MemPtr *volatile list;volatile Err e=0;UInt16 i,poll,token,ack;
 list=VisorCardCatchListP();if(!list)return 5;frame.err=0;frame.nextP=(ErrExceptionPtr)*list;*list=(MemPtr)&frame;
 if(ErrSetJump(frame.state)==0){
 r[18]=va_read(r[8],VMB_ID);r[19]=va_read(r[8],VMB_VERSION);r[20]=va_read(r[8],VMB_CAPABILITIES);r[21]=va_read(r[8],VMB_STATUS);r[22]=va_read(r[8],VMB_PROBE_ACK);
 if(r[18]!=VMB_ID_VALUE||r[19]!=VMB_ABI_VERSION||r[20]!=VMB_CAP_STAGE_A||r[21]!=VMB_ST_CAN_STAGE||r[22]>0xfffa)e=6;
 for(i=0;i<4&&!e;++i){
  if(guard(r)){e=2;break;}token=(UInt16)(r[22]+i+1);va_write(r[8],VMB_PROBE,token);
  for(poll=0;poll<64;++poll){ack=va_read(r[8],VMB_PROBE_ACK);++r[26];r[23]=ack;
   if(va_read(r[8],VMB_STATUS)&(VMB_ST_FAULT|VMB_ST_ERROR)){e=7;break;}
   if(ack==token)break;if(ack!=(UInt16)(token-1)){e=8;break;}SysTaskDelay(1);}
  if(!e&&poll==64)e=9;
  r[24]=token;
  if(!e&&(va_read(r[8],VMB_ID)!=VMB_ID_VALUE||va_read(r[8],VMB_VERSION)!=VMB_ABI_VERSION))e=6;
  if(!e){r[27]=va_read(r[6],8);r[28]=va_read(r[6],10);if(r[27]!=0xfeed||r[28]!=0xbeef)e=11;}
  if(!e)++r[25];
 }
 }else e=5;
 *list=(MemPtr)frame.nextP;if(!e)e=guard(r);return e;
}
static Err save(UInt32 *r){LocalID id;DmOpenRef db;UInt16 idx,attr=0;UInt32 type=0,creator=0;MemHandle h;MemPtr p;Err e;
 id=DmFindDatabase(0,LOGNAME);if(!id){e=DmCreateDatabase(0,LOGNAME,CREATOR,'DATA',false);if(e)return e;id=DmFindDatabase(0,LOGNAME);}
 e=DmDatabaseInfo(0,id,0,&attr,0,0,0,0,0,0,0,&type,&creator);if(e||type!='DATA'||creator!=CREATOR||(attr&1))return 10;
 db=DmOpenDatabase(0,id,dmModeReadWrite);if(!db)return 10;idx=DmNumRecords(db);if(idx>=32){DmCloseDatabase(db);return 10;}
 r[2]=idx+1;r[30]=TimGetSeconds();r[31]=crc((const UInt8 *)r,124);h=DmNewRecord(db,&idx,VA_RESULT_BYTES);p=h?MemHandleLock(h):0;
 if(!p){if(h)DmReleaseRecord(db,idx,false);DmCloseDatabase(db);return 10;}
 e=DmWrite(p,0,r,VA_RESULT_BYTES);MemHandleUnlock(h);{Err re=DmReleaseRecord(db,idx,e==0);if(!e)e=re;}if(e)DmRemoveRecord(db,idx);DmCloseDatabase(db);return e;
}
static void hex(Coord y,const Char *label,UInt32 v){Char b[48],n[10];StrCopy(b,label);StrIToH(n,v);StrCat(b,n);WinDrawChars(b,StrLen(b),2,y);}
static void draw(const UInt32 *r,const Char *message){RectangleType box;RctSetRectangle(&box,0,18,160,110);WinEraseRectangle(&box,0);WinDrawChars(message,StrLen(message),2,18);
 hex(32,"CS1 ",r[8]);hex(44,"ID ",r[18]);hex(56,"ABI ",r[19]);hex(68,"ACK ",r[23]);hex(80,"Pairs ",r[25]);hex(92,"Error ",r[3]);hex(104,"RAM PC ",r[10]);hex(116,"Guard ",r[29]);}
#ifdef VA_MODULE_INSTALL
#define VA_ENTRY va_runtime
#else
#define VA_ENTRY PilotMain
#endif
UInt32 VA_ENTRY(UInt16 cmd,MemPtr param,UInt16 flags){EventType ev;UInt16 error;VAResult r;const Char *message="Idle; responder must be active";(void)param;(void)flags;if(cmd!=sysAppLaunchCmdNormalLaunch)return 0;FrmGotoForm(1000);
 do{EvtGetEvent(&ev,evtWaitForever);if(!SysHandleEvent(&ev)&&!MenuHandleEvent(0,&ev,&error)){
 if(ev.eType==frmLoadEvent)FrmSetActiveForm(FrmInitForm(ev.data.frmLoad.formID));
 else if(ev.eType==frmOpenEvent){FrmDrawForm(FrmGetActiveForm());r[3]=snapshot(r);draw(r,message);}
 else if(ev.eType==ctlSelectEvent&&ev.data.ctlSelect.controlID==1001){r[3]=snapshot(r);if(!r[3])r[3]=exercise(r);{Err e=save(r);if(e)r[3]=e;}message=r[3]?"FAIL; keep screen":"PASS: four accepted writes";SndPlaySystemSound(r[3]?sndError:sndConfirmation);draw(r,message);}
 else FrmHandleEvent(FrmGetActiveForm(),&ev);
 }}while(ev.eType!=appStopEvent);FrmCloseAllForms();return 0;}
