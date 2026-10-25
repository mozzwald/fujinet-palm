#include "pair_codec.h"
#include "image_read.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
  struct vp_result r={0};unsigned char data[16],record[180];unsigned i;
  const unsigned long spacings[]={0,1,2,4,8,16,32,64};
  r.sequence=1;r.offset=8;r.requested=4;r.expected=0xfeedbeefUL;
  vp_accept(&r,data,0xfeedbeefUL);vp_accept(&r,data,0xfeedfeedUL);
  vp_accept(&r,data,0xbeefbeefUL);vp_accept(&r,data,0);
  assert(r.first_bad==2 && r.second_bad==2 && r.long_bad==3);
  for(i=0;i<8;++i) {
    r.nops=spacings[i];assert(vp_encode(record,sizeof(record),&r,data)==180);
    assert(!memcmp(record,"VPW1",4));assert(vc_get32(record+132)==0x800);
    assert(vc_get32(record+144)==2 && vc_get32(record+148)==1);
    assert(vc_get32(record+152)==1 && vc_get32(record+156)==spacings[i]);
    assert(vc_get32(record+164)==0xfeedfeedUL);
    assert(vc_get32(record+176)==visor_image_local_crc(record,176));
  }
  r.nops=3;assert(!vp_encode(record,sizeof(record),&r,data));
  r.nops=64;r.requested=100;assert(!vp_encode(record,sizeof(record),&r,data));
  puts("Word pairs: distinct envelope, exact NOP metadata, both halves and bounded four-value retention PASS");
  return 0;
}
