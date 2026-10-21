#include "result_codec.h"
#include "image_read.h"
#include <string.h>
unsigned long vc_get32(const unsigned char *p) {
  return ((unsigned long)p[0]<<24)|((unsigned long)p[1]<<16)|((unsigned long)p[2]<<8)|p[3];
}
void vc_put32(unsigned char *p, unsigned long v) {
  p[0]=(unsigned char)(v>>24); p[1]=(unsigned char)(v>>16);
  p[2]=(unsigned char)(v>>8); p[3]=(unsigned char)v;
}
int vc_config(const unsigned char *p, unsigned long n, unsigned char *session,
    unsigned char *sha, unsigned long size, unsigned long crc) {
  unsigned i, nonzero=0;
  if (n!=VC_CONFIG_SIZE || memcmp(p,"VCC1",4) || vc_get32(p+4)!=1 ||
      vc_get32(p+24)!=size || vc_get32(p+28)!=crc ||
      vc_get32(p+64)!=visor_image_local_crc(p,64)) return 0;
  for(i=0;i<16;++i) nonzero |= p[8+i];
  if(!nonzero) return 0;
  memcpy(session,p+8,16); memcpy(sha,p+32,32); return 1;
}
unsigned long vc_encode(unsigned char *p, unsigned long capacity,
    const struct vc_result *r, const unsigned char *actual, const unsigned char *expected) {
  unsigned long i, count=0, at, n;
  const unsigned long fields[] = {r->sequence,r->kind,r->status,r->expected_size,
    r->expected_crc,r->reference_crc,r->completed,r->crc,0,r->reason,r->error,
    r->original,r->requested,r->applied,r->restored,r->setup_error,r->restore_error,
    r->started,r->ended,VC_VERSION};
  if(r->expected_size>VC_MAX_IMAGE || (r->completed&1) || r->completed>r->expected_size ||
     (r->completed && (!actual || !expected))) return 0;
  for(i=0;i<r->completed;i+=2)
    if(actual[i]!=expected[i] || actual[i+1]!=expected[i+1]) ++count;
  n=VC_HEADER+r->completed+count*8+4;
  if(n>capacity || n>65535) return 0;
  memset(p,0,(unsigned)n); memcpy(p,"VCR1",4);
  vc_put32(p+4,1); memcpy(p+8,r->session,16); memcpy(p+24,r->sha,32);
  for(i=0;i<20;++i) vc_put32(p+56+i*4,fields[i]);
  vc_put32(p+88,count); vc_put32(p+136,n); vc_put32(p+140,VC_HEADER);
  if(r->completed) memcpy(p+VC_HEADER,actual,(unsigned)r->completed);
  at=VC_HEADER+r->completed;
  for(i=0;i<r->completed;i+=2) {
    if(actual[i]!=expected[i] || actual[i+1]!=expected[i+1]) {
      vc_put32(p+at,i); p[at+4]=expected[i]; p[at+5]=expected[i+1];
      p[at+6]=actual[i]; p[at+7]=actual[i+1]; at+=8;
    }
  }
  vc_put32(p+n-4,visor_image_local_crc(p,n-4)); return n;
}
