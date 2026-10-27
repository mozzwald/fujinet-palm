"""Compile the actual guard body against read-only fake SDK attributes.

Checks the production guard without a Palm SDK, and verifies every rejection
keeps its specific diagnostic detail. No guard is bypassed for a passing test.
"""
import subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
class Guard(unittest.TestCase):
 def test_actual_guard_admission_and_rejections(self):
  source=(ROOT/'palm/apps/cs1stagea/stagea.c').read_text()
  guard=source[source.index('static Err guard('):source.index('static Err snapshot(')]
  shim=r'''
#include <stdint.h>
#include <assert.h>
#include <string.h>
typedef uint32_t UInt32; typedef uint16_t UInt16; typedef uint8_t UInt8; typedef uint16_t Err;
static UInt32 attrs[16]; static int bad=-1; static unsigned calls; static Err identity_error;
static Err identity(UInt16 *card,UInt32 *gen){*card=1;*gen=7;return identity_error;}
static Err VisorCardAttrGet(UInt16 card,UInt16 attr,void *p){assert(card==1);assert(attr<16);++calls;if((int)attr==bad)return 0x1234;
 if(attr==3||attr==4)*(UInt32*)p=attrs[attr];else *(UInt8*)p=(UInt8)attrs[attr];return 0;}
'''
  cases=r'''
static void init(UInt32 *r){memset(r,0,128);memset(attrs,0,sizeof attrs);attrs[0]=attrs[1]=attrs[2]=attrs[15]=1;attrs[3]=r[6]=0x28000000;attrs[4]=r[7]=0x01000000;r[4]=1;r[5]=7;bad=-1;calls=0;identity_error=0;}
int main(void){UInt32 r[32];unsigned a;unsigned checks[]={0,1,2,6,15,3,4};
 init(r);assert(!guard(r));assert(calls==7);assert(!r[29]);
 for(a=0;a<5;++a){unsigned attr=checks[a];init(r);attrs[attr]=(attr==6);assert(guard(r)==2);assert(r[29]==(0x40000000u|(attr<<16)|attrs[attr]));}
 for(a=0;a<7;++a){unsigned attr=checks[a];init(r);bad=attr;assert(guard(r)==0x1234);assert(r[29]==(0x80000000u|(attr<<16)|0x1234));}
 init(r);r[5]=8;assert(guard(r)==2);assert(r[29]==0x20000000u);assert(!calls);
 init(r);attrs[3]+=2;assert(guard(r)==2);assert(r[29]==0x10000000u);
 init(r);attrs[4]/=2;assert(guard(r)==2);assert(r[29]==0x10000000u);
 init(r);identity_error=1;assert(guard(r)==1);assert(!calls);
 return 0;}
'''
  with tempfile.TemporaryDirectory() as t:
   p=Path(t);(p/'guard.c').write_text(shim+guard+cases)
   subprocess.run(['cc','-std=c99','-O2','-Wall','-Wextra','-Werror',str(p/'guard.c'),'-o',str(p/'guard')],check=True,capture_output=True)
   subprocess.run([str(p/'guard')],check=True,capture_output=True)
if __name__=='__main__':unittest.main()
