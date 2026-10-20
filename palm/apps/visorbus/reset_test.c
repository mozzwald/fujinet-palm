#include "reset_test.h"

struct visor_reset_result visor_reset_check(const struct visor_reset_ops *ops,
                                            unsigned short card) {
  struct visor_reset_result r = {0};
  unsigned char value;
  unsigned short attrs[4] = {visorCardRemovable, visorCardHwInstalled,
                             visorCardSwInstalled, visorCardReset};
  unsigned i;
  if (card == 0 || card > 3) {
    r.reason = visorResetInvalidSlot;
    return r;
  }
  for (i = 0; i < 4; ++i) {
    r.attribute = attrs[i];
    r.query_error = ops->get(ops->context, card, attrs[i], &value);
    if (r.query_error) {
      r.reason = visorResetQueryFailed;
      return r;
    }
    if (i == 0 && !value)
      r.reason = visorResetNotRemovable;
    if (i == 1 && !value)
      r.reason = visorResetNotInstalled;
    if (i == 2 && value)
      r.reason = visorResetSoftwareInstalled;
    if (i == 3) {
      r.before = value;
      if (value)
        r.reason = visorResetAlreadyAsserted;
    }
    if (r.reason != visorResetReady)
      return r;
  }
  return r;
}

struct visor_reset_result visor_reset_pulse(const struct visor_reset_ops *ops,
                                            unsigned short card) {
  struct visor_reset_result r = visor_reset_check(ops, card);
  unsigned char asserted = 1, released = 0;
  if (r.reason != visorResetReady)
    return r;
  r.assert_error = ops->set(ops->context, card, visorCardReset, &asserted);
  if (!r.assert_error)
    ops->delay_ms(ops->context, 250);
  /* Even an assertion error takes the restoration path; prior state was 0. */
  r.release_error = ops->set(ops->context, card, visorCardReset, &released);
  if (r.release_error)
    r.release_error = ops->set(ops->context, card, visorCardReset, &released);
  r.readback_error = ops->get(ops->context, card, visorCardReset, &r.after);
  if (r.release_error)
    r.reason = visorResetRestoreFailed;
  else if (r.readback_error)
    r.reason = visorResetReadbackFailed;
  else if (r.after)
    r.reason = visorResetNotReleased;
  else if (r.assert_error)
    r.reason = visorResetAssertFailed;
  return r;
}
