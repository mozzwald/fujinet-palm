#ifndef VISOR_DIAGNOSTIC_H
#define VISOR_DIAGNOSTIC_H
#include "result_codec.h"
/* The original ROM SHA/config remains the session identity. Test kind selects
 * the independently checked expected payload without replacing control/history. */
const unsigned char *vc_expected(unsigned long kind, const unsigned char *rom,
    unsigned long rom_size, unsigned char *scratch, unsigned long capacity,
    unsigned long *length);

#endif
