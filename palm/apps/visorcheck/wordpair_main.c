/* Separate RAM-only FNVE application. No full ROM, reset or recognition UI. */
#include "pair_app.h"
#include "image_read.h"
#include "expected_rom.h"
UInt32 PilotMain(UInt16 cmd,MemPtr params,UInt16 flags) {
  EventType event;UInt16 error;
  (void)params;(void)flags;
  if(cmd!=sysAppLaunchCmdNormalLaunch)return 0;
  if(visor_image_local_crc(expected_rom_data,sizeof(expected_rom_data))!=EXPECTED_ROM_CRC) {
    FrmAlert(2101);return 1;
  }
  vp_open(expected_rom_data,sizeof(expected_rom_data),EXPECTED_ROM_CRC);
  do {
    EvtGetEvent(&event,vp_active()?0:evtWaitForever);
    if(SysHandleEvent(&event))continue;
    if(MenuHandleEvent(0,&event,&error))continue;
    if(event.eType==frmLoadEvent) {
      FormType *form=FrmInitForm(event.data.frmLoad.formID);
      FrmSetActiveForm(form);FrmSetEventHandler(form,vp_event);continue;
    }
    FrmDispatchEvent(&event);
  }while(event.eType!=appStopEvent);
  vp_close();FrmCloseAllForms();return 0;
}
