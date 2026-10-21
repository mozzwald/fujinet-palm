#include "result_codec.h"
#include "image_read.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned char output[64000];
static void save(const char *directory,const char *name,unsigned long n) {
  char path[1024];FILE *f;
  assert(snprintf(path,sizeof(path),"%s/%s.bin",directory,name)>0);
  f=fopen(path,"wb");assert(f);assert(fwrite(output,1,n,f)==n);assert(!fclose(f));
}
int main(int argc,char **argv) {
  struct vc_result r;
  unsigned char expected[256],actual[256],config[68],session[16],sha[32];
  unsigned long n; unsigned i;
  memset(&r,0,sizeof(r));
  for(i=0;i<16;++i)r.session[i]=i;
  /* SHA256 of bytes(range(256)); independently checked by Python decoder. */
  { const unsigned char hash[32]={0x40,0xaf,0xf2,0xe9,0xd2,0xd8,0x92,0x2e,0x47,0xaf,0xd4,0x64,0x8e,0x69,0x67,0x49,0x71,0x58,0x78,0x5f,0xbd,0x1d,0xa8,0x70,0xe7,0x11,0x02,0x66,0xbf,0x94,0x48,0x80};memcpy(r.sha,hash,32); }
  for(i=0;i<256;++i)expected[i]=actual[i]=(unsigned char)i;
  r.sequence=1;r.kind=VC_SYNTH_PASS;r.expected_size=256;
  r.expected_crc=r.reference_crc=0x29058c73UL;r.started=1;
  n=vc_encode(output,sizeof(output),&r,0,0);assert(n==148);
  if(argc==2)save(argv[1],"start",n);
  r.status=VC_PASS;r.completed=256;r.crc=r.expected_crc;r.ended=2;
  n=vc_encode(output,sizeof(output),&r,actual,expected);assert(n==404);
  assert(vc_get32(output+88)==0);
  if(argc==2)save(argv[1],"pass",n);
  for(i=1;i<256;i+=2)actual[i]^=1;
  r.status=VC_FAIL;r.kind=VC_SYNTH_FAIL;r.crc=visor_image_local_crc(actual,256);
  n=vc_encode(output,sizeof(output),&r,actual,expected);assert(n==1428);
  assert(vc_get32(output+88)==128 && vc_get32(output+n-4)==visor_image_local_crc(output,n-4));
  if(argc==2)save(argv[1],"fail",n);
  assert(!vc_encode(output,100,&r,actual,expected));
  assert(!vc_encode(output,sizeof(output),&r,0,expected));
  r.status=VC_CANCELLED;r.completed=10;r.crc=visor_image_local_crc(actual,10);
  n=vc_encode(output,sizeof(output),&r,actual,expected);assert(n==198);
  if(argc==2)save(argv[1],"cancel",n);
  r.completed=11;assert(!vc_encode(output,sizeof(output),&r,actual,expected));
  r.completed=258;assert(!vc_encode(output,sizeof(output),&r,actual,expected));
  memset(config,0,sizeof(config));memcpy(config,"VCC1",4);vc_put32(config+4,1);
  config[8]=1;vc_put32(config+24,256);vc_put32(config+28,r.expected_crc);
  vc_put32(config+64,visor_image_local_crc(config,64));
  assert(vc_config(config,68,session,sha,256,r.expected_crc));
  assert(!vc_config(config,67,session,sha,256,r.expected_crc));
  config[64]^=1;assert(!vc_config(config,68,session,sha,256,r.expected_crc));
  puts("Result codec: explicit endian layout, full mismatches, partial capture, bounds and config integrity PASS");return 0;
}
