#include "app_identity.h"
/* Separate RAM diagnostic. Never embed this in the image it checks: its
 * expected length/CRC come from the independently verified module artifact. */
#include <PalmOS.h>
#include "hs_card.h"
#include "timed_read.h"
#include "expected_rom.h"
#include "result_store.h"
#include "diagnostic.h"
#include "burst_read.h"
#if EXPECTED_ROM_SIZE > VC_MAX_IMAGE
#error Result record capacity must be reviewed for larger images
#endif

enum { MainForm = 1000, CheckButton = 1001, CancelButton = 1002,
       DetailsButton = 1003, PassButton = 1004, FailButton = 1005, ConfirmAlert = 1100,
       ZeroButton = 1006, OnesButton = 1007, PrefixButton = 1008,
       SpacingButton = 1009, CaptureButton = 1010,
       SlowAlert = 1101, RestoreAlert = 1102 };
static struct visor_timed_read run;
static unsigned details_page;
static Boolean warned;
static Boolean reading;
static UInt32 reference_crc;
static unsigned char captured[EXPECTED_ROM_SIZE] __attribute__((aligned(2)));
static struct vc_result result;
static Boolean recording;
static Err storage_error;
static const Char *result_message;
static unsigned char pattern[VC_SHORT_BYTES];
static const unsigned char *expected_data;
static unsigned long selected_spacing, selected_capture;
static Char spacing_text[24];

static __inline__ void wait_spacing(unsigned long mode, unsigned long iterations) {
  if(mode==VC_SPACE_TICK) SysTaskDelay(1);
  else if(iterations) {
    UInt16 remaining=(UInt16)(iterations-1);
    /* Nominal delay only; actual gaps include bus/RAM waits and interrupts. */
    __asm__ volatile("1: dbra %0,1b" : "+d"(remaining) : : "cc");
  }
}
static void spacing_controls(void) {
  FormType *form=FrmGetActiveForm();
  StrCopy(spacing_text,"Delay: ");
  StrCat(spacing_text,vc_spacing_label(selected_spacing));
  CtlSetLabel((ControlType *)FrmGetObjectPtr(form,FrmGetObjectIndex(form,SpacingButton)),spacing_text);
  CtlSetLabel((ControlType *)FrmGetObjectPtr(form,FrmGetObjectIndex(form,CaptureButton)),
              vc_capture_label(selected_capture));
}


static void line(Coord y, const Char *label, UInt32 value) {
  Char text[48], number[10];
  StrCopy(text, label); StrIToH(number, value); StrCat(text, number);
  WinDrawChars(text, StrLen(text), 4, y);
}
static int get_byte(void *ctx, unsigned short card, unsigned short attr,
                    unsigned char *value) {
  (void)ctx; return VisorCardAttrGet(card, attr, value);
}
static int get_wide(void *ctx, unsigned short card, unsigned short attr,
                    unsigned long *value) {
  (void)ctx; return VisorCardAttrGet(card, attr, value);
}
/* Same Handspring module-removal recovery mechanism as Visor Bus Test.
 * This is not general protection from wiring faults or arbitrary bus errors. */
static int read_word(void *ctx, unsigned long address, unsigned short *value) {
  ErrExceptionType frame;
  MemPtr *volatile list;
  volatile UInt16 word = 0;
  volatile int error = 0;
  (void)ctx;
  list = VisorCardCatchListP();
  if (!list) return 0x8101;
  frame.err = 0; frame.nextP = (ErrExceptionPtr)*list; *list = (MemPtr)&frame;
  if (ErrSetJump(frame.state) == 0) word = *(volatile UInt16 *)address;
  else error = 0x810a;
  *list = (MemPtr)frame.nextP;
  if (!error) {
    UInt32 offset = address - run.image.guard.base;
    *value = word;
    if (recording && offset + 1 < EXPECTED_ROM_SIZE) {
      captured[offset] = (unsigned char)(word >> 8);
      captured[offset + 1] = (unsigned char)word;
    }
  }
  return error;
}
static const struct visor_read_ops ops = {get_byte, get_wide, read_word, 0};
static int set_time(void *ctx, unsigned short card, unsigned long value) {
  (void)ctx; return VisorCardAttrSet(card, visorCardAccessTime, &value);
}
static int journal(void *ctx, unsigned long original) {
  UInt32 value = original, verify = 0;
  UInt16 size = sizeof(verify);
  (void)ctx;
  PrefSetAppPreferences(VC_CREATOR, 1, 1, &value, sizeof(value), true);
  if (PrefGetAppPreferences(VC_CREATOR, 1, &verify, &size, true) != 1 ||
      size != sizeof(verify) || verify != value) return 0x8204;
  return 0;
}
static const struct visor_timing_ops timing_ops = {&ops, set_time, journal};
static int capture_batch(void *ctx, unsigned long address, unsigned char *destination,
    unsigned short words, unsigned long spacing, unsigned long iterations,
    unsigned short *completed) {
  ErrExceptionType frame;
  MemPtr *volatile list;
  volatile UInt16 done=0;
  volatile int error=0;
  volatile UInt16 *source=(volatile UInt16 *)address;
  UInt16 *target=(UInt16 *)destination;
  UInt16 i;
  (void)ctx; *completed=0;
  if(!words || words>VC_BURST_WORDS || (address&1) || ((UInt32)destination&1)) return 0x8102;
  list=VisorCardCatchListP();
  if(!list) return 0x8101;
  frame.err=0; frame.nextP=(ErrExceptionPtr)*list; *list=(MemPtr)&frame;
  if(ErrSetJump(frame.state)==0) {
    for(i=0;i<words;++i) {
      wait_spacing(spacing,iterations);
      /* Exactly one volatile module read and one native big-endian RAM store.
       * No expected data, CRC, GUI or database work in this loop. */
      *target++=*source++;
      /* Only this volatile progress store is required between physical reads.
       * The catch path reads done, never the modified pointers or loop index. */
      done=(UInt16)(i+1);
    }
  } else error=0x810a;
  *list=(MemPtr)frame.nextP;
  *completed=done;
  return error;
}
static const struct vc_burst_ops burst_ops={capture_batch,0};
static void finish(unsigned long status) {
  visor_timed_finish(&run, &timing_ops);
  reading = false;
  if (recording) {
    result.status = status;
    result.completed = run.image.completed;
    result.crc = run.image.crc ^ 0xffffffffUL;
    result.reason = run.image.guard.reason;
    result.error = (UInt16)run.image.guard.error;
    result.original = run.original; result.requested = run.requested;
    result.applied = run.applied; result.restored = run.restored;
    result.setup_error = (UInt16)run.error;
    result.restore_error = (UInt16)run.restore_error;
    if (result.reason || result.error || result.setup_error || result.restore_error)
      result.status = VC_ERROR;
    result.ended = TimGetSeconds();
    storage_error = vc_store_append(&result, captured, expected_data);
    recording = false;
    /* Notify only after reading and persistence finish, outside the bus path.
     * System sounds honor the user's sound-volume preference. */
    if (result.status == VC_PASS || result.status == VC_FAIL)
      SndPlaySystemSound(!storage_error && result.status == VC_PASS ?
                         sndConfirmation : sndError);
  }
  if (run.blocked && !warned) {
    warned = true;
    FrmAlert(RestoreAlert);
  }
}


static void draw(const Char *message) {
  RectangleType area;
  RctSetRectangle(&area, 0, 18, 160, 95); WinEraseRectangle(&area, 0);
  WinDrawChars(message, StrLen(message), 4, 22);
  line(39, "Test bytes: ", result.expected_size ? result.expected_size : EXPECTED_ROM_SIZE);
  line(49, "Read bytes: ", run.image.completed);
  line(59, "Expected: ", result.expected_size ? result.expected_crc : EXPECTED_ROM_CRC);
  line(69, "Bus CRC: ", run.image.crc ^ 0xffffffffUL);
  line(79, "Bad words: ", run.image.mismatch_count);
  line(89, "RAM CRC: ", reference_crc);
  line(99, "Error: ", (UInt16)(run.restore_error ? run.restore_error :
       run.error ? run.error : storage_error ? storage_error : run.image.guard.error));
  if (storage_error) {
    RctSetRectangle(&area, 0, 20, 160, 15); WinEraseRectangle(&area, 0);
    WinDrawChars("RESULT NOT SAVED", 16, 4, 22);
  }
}
static void timing_details(void) {
  RectangleType area;
  Char text[32], number[12];
  unsigned i;
  const Char *labels[] = {"Original ns: ", "Request ns: ", "Applied ns: ", "Restored ns: "};
  UInt32 values[4];
  values[0] = run.original; values[1] = run.requested;
  values[2] = run.applied; values[3] = run.restored;
  RctSetRectangle(&area, 0, 18, 160, 95); WinEraseRectangle(&area, 0);
  WinDrawChars("Timing (0 = not recorded)", 24, 4, 20);
  for (i = 0; i < 4; ++i) {
    StrCopy(text, labels[i]); StrIToA(number, values[i]); StrCat(text, number);
    WinDrawChars(text, StrLen(text), 4, 39 + i * 13);
  }
  line(93, "Setup err: ", (UInt16)run.error);
  line(104, "Restore err: ", (UInt16)run.restore_error);
}
static void details(void) {
  RectangleType area;
  unsigned i, count = run.image.mismatch_count < 8 ? (unsigned)run.image.mismatch_count : 8;
  RctSetRectangle(&area, 0, 18, 160, 95); WinEraseRectangle(&area, 0);
  line(20, "Bad words: ", run.image.mismatch_count);
  WinDrawChars("Offset   Got   Expected", 23, 4, 31);
  for (i = 0; i < count; ++i) {
    Char hex[10];
    StrIToH(hex, run.image.first[i].offset); WinDrawChars(hex + 4, 4, 4, 40 + i * 8);
    StrIToH(hex, run.image.first[i].actual); WinDrawChars(hex + 4, 4, 58, 40 + i * 8);
    StrIToH(hex, run.image.first[i].expected); WinDrawChars(hex + 4, 4, 106, 40 + i * 8);
  }
  line(105, "Reason: ", run.image.guard.reason);
}
static Boolean begin_result(unsigned long kind) {
  MemSet(&run, sizeof(run), 0);
  MemSet(&result, sizeof(result), 0);
  result.spacing_mode=(kind==VC_SYNTH_PASS || kind==VC_SYNTH_FAIL)?VC_SPACE_TICK:selected_spacing;
  result.capture_profile=(kind==VC_SYNTH_PASS || kind==VC_SYNTH_FAIL)?VC_CAPTURE_GUARDED:selected_capture;
  result.delay_iterations=vc_spacing_iterations(result.spacing_mode);
  result.batch_words=vc_capture_words(result.capture_profile);
  expected_data = vc_expected(kind, expected_rom_data, EXPECTED_ROM_SIZE,
                             pattern, sizeof(pattern), &result.expected_size);
  if (!expected_data) { draw("Invalid test reference"); return false; }
  result.expected_crc = visor_image_local_crc(expected_data, result.expected_size);
  result.reference_crc = reference_crc;
  result.kind = kind; result.started = TimGetSeconds();
  storage_error = vc_store_begin_reference(&result, EXPECTED_ROM_SIZE, EXPECTED_ROM_CRC);
  recording = !storage_error;
  details_page = 0;
  if (storage_error) draw("Install session control PDB");
  return recording;
}
static void synthetic(Boolean fail) {
  unsigned long i;
  if (!begin_result(fail ? VC_SYNTH_FAIL : VC_SYNTH_PASS)) return;
  run.image.length = run.image.completed = EXPECTED_ROM_SIZE;
  for (i = 0; i < EXPECTED_ROM_SIZE; ++i) captured[i] = expected_rom_data[i];
  if (fail) {
    for (i = 1; i < EXPECTED_ROM_SIZE; i += 2) captured[i] ^= 1;
    run.image.mismatch_count = EXPECTED_ROM_SIZE / 2;
    for (i = 0; i < 8; ++i) {
      run.image.first[i].offset = i * 2;
      run.image.first[i].expected = ((UInt16)expected_rom_data[i*2] << 8) | expected_rom_data[i*2+1];
      run.image.first[i].actual = run.image.first[i].expected ^ 1;
    }
  }
  run.image.crc = visor_image_local_crc(captured, EXPECTED_ROM_SIZE) ^ 0xffffffffUL;
  finish(fail ? VC_FAIL : VC_PASS);
  result_message = fail ? "Synthetic FAIL saved" : "Synthetic PASS saved";
  draw(result_message);
}
static Boolean main_event(EventType *e) {
  if (e->eType == frmOpenEvent) {
    FrmDrawForm(FrmGetActiveForm());
    spacing_controls();
    draw(run.blocked ? "BLOCKED: timing journal" : "Collector: await host cue"); return true;
  }
  if(e->eType==ctlSelectEvent && (e->data.ctlSelect.controlID==SpacingButton ||
                                e->data.ctlSelect.controlID==CaptureButton)) {
    if(!reading) {
      if(e->data.ctlSelect.controlID==SpacingButton)
        selected_spacing=(selected_spacing+1)%VC_SPACE_COUNT;
      else selected_capture=(selected_capture+1)%VC_CAPTURE_COUNT;
      spacing_controls();
    }
    return true;
  }
  if (e->eType == ctlSelectEvent && e->data.ctlSelect.controlID == CancelButton) {
    finish(VC_CANCELLED); result_message = "Cancelled; send RP x";
    draw(run.blocked ? "RESTORE FAIL: blocked" : result_message); return true;
  }
  if (e->eType == ctlSelectEvent && e->data.ctlSelect.controlID == DetailsButton) {
    if (!reading) {
      details_page = (details_page + 1) % 3;
      if (details_page == 1) details();
      else if (details_page == 2) timing_details();
      else draw(run.blocked ? "RESTORE FAIL: blocked" : result_message);
    }
    return true;
  }
  if (e->eType == ctlSelectEvent &&
      (e->data.ctlSelect.controlID == CheckButton ||
       e->data.ctlSelect.controlID == PassButton ||
       e->data.ctlSelect.controlID == FailButton ||
       e->data.ctlSelect.controlID == ZeroButton ||
       e->data.ctlSelect.controlID == OnesButton ||
       e->data.ctlSelect.controlID == PrefixButton)) {
    UInt32 hs;
    if (reading) return true;
    if (run.blocked) { FrmAlert(RestoreAlert); return true; }
    if (reference_crc != EXPECTED_ROM_CRC) { draw("RAM reference CRC FAIL"); return true; }
    if (e->data.ctlSelect.controlID == PassButton || e->data.ctlSelect.controlID == FailButton) {
      synthetic(e->data.ctlSelect.controlID == FailButton); return true;
    }
    if (FtrGet(VISOR_HS_CREATOR, VISOR_HS_VERSION, &hs)) {
      draw("No Handspring API"); return true;
    }
    if (FrmAlert(ConfirmAlert) != 1) return true;
    if (!begin_result(e->data.ctlSelect.controlID == ZeroButton ? VC_CONST_ZERO :
          e->data.ctlSelect.controlID == OnesButton ? VC_CONST_ONES :
          e->data.ctlSelect.controlID == PrefixButton ? VC_ROM_PREFIX : VC_ROM)) return true;
    visor_timed_begin(&run, &timing_ops, result.expected_size, expected_data, 0);
    reading = run.active;
    if (!reading) finish(VC_ERROR);
    result_message = reading ? "Reading CS0 words" : "Refused; send RP x";
    draw(result_message); return true;
  }
  if (e->eType == nilEvent && reading) {
    /* No database allocation or writes during the bus read. Capture only the
     * returned word in RAM; append the completed result after output release. */
    if(result.capture_profile==VC_CAPTURE_BURST)
      vc_burst_step(&run,&timing_ops,&burst_ops,captured,sizeof(captured),result.spacing_mode);
    else {
      wait_spacing(result.spacing_mode,result.delay_iterations);
      visor_timed_step(&run, &timing_ops);
    }
    EvtResetAutoOffTimer();
    if (!run.active) {
      Boolean pass = run.image.completed == run.image.length &&
          (run.image.crc ^ 0xffffffffUL) == result.expected_crc && !run.image.mismatch_count;
      finish(pass ? VC_PASS : VC_FAIL);
      result_message = run.blocked ? "RESTORE FAIL: blocked" :
          result.status == VC_ERROR ? "Interrupted; send RP x" :
          pass ? "Pattern CRC PASS" : "Pattern CRC FAIL";
      draw(result_message);
    } else if (!(run.image.completed & 127)) draw("Reading CS0 words");
    return true;
  }
  return false;
}
static Boolean app_event(EventType *e) {
  if (e->eType == frmLoadEvent) {
    FormType *form = FrmInitForm(e->data.frmLoad.formID);
    FrmSetActiveForm(form); FrmSetEventHandler(form, main_event); return true;
  }
  return false;
}
UInt32 PilotMain(UInt16 cmd, MemPtr params, UInt16 flags) {
  EventType event;
  UInt16 error;
  (void)params; (void)flags;
  if (cmd != sysAppLaunchCmdNormalLaunch) return 0;
  reading = false;
  selected_spacing=VC_SPACE_TICK; selected_capture=VC_CAPTURE_GUARDED;
  expected_data = expected_rom_data;
  {
    UInt32 pending = 0;
    UInt16 size = sizeof(pending);
    Int16 version = PrefGetAppPreferences(VC_CREATOR, 1, &pending, &size, true);
    MemSet(&run, sizeof(run), 0);
    /* Missing preference is a clean first launch; malformed records fail closed. */
    if (version != noPreferenceFound &&
        (version != 1 || size != sizeof(pending) || pending)) {
      run.blocked = 1; run.original = pending;
    }
  }
  warned = false; recording = false; storage_error = 0;
  result_message = "Collector: await host cue";
  reference_crc = visor_image_local_crc(expected_rom_data, sizeof(expected_rom_data));
  FrmGotoForm(MainForm);
  do {
    EvtGetEvent(&event, reading ? 0 : evtWaitForever);
    if (SysHandleEvent(&event)) continue;
    if (MenuHandleEvent(0, &event, &error)) continue;
    if (app_event(&event)) continue;
    FrmDispatchEvent(&event);
  } while (event.eType != appStopEvent);
  finish(VC_CANCELLED);
  FrmCloseAllForms();
  return 0;
}
