#ifndef VISOR_RESULT_STORE_H
#define VISOR_RESULT_STORE_H
#include <PalmOS.h>
#include "result_codec.h"
/* Config is installed once per host session; records are append-only. */
Err vc_store_begin(struct vc_result *);
Err vc_store_append(const struct vc_result *, const unsigned char *, const unsigned char *);
#endif
