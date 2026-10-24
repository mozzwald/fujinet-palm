/* ROM-only recognition/execution probe. Never install its PRC in RAM.
 * No startup/insert notifications, reset pulses, direct bus reads or writes. */
#include <PalmOS.h>
static void hex(Coord y,const Char *label,UInt32 value) {
    Char text[48],number[10];StrCopy(text,label);StrIToH(number,value);
    StrCat(text,number);WinDrawChars(text,StrLen(text),2,y);
}
static UInt32 crc32(const UInt8 *p,UInt32 n) {
    UInt32 crc=0xffffffffUL,i;UInt16 bit;
    for(i=0;i<n;++i){crc^=p[i];for(bit=0;bit<8;++bit)crc=(crc>>1)^((crc&1)?0xedb88320UL:0);}
    return ~crc;
}
UInt32 PilotMain(UInt16 cmd,MemPtr param,UInt16 flags) {
    EventType event;UInt16 error,card=0;LocalID db=0;MemHandle h;const UInt8 *pattern;
    (void)param;(void)flags;
    if(cmd!=sysAppLaunchCmdNormalLaunch)return 0;
    FrmGotoForm(1000);
    do {
        EvtGetEvent(&event,evtWaitForever);
        if(!SysHandleEvent(&event)&&!MenuHandleEvent(0,&event,&error)) {
            if(event.eType==frmLoadEvent)FrmSetActiveForm(FrmInitForm(event.data.frmLoad.formID));
            else if(event.eType==frmOpenEvent) {
                FrmDrawForm(FrmGetActiveForm());
                SysCurAppDatabase(&card,&db);
                hex(24,"Card ",card);hex(38,"DB ID ",db);
                hex(52,"PilotMain ",(UInt32)PilotMain);
                h=DmGetResource('tPAT',1000);
                if(h) {
                    pattern=MemHandleLock(h);hex(66,"Pattern ",(UInt32)pattern);
                    hex(80,"CRC ",crc32(pattern,MemHandleSize(h)));
                    MemHandleUnlock(h);DmReleaseResource(h);
                }
                WinDrawChars("Expected CRC 29058C73",21,2,100);
                WinDrawChars("ROM FNVR R3.20261023",19,2,114);
            } else FrmHandleEvent(FrmGetActiveForm(),&event);
        }
    }while(event.eType!=appStopEvent);
    FrmCloseAllForms();return 0;
}
