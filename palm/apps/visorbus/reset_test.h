#ifndef VISOR_RESET_TEST_H
#define VISOR_RESET_TEST_H

enum {
  visorCardRemovable,
  visorCardHwInstalled,
  visorCardSwInstalled,
  visorCardCsBase,
  visorCardCsSize,
  visorCardAccessTime,
  visorCardReset
};
enum visor_reset_reason {
  visorResetReady,
  visorResetInvalidSlot,
  visorResetQueryFailed,
  visorResetNotRemovable,
  visorResetNotInstalled,
  visorResetSoftwareInstalled,
  visorResetAlreadyAsserted,
  visorResetAssertFailed,
  visorResetRestoreFailed,
  visorResetReadbackFailed,
  visorResetNotReleased
};
struct visor_reset_ops {
  int (*get)(void *, unsigned short, unsigned short, unsigned char *);
  int (*set)(void *, unsigned short, unsigned short, unsigned char *);
  void (*delay_ms)(void *, unsigned long);
  void *context;
};
struct visor_reset_result {
  enum visor_reset_reason reason;
  unsigned short attribute;
  int query_error, assert_error, release_error, readback_error;
  unsigned char before, after;
};
struct visor_reset_result visor_reset_check(const struct visor_reset_ops *,
                                            unsigned short);
struct visor_reset_result visor_reset_pulse(const struct visor_reset_ops *,
                                            unsigned short);
#endif
