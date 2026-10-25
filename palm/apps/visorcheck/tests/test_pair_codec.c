#include "pair_codec.h"
#include "image_read.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
int main(void) {
  struct vp_result r={0};unsigned long n,i;
  unsigned char *data=malloc(40000),*record=malloc(40164);assert(data&&record);
  r.sequence=1;r.offset=8;r.expected=0xfeedbeefUL;r.requested=10000;
  n=vp_encode(record,40164,&r,0);assert(n==164 && vc_get32(record+76)==0);
  vp_accept(&r,data,0xfeedbeefUL);vp_accept(&r,data,0xfeed0000UL);
  vp_accept(&r,data,0x0000beefUL);vp_accept(&r,data,0x00000000UL);
  assert(r.first_bad==2 && r.second_bad==2 && r.long_bad==3);
  for(i=4;i<10000;++i)vp_accept(&r,data,0xfeedbeefUL);
  n=vp_encode(record,40164,&r,data);assert(n==40164);
  assert(!memcmp(record,"VPL1",4));assert(vc_get32(record+76)==10000);
  assert(vc_get32(record+80)==2 && vc_get32(record+84)==2 && vc_get32(record+88)==3);
  assert(vc_get32(record+VP_HEADER+4)==0xfeed0000UL);
  assert(vc_get32(record+n-4)==visor_image_local_crc(record,n-4));
  assert(vp_encode(record,40163,&r,data)==0);
  r.requested=10001;assert(vp_encode(record,40164,&r,data)==0);
  r.requested=9999;assert(vp_encode(record,40164,&r,data)==0);
  r.requested=10000;r.offset=10;assert(vp_encode(record,40164,&r,data)==0);
  r.offset=8;assert(vp_encode(record,40164,&r,0)==0);
  free(data);free(record);puts("Paired record: both halves, complete 10,000-value retention and bounds PASS");return 0;
}
