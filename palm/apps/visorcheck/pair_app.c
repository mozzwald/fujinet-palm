/* RAM-only paired-read mode. All slot attributes are read-only. */
#include "app_identity.h"
#include "pair_app.h"
#include "pair_codec.h"
#include "../visorbus/hs_card.h"
#include "../visorbus/read_test.h"
#include <string.h>
#ifdef VC_WORDPAIR_DIAGNOSTIC
extern UInt32 vc_words_0(const void *),vc_words_1(const void *),vc_words_2(const void *),
  vc_words_4(const void *),vc_words_8(const void *),vc_words_16(const void *),
  vc_words_32(const void *),vc_words_64(const void *);
static UInt32 (*const word_loads[])(const void *)={vc_words_0,vc_words_1,vc_words_2,
  vc_words_4,vc_words_8,vc_words_16,vc_words_32,vc_words_64};
static const UInt32 nop_counts[]={0,1,2,4,8,16,32,64};
static unsigned selected_nops;
#else
extern UInt32 vc_pair_load(const void *address);
#endif
enum { PairRun=2001,PairCancel=2002,PairCount=2003,PairPattern=2004,PairBack=2005,
       PairConfirm=2100,pairBadConfig=0x8501,pairStorage=0x8502,
       pairAccess=0x8503,pairTimingChanged=0x8504 };
#ifdef VC_WORDPAIR_DIAGNOSTIC
static const UInt32 counts[]={4};
#else
static const UInt32 counts[]={4,100,10000};
#endif
static const unsigned char *reference;
static UInt32 reference_size,reference_crc,contrast_offset;
static unsigned selected_count,selected_pattern;
static struct vp_result result;
static unsigned char *payload;
static Boolean active;
static Err storage_error;
static const Char *message="Await host cue";
static int get_byte(void *ctx,unsigned short card,unsigned short attr,unsigned char *v) {
  (void)ctx;return VisorCardAttrGet(card,attr,v);
}
static int get_wide(void *ctx,unsigned short card,unsigned short attr,unsigned long *v) {
  (void)ctx;return VisorCardAttrGet(card,attr,v);
}
static const struct visor_read_ops bus={get_byte,get_wide,0,0};
static void number(Coord y,const Char *label,UInt32 value) {
  Char text[48],n[10];StrCopy(text,label);StrIToH(n,value);StrCat(text,n);
  WinDrawChars(text,StrLen(text),2,y);
}
static void draw(void) {
  RectangleType area;RctSetRectangle(&area,0,18,160,105);WinEraseRectangle(&area,0);
  WinDrawChars(message,StrLen(message),2,20);
  number(33,"Pairs ",active?result.requested:counts[selected_count]);
  number(45,"Offset ",active?result.offset:selected_pattern?contrast_offset:8);
  number(57,"Expect ",active?result.expected:vc_get32(reference+(selected_pattern?contrast_offset:8)));
#ifdef VC_WORDPAIR_DIAGNOSTIC
  number(69,"NOPs ",active?result.nops:nop_counts[selected_nops]);
  number(81,"Done ",result.completed);number(93,"First bad ",result.first_bad);
  number(105,"Second bad ",result.second_bad);
  number(117,"Error ",storage_error?storage_error:result.error);
#else
  number(69,"Done ",result.completed);number(81,"First bad ",result.first_bad);
  number(93,"Second bad ",result.second_bad);
  number(105,"Error ",storage_error?storage_error:result.error);
  number(117,"Reason ",result.reason);
#endif
}
static Err database(LocalID *id,const Char *name,UInt32 type,Boolean create) {
  UInt32 got_type=0,creator=0;Err err;
  *id=DmFindDatabase(0,name);
  if(!*id && create) {
    err=DmCreateDatabase(0,name,VC_CREATOR,type,false);if(err)return err;
    *id=DmFindDatabase(0,name);
  }
  if(!*id)return pairBadConfig;
  err=DmDatabaseInfo(0,*id,0,0,0,0,0,0,0,0,0,&got_type,&creator);
  return err?err:(got_type!=type || creator!=VC_CREATOR)?pairBadConfig:0;
}
static Err append(void) {
  LocalID id;DmOpenRef db;MemHandle h;MemPtr p;Err err;
  UInt16 index=dmMaxRecordIndex;
  unsigned long n=VP_HEADER+4*result.completed+4;
  unsigned char *encoded;
  err=database(&id,VP_RESULTS_DB,'VRSL',true);if(err)return err;
  encoded=MemPtrNew(n);if(!encoded)return pairStorage;
  if(vp_encode(encoded,n,&result,payload)!=n){MemPtrFree(encoded);return pairStorage;}
  db=DmOpenDatabase(0,id,dmModeReadWrite);
  if(!db){MemPtrFree(encoded);return DmGetLastErr();}
  h=DmNewRecord(db,&index,n);
  if(!h){err=DmGetLastErr();MemPtrFree(encoded);DmCloseDatabase(db);return err;}
  p=MemHandleLock(h);err=p?DmWrite(p,0,encoded,n):pairStorage;
  if(p)MemHandleUnlock(h);
  {Err release=DmReleaseRecord(db,index,!err);if(!err)err=release;}
  if(err)DmRemoveRecord(db,index);
  MemPtrFree(encoded);
  {Err close=DmCloseDatabase(db);if(!err)err=close;}
  return err;
}
static Err begin_record(void) {
  LocalID id;DmOpenRef db;MemHandle h;const unsigned char *p;Err err;
  unsigned char saved[20],verify[20];UInt16 size=20;Int16 version;UInt32 sequence;
  err=database(&id,VP_CONTROL_DB,'VCTL',false);if(err)return err;
  db=DmOpenDatabase(0,id,dmModeReadOnly);if(!db)return DmGetLastErr();
  h=DmNumRecords(db)==1?DmQueryRecord(db,0):0;
  p=h?MemHandleLock(h):0;
  err=(!p || !vc_config(p,MemHandleSize(h),result.session,result.sha,
                         reference_size,reference_crc))?pairBadConfig:0;
  if(p)MemHandleUnlock(h);DmCloseDatabase(db);if(err)return err;
  /* Separate preference from the existing checker session/sequence. */
  version=PrefGetAppPreferences(VC_CREATOR,3,saved,&size,true);
  if(version!=noPreferenceFound && (version!=1 || size!=20))return pairBadConfig;
  sequence=version==1&&!memcmp(saved,result.session,16)?vc_get32(saved+16):0;
  if(sequence==0xffffffffUL)return pairBadConfig;
  memcpy(saved,result.session,16);vc_put32(saved+16,sequence+1);
  PrefSetAppPreferences(VC_CREATOR,3,1,saved,20,true);size=20;
  if(PrefGetAppPreferences(VC_CREATOR,3,verify,&size,true)!=1 || size!=20 ||
     memcmp(saved,verify,20))return pairStorage;
  result.sequence=sequence+1;return append();
}
static Boolean check_slot(void) {
  struct visor_read_result now=visor_read_check_physical(&bus,1);
  if(now.reason || now.base!=result.base || now.size!=result.size) {
    result.reason=now.reason?now.reason:visorReadMappingChanged;
    result.error=now.error?now.error:pairAccess;return false;
  }
  return true;
}
static void finish(UInt32 status) {
  if(!active)return;
  active=false;
  if(!check_slot())status=VC_ERROR;
  else {
    Err err=VisorCardAttrGet(1,visorCardAccessTime,&result.after);
    if(err || result.after!=result.before) {
      result.error=err?err:pairTimingChanged;status=VC_ERROR;
    }
  }
  result.status=status;result.ended=TimGetSeconds();storage_error=append();
  if(payload){MemPtrFree(payload);payload=0;}
  message=storage_error?"RESULT NOT SAVED":status==VC_PASS?"Pair PASS saved":
      status==VC_FAIL?"Pair FAIL saved":"Incomplete saved";
  if(status==VC_PASS || status==VC_FAIL)
    SndPlaySystemSound(storage_error || status==VC_FAIL?sndError:sndConfirmation);
}
static void start(void) {
  struct visor_read_result guard;UInt32 hs;Err err;
  if(active)return;
  if(FtrGet(VISOR_HS_CREATOR,VISOR_HS_VERSION,&hs)){message="No Handspring API";return;}
  guard=visor_read_check_physical(&bus,1);
  if(guard.reason){result.reason=guard.reason;result.error=guard.error;message="Slot guard refused";return;}
  if(FrmAlert(PairConfirm)!=1)return;
  MemSet(&result,sizeof(result),0);storage_error=0;
  result.requested=counts[selected_count];result.offset=selected_pattern?contrast_offset:8;
#ifdef VC_WORDPAIR_DIAGNOSTIC
  result.nops=nop_counts[selected_nops];
#endif
  result.expected=vc_get32(reference+result.offset);result.reference_crc=reference_crc;
  result.base=guard.base;result.size=guard.size;
  if((result.base&3) || result.offset+4>result.size){message="Unaligned / outside slot";return;}
  err=VisorCardAttrGet(1,visorCardAccessTime,&result.before);
  if(err || !result.before){message="Cannot read access time";return;}
  payload=MemPtrNew(result.requested*4);if(!payload){message="No capture memory";return;}
  result.started=TimGetSeconds();storage_error=begin_record();
  if(storage_error){MemPtrFree(payload);payload=0;message="START not saved";return;}
  active=true;message="Reading paired words";
}
static void step(void) {
  ErrExceptionType frame;MemPtr *volatile list;volatile UInt32 value=0;
  volatile int error=0;
  SysTaskDelay(1);
  if(!check_slot()){finish(VC_ERROR);return;}
  list=VisorCardCatchListP();if(!list){result.error=pairAccess;finish(VC_ERROR);return;}
  /* A tick BETWEEN longwords isolates each pair. No trap or logging between
   * the two halfword cycles of the external assembly MOVE.L. */
  frame.err=0;frame.nextP=(ErrExceptionPtr)*list;*list=(MemPtr)&frame;
  if(ErrSetJump(frame.state)==0) {
#ifdef VC_WORDPAIR_DIAGNOSTIC
    value=word_loads[selected_nops]((const void *)(result.base+result.offset));
#else
    value=vc_pair_load((const void *)(result.base+result.offset));
#endif
  }
  else error=pairAccess;
  *list=(MemPtr)frame.nextP;
  if(error){result.error=error;finish(VC_ERROR);return;}
  vp_accept(&result,payload,value);EvtResetAutoOffTimer();
  if(result.completed==result.requested)finish(result.long_bad?VC_FAIL:VC_PASS);
}
void vp_open(const unsigned char *rom,unsigned long size,unsigned long crc) {
  unsigned long off;unsigned best=0;
  reference=rom;reference_size=size;reference_crc=crc;contrast_offset=8;
  /* Choose existing, 4-byte-aligned data with maximally opposite halfwords;
   * current distinct ROM: offset0444, A1745C8F,14/16 bits different. */
  for(off=0;off+4<=size;off+=4) {
    unsigned long value=vc_get32(rom+off),bits=(value>>16)^(value&0xffff);unsigned n=0;
    while(bits){n+=(unsigned)(bits&1);bits>>=1;}
    if(n>best){best=n;contrast_offset=off;}
  }
  FrmGotoForm(PairForm);
}
Boolean vp_active(void){return active;}
void vp_close(void){finish(VC_CANCELLED);}
Boolean vp_event(EventType *e) {
  if(e->eType==frmOpenEvent){FrmDrawForm(FrmGetActiveForm());draw();return true;}
  if(e->eType==nilEvent && active){step();draw();return true;}
  if(e->eType!=ctlSelectEvent)return false;
  switch(e->data.ctlSelect.controlID) {
  case PairRun:start();break;
  case PairCancel:finish(VC_CANCELLED);break;
  case PairCount:if(!active) {
#ifdef VC_WORDPAIR_DIAGNOSTIC
    selected_nops=(selected_nops+1)%(sizeof(nop_counts)/sizeof(nop_counts[0]));
#else
    selected_count=(selected_count+1)%3;
#endif
    }break;
  case PairPattern:if(!active)selected_pattern^=1;break;
  case PairBack:if(!active){
#ifdef VC_WORDPAIR_DIAGNOSTIC
    EventType stop;MemSet(&stop,sizeof(stop),0);stop.eType=appStopEvent;EvtAddEventToQueue(&stop);
#else
    FrmGotoForm(1000);
#endif
    return true;}break;
  default:return false;
  }
  draw();return true;
}
