#include "image_read.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake { unsigned reads, remove_after, fail_after, change_after; };
static int byte(void *p, unsigned short card, unsigned short attr, unsigned char *v) {
  struct fake *f = p;
  assert(card == 1);
  *v = attr == visorCardReset ? 0 : 1;
  if (attr == visorCardHwInstalled && f->remove_after && f->reads >= f->remove_after) *v = 0;
  return 0;
}
static int wide(void *p, unsigned short card, unsigned short attr, unsigned long *v) {
  struct fake *f = p;
  assert(card == 1);
  *v = attr == visorCardCsBase ? 0x28000000UL : 0x01000000UL;
  if (attr == visorCardCsBase && f->change_after && f->reads >= f->change_after) *v += 2;
  return 0;
}
static int word(void *p, unsigned long address, unsigned short *v) {
  struct fake *f = p;
  unsigned n = f->reads * 2;
  assert(address == 0x28000000UL + n);
  if (f->fail_after && f->reads == f->fail_after) return 17;
  *v = (unsigned short)(((n & 255) << 8) | ((n + 1) & 255));
  ++f->reads; return 0;
}
int main(void) {
  struct fake f = {0};
  struct visor_read_ops ops = {byte, wide, word, &f};
  struct visor_image_read s;
  unsigned i;
  const unsigned long bad[] = {0,1,257,65538};
  for (i=0; i<sizeof(bad)/sizeof(bad[0]); ++i) {
    s = visor_image_begin(&ops, 1, bad[i], 0);
    visor_image_step(&s, &ops, 1);
    assert(s.guard.reason == visorReadBadMapping && f.reads == 0);
  }
  s = visor_image_begin(&ops, 0, 256, 0);
  assert(s.guard.reason == visorReadInvalidSlot && f.reads == 0);
  s = visor_image_begin(&ops, 1, 256, 0);
  for (i=0; i<128; ++i) visor_image_step(&s, &ops, 1);
  assert(s.guard.reason == visorReadReady && s.completed == 256);
  assert((s.crc ^ 0xffffffffUL) == 0x29058c73UL);
  visor_image_step(&s, &ops, 1); assert(f.reads == 128);
  memset(&f, 0, sizeof(f));
  s = visor_image_begin(&ops, 1, 65536, 0);
  while (s.completed < s.length) visor_image_step(&s, &ops, 1);
  assert(s.completed == 65536 && f.reads == 32768);
  assert((s.crc ^ 0xffffffffUL) == 0xb11de6a1UL);
  for (i=0; i<3; ++i) {
    memset(&f, 0, sizeof(f));
    if (i==0) f.remove_after=3;
    if (i==1) f.fail_after=3;
    if (i==2) f.change_after=3;
    s = visor_image_begin(&ops, 1, 256, 0);
    while (s.guard.reason == visorReadReady && s.completed < s.length)
      visor_image_step(&s, &ops, 1);
    assert(s.completed == 6 && f.reads == 3);
    assert(s.guard.reason == (i==0 ? visorReadNotInstalled :
           i==1 ? visorReadAccessFailed : visorReadMappingChanged));
    visor_image_step(&s, &ops, 1); assert(f.reads == 3);
  }
  {
    unsigned char reference[256];
    for (i=0; i<256; ++i) reference[i] = (unsigned char)i;
    assert(visor_image_local_crc(reference, 256) == 0x29058c73UL);
    /* Wrong reference must not change the CRC of actual bus reads. */
    reference[10] ^= 1;
    memset(&f, 0, sizeof(f));
    s = visor_image_begin(&ops, 1, 256, reference);
    while (s.completed < s.length) visor_image_step(&s, &ops, 1);
    assert(s.mismatch_count == 1 && s.first[0].offset == 10);
    assert(s.first[0].actual == 0x0a0b && s.first[0].expected == 0x0b0b);
    assert((s.crc ^ 0xffffffffUL) == 0x29058c73UL);
    for (i=0; i<256; ++i) reference[i] = 0;
    memset(&f, 0, sizeof(f));
    s = visor_image_begin(&ops, 1, 256, reference);
    while (s.completed < s.length) visor_image_step(&s, &ops, 1);
    assert(s.mismatch_count == 128);
    for (i=0; i<8; ++i) assert(s.first[i].offset == i*2);
  }
  puts("Full-image guarded read, CRC and mismatch diagnostics PASS");
  return 0;
}
