/* UI/event-loop structure reused from palm/apps/hello. No legacy FujiNet link.
 */
#include "hs_card.h"
#include "read_test.h"
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

static UInt16 selected_card = 1;

static void attribute(Coord y, const Char *name, UInt16 attr, Boolean byte) {
  UInt32 wide = 0;
  UInt8 small = 0;
  Char number[10];
  Err err = VisorCardAttrGet(selected_card, attr,
                             byte ? (void *)&small : (void *)&wide);
  WinDrawChars(name, StrLen(name), 4, y);
  if (err)
    WinDrawChars("--------", 8, 36, y);
  else {
    StrIToH(number, byte ? small : wide);
    WinDrawChars(number, 8, 36, y);
  }
  StrIToH(number, (UInt16)err);
  WinDrawChars(number + 4, 4, 128, y);
}

static void inspect(void) {
  UInt32 os = 0, hs = 0;
  Char text[40], number[10];
  Err err;
  clear();
  line(20, "Slot: ", selected_card);
  FtrGet(sysFtrCreator, sysFtrNumROMVersion, &os);
  err = FtrGet(VISOR_HS_CREATOR, VISOR_HS_VERSION, &hs);
  StrCopy(text, "OS ");
  StrIToH(number, os);
  StrCat(text, number);
  StrCat(text, " HS ");
  StrIToH(number, hs);
  StrCat(text, number);
  WinDrawChars(text, StrLen(text), 4, 31);
  if (err) {
    line(54, "Handspring err: ", (UInt16)err);
    return;
  }
  WinDrawChars("Attr", 4, 4, 43);
  WinDrawChars("Value", 5, 36, 43);
  WinDrawChars("Err", 3, 128, 43);
  attribute(55, "Rem", visorCardRemovable, true);
  attribute(66, "Hw", visorCardHwInstalled, true);
  attribute(77, "Sw", visorCardSwInstalled, true);
  attribute(88, "Rst", visorCardReset, true);
  attribute(99, "CS0", visorCardCsBase, false);
  attribute(110, "Size", visorCardCsSize, false);
  attribute(121, "ns", visorCardAccessTime, false);
}

static int reset_get(void *context, unsigned short card, unsigned short attr,
                     unsigned char *value) {
  (void)context;
  return VisorCardAttrGet(card, attr, value);
}
static int reset_set(void *context, unsigned short card, unsigned short attr,
                     unsigned char *value) {
  (void)context;
  return VisorCardAttrSet(card, attr, value);
}
static void reset_delay(void *context, unsigned long ms) {
  UInt32 ticks = (SysTicksPerSecond() * ms + 999) / 1000;
  (void)context;
  SysTaskDelay(ticks ? ticks : 1);
}
static const struct visor_reset_ops reset_ops = {reset_get, reset_set,
                                                 reset_delay, 0};

static void reset_result(struct visor_reset_result r) {
  static const Char *reasons[] = {"Pulse complete",
                                  "Invalid/internal slot",
                                  "Attribute query failed",
                                  "Slot is not removable",
                                  "Hardware not installed",
                                  "OS-managed card: refused",
                                  "RESET already asserted",
                                  "Assert failed",
                                  "Restore failed: check RESET",
                                  "Readback failed",
                                  "RESET still asserted"};
  clear();
  WinDrawChars(reasons[r.reason], StrLen(reasons[r.reason]), 4, 20);
  line(35, "Slot: ", selected_card);
  line(47, "Attr: ", r.attribute);
  line(59, "Query err: ", (UInt16)r.query_error);
  line(71, "Assert err: ", (UInt16)r.assert_error);
  line(83, "Release err: ", (UInt16)r.release_error);
  line(95, "Readback err: ", (UInt16)r.readback_error);
  if (!r.readback_error &&
      (r.reason == visorResetReady || r.reason >= visorResetAssertFailed))
    line(107, "After RESET: ", r.after);
  WinDrawChars("Module only; no memory reads",
               StrLen("Module only; no memory reads"), 4, 123);
}

static void pulse(void) {
  UInt32 hs;
  struct visor_reset_result result;
  if (FtrGet(VISOR_HS_CREATOR, VISOR_HS_VERSION, &hs)) {
    clear();
    WinDrawChars("No Handspring API", 17, 4, 40);
    return;
  }
  result = visor_reset_check(&reset_ops, selected_card);
  if (result.reason != visorResetReady) {
    reset_result(result);
    return;
  }
  if (FrmAlert(ResetConfirmAlert) != 1)
    return;
  /* Recheck after the modal confirmation; the slot may have changed. */
  result = visor_reset_pulse(&reset_ops, selected_card);
  reset_result(result);
}

static int read_wide(void *context, unsigned short card, unsigned short attr,
                     unsigned long *value) {
  (void)context;
  return VisorCardAttrGet(card, attr, value);
}

/* Use the Handspring removal-recovery list rather than Palm's general exception
 * list. Keep the module seated: this handles removal, not arbitrary bus faults.
 * No exits may bypass restoring the list after ErrSetJump.
 */
static int read_word(void *context, unsigned long address, unsigned short *value) {
  ErrExceptionType frame;
  MemPtr *volatile list;
  volatile UInt16 word = 0;
  volatile int error = 0;
  (void)context;
  list = VisorCardCatchListP();
  if (!list) return 0x8101;
  frame.err = 0;
  frame.nextP = (ErrExceptionPtr)*list;
  *list = (MemPtr)&frame;
  if (ErrSetJump(frame.state) == 0)
    word = *(volatile UInt16 *)address;
  else
    error = 0x810a; /* hsErrCardNotInstalled */
  *list = (MemPtr)frame.nextP;
  if (!error) *value = word;
  return error;
}

static const struct visor_read_ops read_ops = {reset_get, read_wide, read_word, 0};

static void read_result(struct visor_read_result r) {
  static const Char *reasons[] = {
      "8 reads complete", "Invalid/internal slot", "Attribute query failed",
      "Slot is not removable", "Hardware not installed", "Slot not registered",
      "RESET is asserted", "Invalid CS0 mapping", "Read interrupted",
      "CS0 mapping changed"};
  unsigned i;
  clear();
  WinDrawChars(reasons[r.reason], StrLen(reasons[r.reason]), 4, 20);
  line(33, "Base: ", r.base);
  for (i = 0; i < r.completed; ++i) {
    Char label[12], hex[10];
    StrIToH(hex, i * 2);
    StrCopy(label, "+"); StrCat(label, hex + 4); StrCat(label, ": ");
    line(45 + i * 10, label, r.words[i]);
  }
  if (r.reason != visorReadReady) {
    line(114, "Err: ", (UInt16)r.error);
    line(126, "Attr: ", r.attribute);
  } else
    WinDrawChars("Observed data; no ROM verdict", 29, 4, 128);
}

static void read_probe(void) {
  UInt32 hs;
  struct visor_read_result r;
  if (FtrGet(VISOR_HS_CREATOR, VISOR_HS_VERSION, &hs)) {
    clear(); WinDrawChars("No Handspring API", 17, 4, 40); return;
  }
  r = visor_read_check(&read_ops, selected_card);
  if (r.reason != visorReadReady) { read_result(r); return; }
  if (FrmAlert(ReadConfirmAlert) != 1) return;
  /* Recheck after confirmation and before every individual volatile word read. */
  r = visor_read_probe(&read_ops, selected_card);
  read_result(r);
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
  WinDrawChars("Build: visorbus-0.3.0", 20, 4, 88);
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
    if (event->data.ctlSelect.controlID == NextButton) {
      selected_card = (selected_card + 1) % 4;
      inspect();
      return true;
    }
    if (event->data.ctlSelect.controlID == ReadButton) {
      read_probe();
      return true;
    }
    if (event->data.ctlSelect.controlID == PulseButton) {
      pulse();
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
