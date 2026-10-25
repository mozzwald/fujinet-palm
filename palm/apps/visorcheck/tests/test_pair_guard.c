#include "read_test.h"
#include <assert.h>
#include <stdio.h>
static int hw=1,sw=0,reset=0;
static int byte(void *ctx,unsigned short card,unsigned short attr,unsigned char *v) {
  (void)ctx;(void)card;
  if(attr==visorCardRemovable)*v=1;
  else if(attr==visorCardHwInstalled)*v=hw;
  else if(attr==visorCardSwInstalled)*v=sw;
  else if(attr==visorCardReset)*v=reset;
  else return 1;
  return 0;
}
static int wide(void *ctx,unsigned short card,unsigned short attr,unsigned long *v) {
  (void)ctx;(void)card;
  if(attr==visorCardCsBase)*v=0x28000000;
  else if(attr==visorCardCsSize)*v=0x100000;
  else return 1;
  return 0;
}
int main(void) {
  const struct visor_read_ops ops={byte,wide,0,0};
  assert(visor_read_check(&ops,1).reason==visorReadNotRegistered);
  assert(visor_read_check_physical(&ops,1).reason==visorReadReady);
  hw=0;assert(visor_read_check_physical(&ops,1).reason==visorReadNotInstalled);
  hw=1;reset=1;assert(visor_read_check_physical(&ops,1).reason==visorReadResetAsserted);
  reset=0;sw=1;assert(visor_read_check(&ops,1).reason==visorReadReady);
  assert(visor_read_check_physical(&ops,0).reason==visorReadInvalidSlot);
  puts("Unrecognized-ROM physical guard: presence/reset/mapping retained; original registered guard unchanged PASS");return 0;
}
