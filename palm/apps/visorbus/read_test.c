#include "read_test.h"

struct visor_read_result visor_read_check(const struct visor_read_ops *ops,
                                         unsigned short card) {
  struct visor_read_result r = {0};
  const unsigned short attrs[] = {visorCardRemovable, visorCardHwInstalled,
                                  visorCardSwInstalled, visorCardReset};
  unsigned char value;
  unsigned i;
  if (card == 0 || card > 3) {
    r.reason = visorReadInvalidSlot;
    return r;
  }
  for (i = 0; i < 4; ++i) {
    r.attribute = attrs[i];
    r.error = ops->get_byte(ops->context, card, attrs[i], &value);
    if (r.error) { r.reason = visorReadQueryFailed; return r; }
    if (i == 0 && !value) r.reason = visorReadNotRemovable;
    if (i == 1 && !value) r.reason = visorReadNotInstalled;
    if (i == 2 && !value) r.reason = visorReadNotRegistered;
    if (i == 3 && value) r.reason = visorReadResetAsserted;
    if (r.reason != visorReadReady) return r;
  }
  r.attribute = visorCardCsBase;
  r.error = ops->get_wide(ops->context, card, r.attribute, &r.base);
  if (!r.error) {
    r.attribute = visorCardCsSize;
    r.error = ops->get_wide(ops->context, card, r.attribute, &r.size);
  }
  if (r.error) r.reason = visorReadQueryFailed;
  else if (!r.base || (r.base & 1) || r.base > 0xffffffffUL ||
           r.size < visorReadWords * 2 || r.size > 0x01000000UL ||
           r.base > 0xffffffffUL - (r.size - 1))
    r.reason = visorReadBadMapping;
  return r;
}

struct visor_read_result visor_read_probe(const struct visor_read_ops *ops,
                                         unsigned short card) {
  struct visor_read_result r = visor_read_check(ops, card);
  if (r.reason != visorReadReady) return r;
  while (r.completed < visorReadWords) {
    struct visor_read_result current = visor_read_check(ops, card);
    if (current.reason != visorReadReady) {
      r.reason = current.reason; r.error = current.error;
      r.attribute = current.attribute; return r;
    }
    if (current.base != r.base || current.size != r.size) {
      r.reason = visorReadMappingChanged; return r;
    }
    r.error = ops->read_word(ops->context, r.base + r.completed * 2,
                             &r.words[r.completed]);
    if (r.error) { r.reason = visorReadAccessFailed; return r; }
    ++r.completed;
  }
  return r;
}
