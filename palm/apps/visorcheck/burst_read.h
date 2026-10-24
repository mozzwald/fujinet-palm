#ifndef VISOR_BURST_READ_H
#define VISOR_BURST_READ_H
#include "timed_read.h"
#include "read_spacing.h"
/* Capture callback must report only words actually read/stored, even on
 * removal recovery. Maximum64words. No reference data enters this callback. */
struct vc_burst_ops {
  int (*capture)(void *, unsigned long address, unsigned char *destination,
                 unsigned short words, unsigned long spacing,
                 unsigned long iterations, unsigned short *completed);
  void *context;
};
void vc_burst_step(struct visor_timed_read *, const struct visor_timing_ops *,
                   const struct vc_burst_ops *, unsigned char *captured,
                   unsigned long capacity, unsigned long spacing);
#endif
