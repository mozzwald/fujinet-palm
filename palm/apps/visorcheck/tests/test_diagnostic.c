#include "diagnostic.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
  unsigned char rom[256],scratch[130];unsigned long n=0;
  for(unsigned i=0;i<256;++i)rom[i]=(unsigned char)i;
  for(unsigned kind=1;kind<=6;++kind) {
    scratch[128]=0x12;scratch[129]=0x34;
    const unsigned char *p=vc_expected(kind,rom,sizeof(rom),scratch,128,&n);
    assert(p && n==(kind>=4?128:256));
    for(unsigned i=0;i<n;++i)assert(p[i]==(kind==4?0:kind==5?255:rom[i]));
    assert(scratch[128]==0x12 && scratch[129]==0x34);
  }
  assert(!vc_expected(4,rom,128,scratch,127,&n));
  assert(!vc_expected(5,rom,126,scratch,128,&n));
  assert(!vc_expected(6,rom,126,scratch,128,&n));
  assert(!vc_expected(7,rom,256,scratch,128,&n));
  puts("Diagnostic patterns:64 guarded words, zero/ones/prefix, bounds and reference selection PASS");
}
