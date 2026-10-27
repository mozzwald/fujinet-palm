#include "policy.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct world {
 struct rp_info db[5];int exists[5],copies,publishes,erases,saves;
 int copy_error,copy_partial,copy_alias,publish_error,bad_publish,erase_error;
 int save_error_at,inspect_error_id;struct rp_record last;
};
static void info(struct rp_info *i,const char *name,RPU32 type,RPU32 creator) {
 memset(i,0,sizeof(*i));strcpy(i->name,name);i->type=type;i->creator=creator;
 i->attrs=1;i->version=1;i->card=0;i->ram=1;i->base=0x1000;i->size=0x100;
 i->heap=1;i->crc=0xa55a;
}
static int inspect(void *ctx,RPU32 id,struct rp_info *i) {
 struct world *w=ctx;assert(id<5);
 if(!w->exists[id]||(int)id==w->inspect_error_id)return 73;
 *i=w->db[id];i->id=id;return 0;
}
static RPU32 find(void *ctx,const char *name) {
 struct world *w=ctx;unsigned i;for(i=1;i<5;++i)if(w->exists[i]&&!strcmp(w->db[i].name,name))return i;return 0;
}
static int copy(void *ctx,RPU32 source,RPU32 *dest) {
 struct world *w=ctx;assert(source==1);++w->copies;
 if(w->copy_alias){*dest=(RPU32)w->copy_alias;return 0;}
 if(w->copy_error&&!w->copy_partial)return w->copy_error;
 assert(!find(ctx,RP_TEMP_NAME));w->db[2]=w->db[1];strcpy(w->db[2].name,RP_TEMP_NAME);
 w->db[2].base=0x2000;w->exists[2]=1;*dest=2;return w->copy_error;
}
static int publish(void *ctx,RPU32 id) {
 struct world *w=ctx;assert(id==2);++w->publishes;
 strcpy(w->db[id].name,RP_RUNTIME_NAME);w->db[id].type=RP_APP_TYPE;w->db[id].creator=RP_CREATOR;
 w->db[id].attrs=w->bad_publish?0x101:9;return w->publish_error;
}
static int erase(void *ctx,RPU32 id) {
 struct world *w=ctx;assert(id==2);++w->erases;if(w->erase_error)return w->erase_error;
 w->exists[id]=0;return 0;
}
static int save(void *ctx,const struct rp_record *r) {
 struct world *w=ctx;++w->saves;if(w->saves==w->save_error_at)return 77;w->last=*r;return 0;
}
static void init(struct world *w,struct rp_ops *o,struct rp_record *r) {
 memset(w,0,sizeof(*w));info(&w->db[1],RP_SETUP_NAME,RP_SETUP_TYPE,RP_SETUP_CREATOR);w->exists[1]=1;
 o->inspect=inspect;o->find=find;o->copy=copy;o->publish=publish;o->erase=erase;o->save=save;o->ctx=w;
 rp_clear(r);r->v[RP_MODULE]=1;r->v[RP_GENERATION]=1;
}
static void preserved(struct world *w) {
 assert(w->exists[1]&&w->db[1].type==RP_SETUP_TYPE&&!strcmp(w->db[1].name,RP_SETUP_NAME));
}
int main(int argc,char **argv) {
 struct world w;struct rp_ops o;struct rp_record r,decoded;unsigned char b[128];unsigned i;
 init(&w,&o,&r);assert(!rp_install(&o,&r,1,0x1010));assert(w.copies==1&&w.publishes==1&&!w.erases);
 assert(r.v[RP_ACTIVE]&&r.v[RP_RUNTIME_DB]==2&&r.v[RP_STAGE]==7&&w.saves==2);preserved(&w);
 assert(!rp_install(&o,&r,1,0x1010));assert(w.copies==1&&w.publishes==1); /* idempotent */
 /* Physical Prism rename regression: source name is OS-owned. */
 init(&w,&o,&r);strcpy(w.db[1].name,"HsCardSetup.787D.9362");
 assert(!rp_install(&o,&r,1,0x1010));assert(w.copies==1&&w.publishes==1);
 init(&w,&o,&r);strcpy(w.db[1].name,"HsCardSetup.787D.9362");w.db[1].creator=0x42414421;
 assert(rp_install(&o,&r,1,0x1010)==RP_BAD_SOURCE);assert(!w.copies);
 assert(r.v[RP_SETUP_BASE]==0x1000&&r.v[RP_SOURCE_CRC]==0xa55a);
 init(&w,&o,&r);strcpy(w.db[1].name,"HsCardSetup.787D.9362");
 assert(rp_install(&o,&r,1,0x2000)==RP_BAD_SOURCE);assert(!w.copies);
 /* Resume the existing successful-reuse fixture. */
 init(&w,&o,&r);assert(!rp_install(&o,&r,1,0x1010));
 w.db[2].busy=1;assert(!rp_install(&o,&r,1,0x1010));assert(w.copies==1); /* reuse, never overwrite open app */
 w.db[2].crc^=1;assert(rp_install(&o,&r,1,0x1010)==RP_COLLISION);assert(!w.erases);
 init(&w,&o,&r);w.exists[3]=1;info(&w.db[3],RP_RUNTIME_NAME,RP_APP_TYPE,0x42414421);
 assert(rp_install(&o,&r,1,0x1010)==RP_COLLISION);assert(w.exists[3]&&!w.copies&&!w.erases);
 init(&w,&o,&r);w.exists[3]=1;info(&w.db[3],RP_RUNTIME_NAME,RP_APP_TYPE,RP_CREATOR);w.db[3].attrs|=0x100;
 assert(rp_install(&o,&r,1,0x1010)==RP_COLLISION);assert(!w.erases);
 init(&w,&o,&r);w.exists[2]=1;info(&w.db[2],RP_TEMP_NAME,0,0);
 assert(rp_install(&o,&r,1,0x1010)==RP_COLLISION);assert(!w.erases);
 init(&w,&o,&r);w.exists[2]=1;info(&w.db[2],RP_TEMP_NAME,RP_SETUP_TYPE,RP_SETUP_CREATOR);w.db[2].busy=1;
 assert(rp_install(&o,&r,1,0x1010)==RP_BUSY);assert(!w.erases);
 w.db[2].busy=0;assert(!rp_install(&o,&r,1,0x1010));assert(w.erases==1&&w.copies==1);preserved(&w);
 init(&w,&o,&r);w.copy_error=74;assert(rp_install(&o,&r,1,0x1010)==74);assert(!w.erases&&!r.v[RP_ACTIVE]);
 init(&w,&o,&r);w.copy_error=74;w.copy_partial=1;
 assert(rp_install(&o,&r,1,0x1010)==74);assert(w.erases==1&&!w.exists[2]);preserved(&w);
 init(&w,&o,&r);w.copy_alias=1;assert(rp_install(&o,&r,1,0x1010)==RP_ALIAS);assert(!w.erases);preserved(&w);
 init(&w,&o,&r);w.copy_alias=4;w.exists[4]=1;info(&w.db[4],"Other",RP_APP_TYPE,RP_CREATOR);
 assert(rp_install(&o,&r,1,0x1010)==RP_ALIAS);assert(w.exists[4]&&!w.erases);
 init(&w,&o,&r);w.publish_error=75;assert(rp_install(&o,&r,1,0x1010)==75);
 assert(w.erases==1&&!w.exists[2]);preserved(&w);
 init(&w,&o,&r);w.bad_publish=1;assert(rp_install(&o,&r,1,0x1010)==RP_BAD_CLONE);assert(w.erases==1);
 init(&w,&o,&r);w.inspect_error_id=2;assert(rp_install(&o,&r,1,0x1010)==73);assert(w.erases==1);
 init(&w,&o,&r);w.publish_error=75;w.erase_error=76;
 assert(rp_install(&o,&r,1,0x1010)==75);assert(r.v[RP_AUX_ERROR]==76&&!r.v[RP_ACTIVE]);
 init(&w,&o,&r);w.save_error_at=1;assert(rp_install(&o,&r,1,0x1010)==RP_LOG_ERROR);assert(!w.copies);
 init(&w,&o,&r);w.save_error_at=2;assert(rp_install(&o,&r,1,0x1010)==RP_LOG_ERROR);assert(!r.v[RP_ACTIVE]&&w.exists[2]);
 init(&w,&o,&r);w.db[1].ram=0;assert(rp_install(&o,&r,1,0x1010)==RP_BAD_SOURCE);assert(!w.copies);
 init(&w,&o,&r);w.db[1].heapflags=1;assert(rp_install(&o,&r,1,0x1010)==RP_BAD_SOURCE);
 init(&w,&o,&r);w.db[1].card=1;assert(rp_install(&o,&r,1,0x1010)==RP_BAD_SOURCE);
 init(&w,&o,&r);assert(rp_install(&o,&r,1,0x1100)==RP_BAD_SOURCE);assert(!w.copies);
 init(&w,&o,&r);w.db[1].base=0xfffffff0;w.db[1].size=0x40;assert(!rp_in_ram(&w.db[1],0xfffffff1));
 init(&w,&o,&r);assert(!rp_install(&o,&r,1,0x1010));
 assert(rp_remove(&o,&r,2)==RP_BAD_RECEIPT);assert(r.v[RP_ACTIVE]);
 assert(!rp_remove(&o,&r,1));assert(!r.v[RP_ACTIVE]&&r.v[RP_EVENT]==RP_REMOVE&&w.exists[2]);preserved(&w);
 for(i=3;i<RP_FIELDS;++i)r.v[i]=0x12340000u+i;
 rp_encode(&r,b);assert(!memcmp(b,"FRP2\0\0\0\1",8));assert(!rp_decode(&decoded,b,sizeof(b)));
 assert(!memcmp(&r,&decoded,sizeof(r)));assert(rp_decode(&decoded,b,127)==RP_BAD_RECEIPT);
 assert(argc<=2);if(argc==2){FILE *file=fopen(argv[1],"wb");assert(file);assert(fwrite(b,1,128,file)==128);assert(!fclose(file));}
 b[0]^=1;assert(rp_decode(&decoded,b,128)==RP_BAD_RECEIPT);
 puts("RAM installer policy PASS: success/idempotence, collision ownership, copy/convert/log failures, rollback, RAM bounds, removal retention and BE codec");return 0;
}
