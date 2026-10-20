#include "reset_test.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {
  unsigned char values[7];
  int get_error, set_errors[3];
  unsigned calls, sets, delays;
  unsigned char writes[3];
  unsigned short get_attr;
  int stuck, readback_error;
};
static int get(void *p, unsigned short card, unsigned short attr,
               unsigned char *v) {
  struct fake *f = p;
  assert(card >= 1 && card <= 3);
  f->calls++;
  if (f->readback_error && f->sets)
    return f->readback_error;
  if (f->get_error && attr == f->get_attr)
    return f->get_error;
  *v = f->values[attr];
  return 0;
}
static int set(void *p, unsigned short card, unsigned short attr,
               unsigned char *v) {
  struct fake *f = p;
  assert(card == 1 && attr == visorCardReset);
  assert(f->sets < 3);
  unsigned n = f->sets++;
  f->writes[n] = *v;
  if (!f->set_errors[n] && !(f->stuck && *v == 0))
    f->values[attr] = *v;
  return f->set_errors[n];
}
static void delay(void *p, unsigned long ms) {
  struct fake *f = p;
  assert(ms == 250 && f->sets == 1 && f->writes[0] == 1);
  f->delays++;
}
static void init(struct fake *f) {
  memset(f, 0, sizeof(*f));
  f->values[0] = f->values[1] = 1;
}
int main(void) {
  struct fake f;
  struct visor_reset_ops ops = {get, set, delay, &f};
  struct visor_reset_result r;
  unsigned i;
  init(&f);
  assert(visor_reset_pulse(&ops, 0).reason == visorResetInvalidSlot);
  assert(f.calls == 0 && f.sets == 0);
  assert(visor_reset_pulse(&ops, 4).reason == visorResetInvalidSlot);
  for (i = 0; i < 4; i++) {
    static const unsigned short attrs[] = {0, 1, 2, 6};
    init(&f);
    f.get_error = 17;
    f.get_attr = attrs[i];
    r = visor_reset_pulse(&ops, 1);
    assert(r.reason == visorResetQueryFailed && r.query_error == 17 &&
           f.sets == 0);
  }
  init(&f);
  f.values[0] = 0;
  assert(visor_reset_pulse(&ops, 1).reason == visorResetNotRemovable &&
         f.sets == 0);
  init(&f);
  f.values[1] = 0;
  assert(visor_reset_pulse(&ops, 1).reason == visorResetNotInstalled &&
         f.sets == 0);
  init(&f);
  f.values[2] = 1;
  assert(visor_reset_pulse(&ops, 1).reason == visorResetSoftwareInstalled &&
         f.sets == 0);
  init(&f);
  f.values[6] = 1;
  assert(visor_reset_pulse(&ops, 1).reason == visorResetAlreadyAsserted &&
         f.sets == 0);
  init(&f);
  r = visor_reset_pulse(&ops, 1);
  assert(r.reason == visorResetReady && f.delays == 1 && f.sets == 2 &&
         f.writes[0] == 1 && f.writes[1] == 0 && r.after == 0);
  init(&f);
  f.set_errors[0] = 9;
  r = visor_reset_pulse(&ops, 1);
  assert(r.reason == visorResetAssertFailed && f.sets == 2 && f.delays == 0 &&
         f.writes[1] == 0);
  init(&f);
  f.set_errors[1] = 9;
  r = visor_reset_pulse(&ops, 1);
  assert(r.reason == visorResetReady && f.sets == 3 && f.writes[2] == 0);
  init(&f);
  f.set_errors[1] = f.set_errors[2] = 9;
  r = visor_reset_pulse(&ops, 1);
  assert(r.reason == visorResetRestoreFailed && f.sets == 3 && r.after == 1);
  init(&f);
  f.readback_error = 23;
  r = visor_reset_pulse(&ops, 1);
  assert(r.reason == visorResetReadbackFailed && r.readback_error == 23 &&
         f.sets == 2);
  init(&f);
  f.stuck = 1;
  r = visor_reset_pulse(&ops, 1);
  assert(r.reason == visorResetNotReleased && r.after == 1 && f.sets == 2);
  puts("Reset guard, pulse ordering and restoration checks passed");
  return 0;
}
