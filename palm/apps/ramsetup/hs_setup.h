#ifndef RP_HS_SETUP_H
#define RP_HS_SETUP_H
#include <PalmOS.h>
#include <stddef.h>
/* Audited HsExt/HsCreators ABI, project-owned narrow declarations. */
typedef struct {UInt16 cardNo;UInt8 isReset,reserved;UInt16 setupCardNo;LocalID setupDbID;} RPInstallParams;
typedef struct {UInt16 cardNo;} RPRemoveParams;
typedef char rp_install_size[(sizeof(RPInstallParams)==10)?1:-1];
typedef char rp_install_offset[(offsetof(RPInstallParams,setupDbID)==6)?1:-1];
typedef char rp_remove_size[(sizeof(RPRemoveParams)==2)?1:-1];
Err RPDatabaseCopy(UInt16 srcCard,LocalID source,UInt16 dstCard,Char *name,
 UInt32 flags,Char *tempName,LocalID *out)
 __attribute__((__callseq__("move.w #4,-(%%sp); trap #15; dc.w 0xA349")));
#endif
