#include "pair_codec.h"
#include "image_read.h"
#include <string.h>
void vp_accept(struct vp_result *r,unsigned char *data,unsigned long value) {
  vc_put32(data+4*r->completed,value);
  if((value>>16)!=(r->expected>>16))++r->first_bad;
  if((value&0xffff)!=(r->expected&0xffff))++r->second_bad;
  if(value!=r->expected)++r->long_bad;
  ++r->completed;
}
unsigned long vp_encode(unsigned char *p,unsigned long capacity,
    const struct vp_result *r,const unsigned char *data) {
  unsigned long bytes=r->completed*4,n=VP_HEADER+bytes+4,i;
  const unsigned long fields[]={r->sequence,r->status,r->offset,r->expected,
    r->requested,r->completed,r->first_bad,r->second_bad,r->long_bad,r->base,
    r->size,r->before,r->after,r->reason,r->error,r->started,r->ended,
    0,r->reference_crc,
#ifdef VC_WORDPAIR_DIAGNOSTIC
    0x800,n,VP_HEADER,2,1,1,r->nops};
#else
    0x700,n,VP_HEADER,1,1,0,0};
#endif
  if(!r->requested || r->requested>VP_MAX_PAIRS || r->completed>r->requested ||
     (r->offset&3) || (bytes&&!data) || n>capacity || n>65535 ||
     r->first_bad>r->completed || r->second_bad>r->completed ||
     r->long_bad>r->completed)return 0;
#ifdef VC_WORDPAIR_DIAGNOSTIC
  if(r->requested!=4 || (r->nops!=0 && r->nops!=1 && r->nops!=2 && r->nops!=4 &&
      r->nops!=8 && r->nops!=16 && r->nops!=32 && r->nops!=64))return 0;
#endif
  memset(p,0,(unsigned)n);
#ifdef VC_WORDPAIR_DIAGNOSTIC
  memcpy(p,"VPW1",4);
#else
  memcpy(p,"VPL1",4);
#endif
  vc_put32(p+4,1);
  memcpy(p+8,r->session,16);memcpy(p+24,r->sha,32);
  for(i=0;i<26;++i)vc_put32(p+56+4*i,fields[i]);
  vc_put32(p+124,visor_image_local_crc(data,bytes));
  if(bytes)memcpy(p+VP_HEADER,data,(unsigned)bytes);
  vc_put32(p+n-4,visor_image_local_crc(p,n-4));return n;
}
