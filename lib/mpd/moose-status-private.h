#ifndef MOOSE_STATUS_PRIVATE_H
#define MOOSE_STATUS_PRIVATE_H

#include "moose-status.h"

G_BEGIN_DECLS

void moose_status_update_stats(const MooseStatus* self, const struct mpd_stats* stats);
void moose_status_set_replay_gain_mode(const MooseStatus* self, const char* mode);
void moose_status_set_current_song(MooseStatus* self, const MooseSong* song);
void moose_status_outputs_clear(const MooseStatus* self);
void moose_status_outputs_copy(MooseStatus *self, const MooseStatus *other);
void moose_status_outputs_add(
    const MooseStatus *self,
    const char *name,
    int id,
    bool enabled
);

G_END_DECLS

#endif /* end of include guard: MOOSE_STATUS_PRIVATE_H */
