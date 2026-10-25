#ifndef VISOR_READ_TEST_H
#define VISOR_READ_TEST_H
#include "reset_test.h"
enum { visorReadWords = 8 };
enum visor_read_reason {
  visorReadReady, visorReadInvalidSlot, visorReadQueryFailed,
  visorReadNotRemovable, visorReadNotInstalled, visorReadNotRegistered,
  visorReadResetAsserted, visorReadBadMapping, visorReadAccessFailed,
  visorReadMappingChanged
};
struct visor_read_ops {
  int (*get_byte)(void *, unsigned short, unsigned short, unsigned char *);
  int (*get_wide)(void *, unsigned short, unsigned short, unsigned long *);
  int (*read_word)(void *, unsigned long, unsigned short *);
  void *context;
};
struct visor_read_result {
  enum visor_read_reason reason;
  unsigned short attribute, completed, words[visorReadWords];
  int error;
  unsigned long base, size;
};
struct visor_read_result visor_read_check(const struct visor_read_ops *, unsigned short);
/* Bounded RAM diagnostics may inspect a physically installed, mapped ROM even
 * when recognition/software registration failed. Never fabricates SwInstalled
 * or changes mapping. The original registered-reader guard is unchanged. */
struct visor_read_result visor_read_check_physical(const struct visor_read_ops *, unsigned short);
struct visor_read_result visor_read_probe(const struct visor_read_ops *, unsigned short);
#endif
