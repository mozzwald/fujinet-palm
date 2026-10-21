#ifndef VISOR_TIMED_READ_H
#define VISOR_TIMED_READ_H
#include "image_read.h"
struct visor_timing_ops {
  const struct visor_read_ops *bus;
  int (*set_time)(void *, unsigned short, unsigned long);
  /* Durable journal: nonzero original before changing timing, zero only after
   * verified restoration. A journal failure must prevent further tests. */
  int (*journal)(void *, unsigned long);
};
struct visor_timed_read {
  struct visor_image_read image;
  unsigned long original, requested, applied, restored;
  int error, restore_error;
  unsigned char active, pending, blocked, unsafe;
};
void visor_timed_begin(struct visor_timed_read *, const struct visor_timing_ops *,
    unsigned long length, const unsigned char *expected, int slower);
void visor_timed_step(struct visor_timed_read *, const struct visor_timing_ops *);
/* Shared cleanup for completion, error, Cancel and app exit. */
void visor_timed_finish(struct visor_timed_read *, const struct visor_timing_ops *);
#endif
