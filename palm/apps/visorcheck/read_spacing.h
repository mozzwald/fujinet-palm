#ifndef VISOR_READ_SPACING_H
#define VISOR_READ_SPACING_H
enum { VC_SPACE_TICK, VC_SPACE_100US, VC_SPACE_10US, VC_SPACE_1US,
       VC_SPACE_NONE, VC_SPACE_COUNT };
enum { VC_CAPTURE_GUARDED, VC_CAPTURE_BURST, VC_CAPTURE_COUNT };
#define VC_BURST_WORDS 64UL
unsigned long vc_spacing_iterations(unsigned long mode);
unsigned long vc_spacing_us(unsigned long mode);
unsigned long vc_capture_words(unsigned long profile);
int vc_spacing_valid(unsigned long mode, unsigned long profile,
                     unsigned long iterations, unsigned long batch_words);
const char *vc_spacing_label(unsigned long mode);
const char *vc_capture_label(unsigned long profile);
#endif
