#ifndef VISOR_IMAGE_READ_H
#define VISOR_IMAGE_READ_H
#include "read_test.h"

struct visor_image_read {
  struct visor_read_result guard;
  unsigned long length, completed, crc;
  const unsigned char *expected;
  unsigned long mismatch_count;
  struct {
    unsigned long offset;
    unsigned short expected, actual;
  } first[8];
};
struct visor_image_read visor_image_begin(const struct visor_read_ops *ops,
    unsigned short card, unsigned long length, const unsigned char *expected);
unsigned long visor_image_local_crc(const unsigned char *data, unsigned long length);
/* Exactly one guarded 16-bit read, in increasing byte order. Caller supplies
 * spacing between steps; no hidden burst reads or local-resource substitution. */
void visor_image_step(struct visor_image_read *state,
    const struct visor_read_ops *ops, unsigned short card);
#endif
