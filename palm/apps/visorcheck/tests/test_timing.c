#include "timed_read.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {
  unsigned long time, saved;
  unsigned reads, sets, journals;
  int missing, unregistered, reset, remapped, read_error, corrupt;
  int set_error, get_error, ignore_set, restore_error, journal_error;
  int original_error, restore_get_error, clear_error, round_up, internal;
};
static int byte(void *p, unsigned short card, unsigned short attr, unsigned char *v) {
  struct fake *f = p; assert(card == 1);
  *v = attr == visorCardRemovable ? !f->internal : attr == visorCardSwInstalled ? !f->unregistered : attr == visorCardReset ? f->reset :
       attr == visorCardHwInstalled ? !f->missing : 1;
  return 0;
}
static int wide(void *p, unsigned short card, unsigned short attr, unsigned long *v) {
  struct fake *f = p; assert(card == 1);
  if (attr == visorCardAccessTime) {
    if ((f->get_error && f->sets == 1) ||
        (f->original_error && !f->sets) ||
        (f->restore_get_error && f->sets > 1)) return 31;
    *v = f->time;
  } else *v = attr == visorCardCsBase ? 0x28000000UL + (f->remapped ? 2 : 0) : 0x1000000UL;
  return 0;
}
static int word(void *p, unsigned long address, unsigned short *v) {
  struct fake *f = p;
  assert(address == 0x28000000UL + 2*f->reads);
  if (f->read_error) return 32;
  *v = f->corrupt ? 0xffff : 0;
  ++f->reads; return 0;
}
static int set(void *p, unsigned short card, unsigned long value) {
  struct fake *f = p; assert(card == 1);
  ++f->sets;
  if (!f->ignore_set) f->time = value == 390 && f->round_up ? 400 : value;
  return f->sets == 1 ? f->set_error : f->restore_error;
}
static int journal(void *p, unsigned long value) {
  struct fake *f = p; ++f->journals;
  if (f->journal_error || (!value && f->clear_error)) return 33;
  f->saved = value; return 0;
}
static void reset(struct fake *f, struct visor_timed_read *s) {
  memset(f, 0, sizeof(*f)); memset(s, 0, sizeof(*s)); f->time = 330;
}
int main(void) {
  struct fake f;
  struct visor_timed_read s;
  struct visor_read_ops bus = {byte, wide, word, &f};
  struct visor_timing_ops ops = {&bus, set, journal};
  const unsigned char expected[16] = {0};
  unsigned i;
  reset(&f, &s);
  visor_timed_begin(&s, &ops, 16, expected, 0);
  while (s.active) visor_timed_step(&s, &ops);
  assert(f.reads == 8 && !f.sets && !f.journals && !s.blocked);
  /* Both successful CRC and corrupt data restore before the caller sees completion. */
  for (i=0; i<2; ++i) {
    reset(&f, &s); f.corrupt=i;
    visor_timed_begin(&s, &ops, 16, expected, 1);
    assert(s.active && f.time == 390 && f.saved == 330 && s.applied == 390);
    while (s.active) visor_timed_step(&s, &ops);
    assert(f.time == 330 && !f.saved && s.restored == 330 && !s.blocked);
    assert(s.image.mismatch_count == (i ? 8 : 0));
  }
  /* Cancel and app exit use the same cleanup, including before the first word. */
  for (i=0; i<2; ++i) {
    reset(&f, &s); visor_timed_begin(&s, &ops, 16, expected, 1);
    if (i) visor_timed_step(&s, &ops);
    visor_timed_finish(&s, &ops); visor_timed_finish(&s, &ops);
    assert(!s.active && !s.pending && f.sets == 2 && !f.saved && f.time == 330);
  }
  reset(&f, &s); visor_timed_begin(&s, &ops, 16, expected, 1);
  f.read_error=1; visor_timed_step(&s, &ops);
  assert(!s.active && !s.pending && f.time == 330 && s.image.guard.error == 32);
  for (i=0; i<3; ++i) {
    reset(&f, &s);
    if (i==0) f.set_error=34; /* hardware changed despite API error */
    if (i==1) f.get_error=1;
    if (i==2) f.ignore_set=1;
    visor_timed_begin(&s, &ops, 16, expected, 1);
    assert(!s.active && s.error && !s.pending && !s.blocked);
    assert(!f.reads && !f.saved && f.time == 330 && f.sets == 2);
  }
  /* No restoration writes to removed/replaced, unregistered, reset or remapped slots. */
  for (i=0; i<4; ++i) {
    unsigned sets;
    reset(&f, &s); visor_timed_begin(&s, &ops, 16, expected, 1);
    if (i==0) f.missing=1;
    if (i==1) f.unregistered=1;
    if (i==2) f.reset=1;
    if (i==3) f.remapped=1;
    visor_timed_step(&s, &ops);
    assert(s.blocked && s.pending && f.saved == 330 && f.sets == 1 && !f.reads);
    f.missing=f.unregistered=f.reset=f.remapped=0;
    visor_timed_finish(&s, &ops); /* unsafe state remains latched after reseating */
    sets=f.sets; visor_timed_begin(&s, &ops, 16, expected, 0);
    assert(s.blocked && f.sets == sets && !s.active);
  }
  reset(&f, &s); f.journal_error=1;
  visor_timed_begin(&s, &ops, 16, expected, 1);
  assert(s.blocked && !f.sets && !f.reads);
  reset(&f, &s); visor_timed_begin(&s, &ops, 16, expected, 1);
  f.restore_error=35; visor_timed_finish(&s, &ops);
  assert(s.blocked && s.pending && f.saved == 330 && s.restore_error == 35);
  reset(&f, &s); visor_timed_begin(&s, &ops, 16, expected, 1);
  f.ignore_set=1; visor_timed_finish(&s, &ops);
  assert(s.blocked && s.pending && s.restored == 390);
  reset(&f, &s); f.time=390;
  visor_timed_begin(&s, &ops, 16, expected, 1);
  assert(!s.active && s.error && !f.sets);
  for (i=0; i<4; ++i) {
    reset(&f, &s);
    if (i==0) f.original_error=1;
    if (i==1) f.internal=1;
    if (i==2) f.unregistered=1;
    if (i==3) f.missing=1;
    visor_timed_begin(&s, &ops, 16, expected, 1);
    assert(!s.active && !f.sets && !f.reads && !f.journals);
  }
  reset(&f, &s); f.round_up=1;
  visor_timed_begin(&s, &ops, 16, expected, 1);
  assert(s.active && s.applied == 400);
  visor_timed_finish(&s, &ops);
  assert(!s.blocked && s.restored == 330);
  for (i=0; i<2; ++i) {
    reset(&f, &s); visor_timed_begin(&s, &ops, 16, expected, 1);
    if (i==0) f.restore_get_error=1;
    if (i==1) f.clear_error=1;
    visor_timed_finish(&s, &ops);
    assert(s.blocked && s.pending && s.restore_error && f.saved == 330);
  }
  puts("Timing lifecycle: baseline, CRC pass/fail, cancellation/exit, partial setup, removal and restoration failures PASS");
  return 0;
}
