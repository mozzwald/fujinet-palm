#include "timed_read.h"
#include <string.h>
enum { timingRejected = 0x8201, timingUnsafe = 0x8202,
       timingRestoreMismatch = 0x8203 };
static int same_slot(struct visor_timed_read *s, const struct visor_timing_ops *o) {
  struct visor_read_result now = visor_read_check(o->bus, 1);
  if (now.reason != visorReadReady || now.base != s->image.guard.base ||
      now.size != s->image.guard.size) s->unsafe = 1;
  return !s->unsafe;
}
void visor_timed_finish(struct visor_timed_read *s, const struct visor_timing_ops *o) {
  int error, read_error;
  s->active = 0;
  if (!s->pending) return;
  /* Never write to a slot observed removed, reset, unregistered or remapped. */
  if (!same_slot(s, o)) { s->restore_error = timingUnsafe; s->blocked = 1; return; }
  error = o->set_time(o->bus->context, 1, s->original);
  read_error = o->bus->get_wide(o->bus->context, 1, visorCardAccessTime, &s->restored);
  s->restore_error = error ? error : read_error ? read_error :
      s->restored != s->original ? timingRestoreMismatch : 0;
  if (!s->restore_error) s->restore_error = o->journal(o->bus->context, 0);
  s->blocked = s->restore_error != 0;
  if (!s->blocked) s->pending = 0;
}
void visor_timed_begin(struct visor_timed_read *s, const struct visor_timing_ops *o,
    unsigned long length, const unsigned char *expected, int slower) {
  if (s->blocked || s->pending || s->active) return;
  memset(s, 0, sizeof(*s));
  s->image = visor_image_begin(o->bus, 1, length, expected);
  if (s->image.guard.reason != visorReadReady) return;
  s->error = o->bus->get_wide(o->bus->context, 1, visorCardAccessTime, &s->original);
  if (s->error) return;
  s->applied = s->original;
  if (slower) {
    s->requested = 390;
    if (!s->original || s->original >= s->requested) { s->error = timingRejected; return; }
    if (!same_slot(s, o)) { s->error = timingUnsafe; return; }
    s->error = o->journal(o->bus->context, s->original);
    if (s->error) { s->blocked = 1; return; }
    /* Set may change hardware even when it returns an error. */
    s->pending = 1;
    s->error = o->set_time(o->bus->context, 1, s->requested);
    if (!s->error) s->error = o->bus->get_wide(o->bus->context, 1,
        visorCardAccessTime, &s->applied);
    if (!s->error && s->applied < s->requested) s->error = timingRejected;
    if (s->error) { visor_timed_finish(s, o); return; }
  }
  s->active = 1;
}
void visor_timed_step(struct visor_timed_read *s, const struct visor_timing_ops *o) {
  if (!s->active) return;
  visor_image_step(&s->image, o->bus, 1);
  /* Latch removal/mapping changes before cleanup; no restore on a new card. */
  if (s->image.guard.reason != visorReadReady &&
      s->image.guard.reason != visorReadAccessFailed) s->unsafe = 1;
  if (s->image.guard.reason != visorReadReady || s->image.completed == s->image.length)
    visor_timed_finish(s, o);
}
