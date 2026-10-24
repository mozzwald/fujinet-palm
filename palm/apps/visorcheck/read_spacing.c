#include "read_spacing.h"
/* Nominal33MHz MC68000 DBRA loop:10cycles/taken iteration,14 on expiry.
 * RAM waits, interrupts, call overhead and read-loop work are additional.
 * These are requested delays, NEVER a measured bus-gap or timing margin.
 * Palm OS3.5 SDK has no SysMicroDelay API. No clock/timer registers are changed.
 * Reference: NXP MC68000UM Table8-9. Scope qualification remains required. */
unsigned long vc_spacing_iterations(unsigned long mode) {
  return mode == VC_SPACE_100US ? 330 : mode == VC_SPACE_10US ? 33 :
         mode == VC_SPACE_1US ? 3 : 0;
}
unsigned long vc_spacing_us(unsigned long mode) {
  return mode == VC_SPACE_100US ? 100 : mode == VC_SPACE_10US ? 10 :
         mode == VC_SPACE_1US ? 1 : 0;
}
unsigned long vc_capture_words(unsigned long profile) {
  return profile == VC_CAPTURE_BURST ? VC_BURST_WORDS : 1;
}
int vc_spacing_valid(unsigned long mode, unsigned long profile,
    unsigned long iterations, unsigned long batch_words) {
  return mode < VC_SPACE_COUNT && profile < VC_CAPTURE_COUNT &&
         iterations == vc_spacing_iterations(mode) &&
         batch_words == vc_capture_words(profile);
}
const char *vc_spacing_label(unsigned long mode) {
  static const char *const labels[] = {"Tick", "100us", "10us", "1us", "None"};
  return mode < VC_SPACE_COUNT ? labels[mode] : "Invalid";
}
const char *vc_capture_label(unsigned long profile) {
  return profile == VC_CAPTURE_GUARDED ? "Each" :
         profile == VC_CAPTURE_BURST ? "Burst64" : "Invalid";
}
