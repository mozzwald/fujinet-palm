/* RAM helper: inspect recognized FNVR metadata/resources without launching it.
 * Does not insert, reset, register or write to the module. */
#include <PalmOS.h>
#include "../visorbus/hs_card.h"
static void hex(Coord y,const Char *label,UInt32 value) {
    Char text[48],number[10];StrCopy(text,label);StrIToH(number,value);
    StrCat(text,number);WinDrawChars(text,StrLen(text),2,y);
}
static UInt32 resource(DmOpenRef db,UInt32 type,UInt16 id) {
    MemHandle h;MemPtr p;UInt32 address=0;
    UInt16 index=DmFindResource(db,type,id,0);
    if(index==0xffff)return 0;
    h=DmGetResourceIndex(db,index);
    if(h){p=MemHandleLock(h);address=(UInt32)p;MemHandleUnlock(h);DmReleaseResource(h);}
    return address;
}
static void scan(void) {
    DmSearchStateType state;UInt16 card=0,foundCard=0,attr=0;LocalID id=0,foundID=0;
    UInt32 count=0,base=0,logical=0,size=0;DmOpenRef db;Boolean first=true;Err err;
    RectangleType area;RctSetRectangle(&area,0,18,160,120);WinEraseRectangle(&area,0);
    VisorCardAttrGet(1,visorCardCsBase,&base);/* HsExt.h attributes12/13: logical mapping, independent of CS0 alias. */
    VisorCardAttrGet(1,12,&logical);VisorCardAttrGet(1,13,&size);
    while(!(err=DmGetNextDatabaseByTypeCreator(first,&state,'appl','FNVR',false,&card,&id))) {
        first=false;++count;if(count==1){foundCard=card;foundID=id;}
    }
    hex(20,"FNVR count ",count);hex(32,"Slot1 base ",base);hex(44,"Logical ",logical);hex(56,"Log size ",size);
    if(count!=1){WinDrawChars("Require exactly one ROM FNVR",27,2,62);return;}
    hex(68,"Card ",foundCard);hex(80,"DB ID ",foundID);
    hex(92,"DB ptr ",(UInt32)MemLocalIDToGlobal(foundID,foundCard));
    DmDatabaseInfo(foundCard,foundID,0,&attr,0,0,0,0,0,0,0,0,0);hex(104,"DB attrs ",attr);
    db=DmOpenDatabase(foundCard,foundID,dmModeReadOnly);
    if(db){hex(116,"code1 ",resource(db,'code',1));hex(128,"tPAT ",resource(db,'tPAT',1000));DmCloseDatabase(db);}
    else WinDrawChars("Open failed; do not launch",25,2,116);
}
UInt32 PilotMain(UInt16 cmd,MemPtr param,UInt16 flags) {
    EventType event;UInt16 error;(void)param;(void)flags;
    if(cmd!=sysAppLaunchCmdNormalLaunch)return 0;
    FrmGotoForm(1000);
    do {
        EvtGetEvent(&event,evtWaitForever);
        if(!SysHandleEvent(&event)&&!MenuHandleEvent(0,&event,&error)) {
            if(event.eType==frmLoadEvent)FrmSetActiveForm(FrmInitForm(event.data.frmLoad.formID));
            else if(event.eType==frmOpenEvent)FrmDrawForm(FrmGetActiveForm());
            else if(event.eType==ctlSelectEvent&&event.data.ctlSelect.controlID==1001)scan();
            else FrmHandleEvent(FrmGetActiveForm(),&event);
        }
    }while(event.eType!=appStopEvent);
    FrmCloseAllForms();return 0;
}
