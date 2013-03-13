#include "client.h"
#include "client-private.h"
#include "signal-helper.h"
#include "outputs.h"

/**
 * Missing commands:
 *
 *  shuffle_range
 *  (...)
 */

///////////////////

void mc_client_queue_add(mc_Client *self, const char *uri)
{
    COMMAND(
            mpd_run_add(conn, uri),
            mpd_send_add(conn, uri)
    )
}

///////////////////

void mc_client_queue_clear(mc_Client *self)
{
    COMMAND(
            mpd_run_clear(conn),
            mpd_send_clear(conn)
    )
}

///////////////////

void mc_client_consume(mc_Client *self, bool mode)
{
    COMMAND(
            mpd_run_consume(conn, mode),
            mpd_send_consume(conn, mode)
    )
}

///////////////////

void mc_client_crossfade(mc_Client *self, bool mode)
{
    COMMAND(
            mpd_run_crossfade(conn, mode),
            mpd_send_crossfade(conn, mode)
    )
}

///////////////////

void mc_client_queue_delete(mc_Client *self, int pos)
{
    COMMAND(
            mpd_run_delete(conn, pos),
            mpd_send_delete(conn, pos)
    )
}

///////////////////

void mc_client_queue_delete_id(mc_Client *self, int id)
{
    COMMAND(
            mpd_run_delete_id(conn, id),
            mpd_send_delete_id(conn, id)
    )
}

///////////////////

void mc_client_queue_delete_range(mc_Client *self, int start, int end)
{
    COMMAND(
            mpd_run_delete_range(conn, start, end),
            mpd_send_delete_range(conn, start, end)
    )
}

///////////////////

bool mc_client_output_switch(mc_Client *self, const char *output_name, bool mode)
{
    g_assert(self);
    int output_id = mc_proto_outputs_name_to_id(self, output_name);

    if (output_id != -1) {
        if (mode == TRUE) {
            COMMAND(
                    mpd_run_enable_output(conn, output_id),
                    mpd_send_enable_output(conn, output_id)
            );
        } else {
            COMMAND(
                    mpd_run_disable_output(conn, output_id),
                    mpd_send_disable_output(conn, output_id)
            );
        }

        return true;
    } else {
        return false;
    }
}

///////////////////

void mc_client_playlist_load(mc_Client *self, const char *playlist)
{
    COMMAND(
            mpd_run_load(conn, playlist),
            mpd_send_load(conn, playlist)
    );
}

///////////////////

void mc_client_mixramdb(mc_Client *self, int decibel)
{
    COMMAND(
            mpd_run_mixrampdb(conn, decibel),
            mpd_send_mixrampdb(conn, decibel)
    );
}

///////////////////

void mc_client_mixramdelay(mc_Client *self, int seconds)
{
    COMMAND(
            mpd_run_mixrampdelay(conn, seconds),
            mpd_send_mixrampdelay(conn, seconds)
    );
}

///////////////////

void mc_client_queue_move(mc_Client *self, unsigned old_id, unsigned new_id)
{
    COMMAND(
            mpd_run_move_id(conn, old_id, new_id),
            mpd_send_move_id(conn, old_id, new_id)
    );
}

///////////////////

void mc_client_queue_move_range(mc_Client *self, unsigned start_pos, unsigned end_pos, unsigned new_pos)
{
    COMMAND(
            mpd_run_move_range(conn, start_pos, end_pos, new_pos),
            mpd_send_move_range(conn, start_pos, end_pos, new_pos)
    );
}

///////////////////

void mc_client_next(mc_Client *self)
{
    COMMAND(
            mpd_run_next(conn),
            mpd_send_next(conn)
    )
}

///////////////////

bool mc_client_password(mc_Client *self, const char *pwd)
{
    bool rc = false;
    COMMAND(
            rc = mpd_run_password(conn, pwd),
            mpd_send_password(conn, pwd)
    );
    return rc;
}

///////////////////

void mc_client_pause(mc_Client *self)
{
    COMMAND(
            mpd_run_toggle_pause(conn),
            mpd_send_toggle_pause(conn)
    )
}

///////////////////

void mc_client_play(mc_Client *self)
{
    COMMAND(
            mpd_run_play(conn),
            mpd_send_play(conn)
    )
}

///////////////////

void mc_client_play_id(mc_Client *self, unsigned id)
{
    COMMAND(
            mpd_run_play_id(conn, id),
            mpd_send_play_id(conn, id)
    )
}

///////////////////

void mc_client_playlist_add(mc_Client *self, const char *name, const char *file)
{
    COMMAND(
            mpd_run_playlist_add(conn, name, file),
            mpd_send_playlist_add(conn, name, file)
    )
}

///////////////////

void mc_client_playlist_clear(mc_Client *self, const char *name)
{
    COMMAND(
            mpd_run_playlist_clear(conn, name),
            mpd_send_playlist_clear(conn, name)
    )
}

///////////////////

void mc_client_playlist_delete(mc_Client *self, const char *name, unsigned pos)
{
    COMMAND(
            mpd_run_playlist_delete(conn, name, pos),
            mpd_send_playlist_delete(conn, name, pos)
    )
}

///////////////////

void mc_client_playlist_move(mc_Client *self, const char *name, unsigned old_pos, unsigned new_pos)
{
    COMMAND(
        mpd_send_playlist_move(conn, name, old_pos, new_pos);
        mpd_response_finish(conn);
        ,
        mpd_send_playlist_move(conn, name, old_pos, new_pos);
    )
}

///////////////////

void mc_client_previous(mc_Client *self)
{
    COMMAND(
            mpd_run_previous(conn),
            mpd_send_previous(conn)
    )
}

///////////////////

void mc_client_prio(mc_Client *self, unsigned prio, unsigned position)
{
    COMMAND(
            mpd_run_prio(conn,  prio, position),
            mpd_send_prio(conn, prio, position)
    )
}

///////////////////

void mc_client_prio_range(mc_Client *self, unsigned prio, unsigned start_pos, unsigned end_pos)
{
    COMMAND(
            mpd_run_prio_range(conn,  prio, start_pos, end_pos),
            mpd_send_prio_range(conn, prio, start_pos, end_pos)
    )
}

///////////////////

void mc_client_prio_id(mc_Client *self, unsigned prio, unsigned id)
{
    COMMAND(
            mpd_run_prio_id(conn,  prio, id),
            mpd_send_prio_id(conn, prio, id)
    )
}

///////////////////

void mc_client_random(mc_Client *self, bool mode)
{
    COMMAND(
            mpd_run_random(conn, mode),
            mpd_send_random(conn, mode)
    )
}

///////////////////

void mc_client_playlist_rename(mc_Client *self, const char *old_name, const char *new_name)
{
    COMMAND(
            mpd_run_rename(conn, old_name, new_name),
            mpd_send_rename(conn, old_name, new_name)
    );
}

///////////////////

void mc_client_repeat(mc_Client *self, bool mode)
{
    COMMAND(
            mpd_run_repeat(conn, mode),
            mpd_send_repeat(conn, mode)
    );
}

///////////////////

void mc_client_replay_gain_mode(mc_Client *self, const char *replay_gain_mode)
{
    COMMAND(
        mpd_send_command(conn, "replay_gain_mode", replay_gain_mode, NULL);
        mpd_response_finish(conn);
        , /* send command */
        mpd_send_command(conn, "replay_gain_mode", replay_gain_mode, NULL);
    )
}

///////////////////

void mc_client_database_rescan(mc_Client *self, const char *path)
{
    COMMAND(
            mpd_run_rescan(conn, path),
            mpd_send_rescan(conn, path)
    );
}

///////////////////

void mc_client_playlist_rm(mc_Client *self, const char *playlist_name)
{
    COMMAND(
            mpd_run_rm(conn, playlist_name),
            mpd_send_rm(conn, playlist_name)
    );
}

void mc_client_playlist_save(mc_Client *self, const char *as_name)
{
    COMMAND(
            mpd_run_save(conn, as_name),
            mpd_send_save(conn, as_name)
    );
}

///////////////////

void mc_client_seek(mc_Client *self, int pos, int seconds)
{
    COMMAND(
            mpd_run_seek_pos(conn, pos, seconds),
            mpd_send_seek_pos(conn, pos, seconds)
    );
}

///////////////////

void mc_client_seekid(mc_Client *self, int id, int seconds)
{
    COMMAND(
            mpd_run_seek_id(conn, id, seconds),
            mpd_send_seek_id(conn, id, seconds)
    );
}

///////////////////

void mc_client_seekcur(mc_Client *self, int seconds)
{
    /* there is 'seekcur' in newer mpd versions,
     * but we can emulate it easily */
    if (self->song != NULL) {
        int curr_id = mpd_song_get_id(self->song);
        COMMAND(
                mpd_run_seek_id(conn, curr_id, seconds),
                mpd_send_seek_id(conn, curr_id, seconds)
        )
    }
}

///////////////////

void mc_client_setvol(mc_Client *self, int volume)
{
    COMMAND(
            mpd_run_set_volume(conn, volume),
            mpd_send_set_volume(conn, volume)
    );
}

///////////////////

void mc_client_queue_shuffle(mc_Client *self)
{
    COMMAND(
            mpd_run_shuffle(conn),
            mpd_send_shuffle(conn)
    );
}

///////////////////

void mc_client_single(mc_Client *self, bool mode)
{
    COMMAND(
            mpd_run_single(conn, mode),
            mpd_send_single(conn, mode)
    );
}

///////////////////

void mc_client_stop(mc_Client *self)
{
    COMMAND(
            mpd_run_stop(conn),
            mpd_send_stop(conn)
    );
}

///////////////////

void mc_client_queue_swap(mc_Client *self, int pos_a, int pos_b)
{
    COMMAND(
            mpd_run_swap(conn, pos_a, pos_b),
            mpd_send_swap(conn, pos_a, pos_b)
    );
}

///////////////////

void mc_client_queue_swap_id(mc_Client *self, int id_a, int id_b)
{
    COMMAND(
            mpd_run_swap_id(conn, id_a, id_b),
            mpd_send_swap_id(conn, id_a, id_b)
    );
}

///////////////////

void mc_client_database_update(mc_Client *self, const char *path)
{
    COMMAND(
            mpd_run_update(conn, path),
            mpd_send_update(conn, path)
    );
}

///////////////////
