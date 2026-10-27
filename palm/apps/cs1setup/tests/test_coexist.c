/* Run the unchanged adverse-path suite and preserve the older A diagnostic. */
#define main rp_original_policy_tests
#include "../../ramsetup/tests/test_policy.c"
#undef main
int main(int argc,char **argv) {
 struct world w; struct rp_ops o; struct rp_record r; struct rp_info previous;
 assert(RP_CREATOR==0x464e4d43UL);
 init(&w,&o,&r);w.exists[3]=1;
 info(&w.db[3],"CS1 Module A",RP_APP_TYPE,0x464e4d42UL);previous=w.db[3];
 assert(!rp_install(&o,&r,1,0x1010));
 assert(w.exists[3]&&!memcmp(&previous,&w.db[3],sizeof previous));
 assert(w.copies==1&&w.publishes==1&&!w.erases);
 return rp_original_policy_tests(argc,argv);
}
