#include "read_test.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {
  unsigned char values[7];
  unsigned long base, size, addresses[8];
  unsigned reads, queries;
  unsigned short fail_attr;
  int query_error, read_error;
  unsigned fail_after, remove_after, change_after;
};
static int byte(void *p, unsigned short card, unsigned short attr,
                unsigned char *v) {
  struct fake *f = p;
  assert(card >= 1 && card <= 3);
  ++f->queries;
  if (f->query_error && attr == f->fail_attr) return f->query_error;
  *v = f->values[attr];
  if (f->remove_after && f->reads >= f->remove_after && attr == visorCardHwInstalled)
    *v = 0;
  return 0;
}
static int wide(void *p, unsigned short card, unsigned short attr,
                unsigned long *v) {
  struct fake *f = p;
  assert(card == 1);
  if (f->query_error && attr == f->fail_attr) return f->query_error;
  *v = attr == visorCardCsBase ? f->base : f->size;
  if (f->change_after && f->reads >= f->change_after && attr == visorCardCsBase)
    *v += 2;
  return 0;
}
static int read_word(void *p, unsigned long address, unsigned short *v) {
  struct fake *f = p;
  assert(f->reads < 8);
  f->addresses[f->reads] = address;
  if (f->read_error && f->reads == f->fail_after) return f->read_error;
  *v = (unsigned short)(0xa500 + f->reads++);
  return 0;
}
static void init(struct fake *f) {
  memset(f, 0, sizeof(*f));
  f->values[0] = f->values[1] = f->values[2] = 1;
  f->base = 0x28000000UL; f->size = 0x01000000UL;
}
int main(void) {
  struct fake f;
  struct visor_read_ops ops = {byte, wide, read_word, &f};
  struct visor_read_result r;
  unsigned i;
  init(&f);
  assert(visor_read_probe(&ops, 0).reason == visorReadInvalidSlot);
  assert(visor_read_probe(&ops, 4).reason == visorReadInvalidSlot);
  assert(f.queries == 0 && f.reads == 0);
  for (i = 0; i < 7; ++i) {
    if (i == visorCardAccessTime) continue;
    init(&f); f.fail_attr = i; f.query_error = 17;
    r = visor_read_probe(&ops, 1);
    assert(r.reason == visorReadQueryFailed && r.error == 17 && f.reads == 0);
  }
  for (i = 0; i < 3; ++i) {
    init(&f); f.values[i] = 0;
    r = visor_read_probe(&ops, 1);
    assert(r.reason != visorReadReady && f.reads == 0);
  }
  init(&f); f.values[visorCardReset] = 1;
  assert(visor_read_probe(&ops, 1).reason == visorReadResetAsserted && f.reads == 0);
  { const unsigned long bases[] = {0, 1, 0xfffffff2UL};
    for (i = 0; i < 3; ++i) {
      init(&f); f.base = bases[i];
      assert(visor_read_probe(&ops, 1).reason == visorReadBadMapping && f.reads == 0);
    }
  }
  { const unsigned long sizes[] = {0, 14, 0x01000001UL};
    for (i = 0; i < 3; ++i) {
      init(&f); f.size = sizes[i];
      assert(visor_read_probe(&ops, 1).reason == visorReadBadMapping && f.reads == 0);
    }
  }
  init(&f); f.size = 16;
  r = visor_read_probe(&ops, 1);
  assert(r.reason == visorReadReady && r.completed == 8 && f.reads == 8);
  for (i = 0; i < 8; ++i) {
    assert(f.addresses[i] == f.base + i * 2);
    assert(r.words[i] == 0xa500 + i);
  }
  init(&f); f.read_error = 23; f.fail_after = 3;
  r = visor_read_probe(&ops, 1);
  assert(r.reason == visorReadAccessFailed && r.error == 23 && r.completed == 3);
  init(&f); f.remove_after = 2;
  r = visor_read_probe(&ops, 1);
  assert(r.reason == visorReadNotInstalled && r.completed == 2 && f.reads == 2);
  init(&f); f.change_after = 1;
  r = visor_read_probe(&ops, 1);
  assert(r.reason == visorReadMappingChanged && r.completed == 1 && f.reads == 1);
  puts("Visor bounded read policy tests passed");
  return 0;
}
