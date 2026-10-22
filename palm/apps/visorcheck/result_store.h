#ifndef VISOR_RESULT_STORE_H
#define VISOR_RESULT_STORE_H
#include <PalmOS.h>
#include "result_codec.h"
/* Config is installed once per host session; records are append-only. */
Err vc_store_begin(struct vc_result *);
Err vc_store_append(const struct vc_result *, const unsigned char *, const unsigned char *);

/* Validate the unchanged session reference separately from a short test's
 * expected payload. STARTED and final records retain identical test metadata. */
Err vc_store_begin_reference(struct vc_result *, unsigned long reference_size,
                             unsigned long reference_crc);

#endif
