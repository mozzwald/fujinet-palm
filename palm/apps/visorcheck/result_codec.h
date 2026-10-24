#ifndef VISOR_RESULT_CODEC_H
#define VISOR_RESULT_CODEC_H
/* Explicit big-endian bytes, independent of compiler struct layout. */
#include "read_spacing.h"
#define VC_HEADER 160UL
#define VC_CONFIG_SIZE 68UL
#define VC_MAX_IMAGE 12000UL
#define VC_VERSION 0x00000601UL
#define VC_STARTED 0
#define VC_PASS 1
#define VC_FAIL 2
#define VC_CANCELLED 3
#define VC_ERROR 4
#define VC_ROM 1
#define VC_SYNTH_PASS 2
#define VC_SYNTH_FAIL 3
#define VC_CONST_ZERO 4
#define VC_CONST_ONES 5
#define VC_ROM_PREFIX 6
#define VC_SHORT_BYTES 128UL
struct vc_result {
  unsigned char session[16], sha[32];
  unsigned long sequence, kind, status, expected_size, expected_crc, reference_crc;
  unsigned long completed, crc, reason, error, original, requested, applied, restored;
  unsigned long setup_error, restore_error, started, ended;
  unsigned long spacing_mode, capture_profile, delay_iterations, batch_words;
};
unsigned long vc_get32(const unsigned char *);
void vc_put32(unsigned char *, unsigned long);
int vc_config(const unsigned char *, unsigned long, unsigned char *, unsigned char *,
              unsigned long, unsigned long);
unsigned long vc_encode(unsigned char *, unsigned long, const struct vc_result *,
                        const unsigned char *, const unsigned char *);
#endif
