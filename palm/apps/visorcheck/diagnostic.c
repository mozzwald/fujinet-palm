#include "diagnostic.h"
#include <string.h>
const unsigned char *vc_expected(unsigned long kind, const unsigned char *rom,
    unsigned long rom_size, unsigned char *scratch, unsigned long capacity,
    unsigned long *length) {
  if (!rom || !rom_size || (rom_size & 1)) return 0;
  if (kind == VC_CONST_ZERO || kind == VC_CONST_ONES) {
    if (!scratch || capacity < VC_SHORT_BYTES || rom_size < VC_SHORT_BYTES) return 0;
    memset(scratch, kind == VC_CONST_ZERO ? 0 : 255, VC_SHORT_BYTES);
    *length = VC_SHORT_BYTES; return scratch;
  }
  if (kind == VC_ROM_PREFIX) {
    if (rom_size < VC_SHORT_BYTES) return 0;
    *length = VC_SHORT_BYTES; return rom;
  }
  if (kind < VC_ROM || kind > VC_SYNTH_FAIL) return 0;
  *length = rom_size; return rom;
}
