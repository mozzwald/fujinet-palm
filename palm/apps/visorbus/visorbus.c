/* UI/event-loop structure reused from palm/apps/hello. No legacy FujiNet link.
 */
#include "hs_card.h"
#include "visorbus_rsc.h"
#include <PalmOS.h>

static void line(Coord y, const Char *label, UInt32 value) {
    Char text[48], number[10];
    StrCopy(text, label);
    StrIToH(number, value);
    StrCat(text, number);
    WinDrawChars(text, StrLen(text), 4, y);
}

static UInt32 crc32(const UInt8 *data, UInt32 size) {
    UInt32 crc = 0xffffffffUL, i;
    UInt16 bit;
    for (i = 0; i < size; ++i) {
        crc ^= data[i];
        for (bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ ((crc & 1) ? 0xedb88320UL : 0);
    }
    return ~crc;
}

static void clear(void) {
    RectangleType area;
    RctSetRectangle(&area, 0, 18, 160, 120);
    WinEraseRectangle(&area, 0);
}

static void inspect(void) {
    UInt32 os = 0, hs = 0, base = 0, size = 0, access = 0;
    UInt16 card;
    UInt8 installed = 0;
    Err err;
    clear();
    FtrGet(sysFtrCreator, sysFtrNumROMVersion, &os);
    line(22, "OS: ", os);
    err = FtrGet(VISOR_HS_CREATOR, VISOR_HS_VERSION, &hs);
    line(34, "Handspring: ", hs);
    if (err) {
        WinDrawChars("No Handspring API", 17, 4, 52);
        return;
    }
    /* Bounds avoid unbounded scans; no physical memory accesses are performed.
     */
    for (card = 0; card < 4; ++card) {
        installed = 0;
        err = VisorCardAttrGet(card, visorCardRemovable, &installed);
        if (err || !installed)
            continue;
        installed = 0;
        err = VisorCardAttrGet(card, visorCardHwInstalled, &installed);
        if (err || !installed)
            continue;
        if (VisorCardAttrGet(card, visorCardCsBase, &base) ||
            VisorCardAttrGet(card, visorCardCsSize, &size) ||
            VisorCardAttrGet(card, visorCardAccessTime, &access)) {
            WinDrawChars("Card attributes failed", 22, 4, 52);
            return;
        }
        line(48, "Card: ", card);
        line(60, "CS0: ", base);
        line(72, "CS size: ", size);
        line(84, "Access ns: ", access);
        if (base > 0xffffffffUL - size) {
            WinDrawChars("Invalid CS range", 16, 4, 98);
            return;
        }
        line(96, "CS1: ", base + size);
        WinDrawChars("Bus outputs disabled", 20, 4, 116);
        return;
    }
    WinDrawChars("No module reported", 18, 4, 52);
    WinDrawChars("No direct bus accesses", 22, 4, 68);
}

static void pattern(void) {
    MemHandle handle;
    UInt8 *data;
    UInt32 size, i;
    Boolean ok = true;
    clear();
    handle = DmGet1Resource('tPAT', 1000);
    if (!handle) {
        WinDrawChars("Pattern missing", 15, 4, 30);
        return;
    }
    size = MemHandleSize(handle);
    data = MemHandleLock(handle);
    if (!data) {
        DmReleaseResource(handle);
        return;
    }
    if (size != 256)
        ok = false;
    else
        for (i = 0; i < size; ++i)
            if (data[i] != (UInt8)i)
                ok = false;
    WinDrawChars(ok ? "Pattern PASS" : "Pattern FAIL", 12, 4, 30);
    line(48, "Bytes: ", size);
    line(64, "CRC32: ", crc32(data, size));
    WinDrawChars("Build: visorbus-0.1.0", 20, 4, 88);
    WinDrawChars("No direct bus accesses", 22, 4, 108);
    MemHandleUnlock(handle);
    DmReleaseResource(handle);
}

static Boolean main_event(EventType *event) {
    if (event->eType == frmOpenEvent) {
        FrmDrawForm(FrmGetActiveForm());
        inspect();
        return true;
    }
    if (event->eType == ctlSelectEvent) {
        if (event->data.ctlSelect.controlID == InspectButton) {
            inspect();
            return true;
        }
        if (event->data.ctlSelect.controlID == PatternButton) {
            pattern();
            return true;
        }
    }
    return false;
}

static Boolean app_event(EventType *event) {
    if (event->eType == frmLoadEvent) {
        FormType *form = FrmInitForm(event->data.frmLoad.formID);
        FrmSetActiveForm(form);
        FrmSetEventHandler(form, main_event);
        return true;
    }
    return false;
}

UInt32 PilotMain(UInt16 cmd, MemPtr params, UInt16 flags) {
    EventType event;
    UInt16 error;
    (void)params;
    (void)flags;
    if (cmd != sysAppLaunchCmdNormalLaunch)
        return 0;
    FrmGotoForm(MainForm);
    do {
        EvtGetEvent(&event, evtWaitForever);
        if (SysHandleEvent(&event))
            continue;
        if (MenuHandleEvent(0, &event, &error))
            continue;
        if (app_event(&event))
            continue;
        FrmDispatchEvent(&event);
    } while (event.eType != appStopEvent);
    FrmCloseAllForms();
    return 0;
}
