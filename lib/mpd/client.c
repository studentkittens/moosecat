#include "client.h"
#include "signal-helper.h"
#include "outputs.h"
#include "update.h"

#include <string.h>

static bool mc_client_command_list_begin(mc_Client *self);
static void mc_client_command_list_append(mc_Client *self, const char *command);
static bool mc_client_command_list_commit(mc_Client *self);

//////////////////////////////////////
//                                  //
//     Client Command Handlers      //
//                                  //
//////////////////////////////////////

/**
 * Missing commands:
 *
 *  shuffle_range
 *  (...)
 */
#define COMMAND(_getput_code_, _command_list_code_) \
    if(mc_client_command_list_is_active(self)) {    \
        _command_list_code_;                        \
    } else {                                        \
        _getput_code_;                              \
    }                                               \


#define intarg(argument, result)                     \
{                                                    \
    char *endptr = NULL;                             \
    result = g_ascii_strtoll(argument, &endptr, 10); \
                                                     \
    if(result == 0 && endptr != NULL) {              \
        return false;                                \
    }                                                \
}                                                    \

#define intarg_named(argument, name) \
    int name = 0;                    \
    intarg(argument, name);          \

///////////////////

static bool handle_queue_add(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    const char *uri = argv[0];
    COMMAND(
        mpd_run_add(conn, uri),
        mpd_send_add(conn, uri)
    )

    return true;
}

///////////////////

static bool handle_queue_clear(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    COMMAND(
        mpd_run_clear(conn),
        mpd_send_clear(conn)
    )

    return true;
}

///////////////////

static bool handle_consume(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    intarg_named(argv[0], mode);
    COMMAND(
        mpd_run_consume(conn, mode),
        mpd_send_consume(conn, mode)
    )

    return true;
}

///////////////////

static bool handle_crossfade(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    intarg_named(argv[0], mode);
    COMMAND(
        mpd_run_crossfade(conn, mode),
        mpd_send_crossfade(conn, mode)
    )

    return true;
}

///////////////////

static bool handle_queue_delete(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    intarg_named(argv[0], pos);
    COMMAND(
        mpd_run_delete(conn, pos),
        mpd_send_delete(conn, pos)
    )

    return true;
}

///////////////////

static bool handle_queue_delete_id(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    intarg_named(argv[0], id);
    COMMAND(
        mpd_run_delete_id(conn, id),
        mpd_send_delete_id(conn, id)
    )

    return true;
}

///////////////////

static bool handle_queue_delete_range(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    intarg_named(argv[0], start);
    intarg_named(argv[1], end);
    COMMAND(
        mpd_run_delete_range(conn, start, end),
        mpd_send_delete_range(conn, start, end)
    )

    return true;
}

///////////////////

static bool handle_output_switch(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    const char *output_name = argv[0];
    int output_id = mc_priv_outputs_name_to_id(self->_outputs, output_name);


    intarg_named(argv[1], mode);

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

static bool handle_playlist_load(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    const char *playlist = argv[0];
    COMMAND(
        mpd_run_load(conn, playlist),
        mpd_send_load(conn, playlist)
    );

    return true;
}

///////////////////

static bool handle_mixramdb(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    intarg_named(argv[0], decibel);
    COMMAND(
        mpd_run_mixrampdb(conn, decibel),
        mpd_send_mixrampdb(conn, decibel)
    );

    return true;
}

///////////////////

static bool handle_mixramdelay(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    intarg_named(argv[0], seconds);
    COMMAND(
        mpd_run_mixrampdelay(conn, seconds),
        mpd_send_mixrampdelay(conn, seconds)
    );

    return true;
}

///////////////////

static bool handle_queue_move(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    intarg_named(argv[0], old_id);
    intarg_named(argv[1], new_id);
    COMMAND(
        mpd_run_move_id(conn, old_id, new_id),
        mpd_send_move_id(conn, old_id, new_id)
    );

    return true;
}

///////////////////

static bool handle_queue_move_range(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    intarg_named(argv[0], start_pos);
    intarg_named(argv[1], end_pos);
    intarg_named(argv[2], new_pos);
    COMMAND(
        mpd_run_move_range(conn, start_pos, end_pos, new_pos),
        mpd_send_move_range(conn, start_pos, end_pos, new_pos)
    );

    return true;
}

///////////////////

static bool handle_next(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    COMMAND(
        mpd_run_next(conn),
        mpd_send_next(conn)
    )

    return true;
}

///////////////////

static bool handle_password(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    const char *password = argv[0];
    bool rc = false;
    COMMAND(
        rc = mpd_run_password(conn, password),
        mpd_send_password(conn, password)
    );
    return rc;
}

///////////////////

static bool handle_pause(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    COMMAND(
        mpd_run_toggle_pause(conn),
        mpd_send_toggle_pause(conn)
    )
    
    return true;
}

///////////////////

static bool handle_play(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    COMMAND(
        mpd_run_play(conn),
        mpd_send_play(conn)
    )

    return true;
}

///////////////////

static bool handle_play_id(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    intarg_named(argv[0], id);
    COMMAND(
        mpd_run_play_id(conn, id),
        mpd_send_play_id(conn, id)
    )

    return true;
}

///////////////////

static bool handle_playlist_add(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    const char *name = argv[0];
    const char *file = argv[1];
    COMMAND(
        mpd_run_playlist_add(conn, name, file),
        mpd_send_playlist_add(conn, name, file)
    )
    
    return true;
}

///////////////////

static bool handle_playlist_clear(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    const char *name = argv[0];
    COMMAND(
        mpd_run_playlist_clear(conn, name),
        mpd_send_playlist_clear(conn, name)
    )

    return true;
}

///////////////////

static bool handle_playlist_delete(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    const char *name = argv[0];
    intarg_named(argv[1], pos);
    COMMAND(
        mpd_run_playlist_delete(conn, name, pos),
        mpd_send_playlist_delete(conn, name, pos)
    )

    return true;
}

///////////////////

static bool handle_playlist_move(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    const char *name = argv[0];
    intarg_named(argv[1], old_pos);
    intarg_named(argv[2], new_pos);
    COMMAND(
        mpd_send_playlist_move(conn, name, old_pos, new_pos);
        mpd_response_finish(conn);
        ,
        mpd_send_playlist_move(conn, name, old_pos, new_pos);
    )

    return true;
}

///////////////////

static bool handle_previous(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    COMMAND(
        mpd_run_previous(conn),
        mpd_send_previous(conn)
    )

    return true;
}

///////////////////

static bool handle_prio(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    intarg_named(argv[0], prio);
    intarg_named(argv[1], position);

    COMMAND(
        mpd_run_prio(conn,  prio, position),
        mpd_send_prio(conn, prio, position)
    )

    return true;
}

///////////////////

static bool handle_prio_range(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    intarg_named(argv[0], prio);
    intarg_named(argv[1], start_pos);
    intarg_named(argv[2], end_pos);

    COMMAND(
        mpd_run_prio_range(conn,  prio, start_pos, end_pos),
        mpd_send_prio_range(conn, prio, start_pos, end_pos)
    )

    return true;
}

///////////////////

static bool handle_prio_id(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    intarg_named(argv[0], prio);
    intarg_named(argv[1], id);

    COMMAND(
        mpd_run_prio_id(conn,  prio, id),
        mpd_send_prio_id(conn, prio, id)
    )

    return true;
}

///////////////////

static bool handle_random(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    intarg_named(argv[0], mode);

    COMMAND(
        mpd_run_random(conn, mode),
        mpd_send_random(conn, mode)
    )

    return true;
}

///////////////////

static bool handle_playlist_rename(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    const char *old_name = argv[0];
    const char *new_name = argv[1];

    COMMAND(
        mpd_run_rename(conn, old_name, new_name),
        mpd_send_rename(conn, old_name, new_name)
    );
    return true;
}

///////////////////

static bool handle_repeat(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    intarg_named(argv[0], mode);

    COMMAND(
        mpd_run_repeat(conn, mode),
        mpd_send_repeat(conn, mode)
    );

    return true;
}

///////////////////

static bool handle_replay_gain_mode(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    intarg_named(argv[0], replay_gain_mode);

    COMMAND(
        mpd_send_command(conn, "replay_gain_mode", replay_gain_mode, NULL);
        mpd_response_finish(conn);
        , /* send command */
        mpd_send_command(conn, "replay_gain_mode", replay_gain_mode, NULL);
    )

    return true;
}

///////////////////

static bool handle_database_rescan(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    const char *path = argv[0];
    COMMAND(
        mpd_run_rescan(conn, path),
        mpd_send_rescan(conn, path)
    );

    return true;
}

///////////////////

static bool handle_playlist_rm(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    const char *playlist_name = argv[0];
    COMMAND(
        mpd_run_rm(conn, playlist_name),
        mpd_send_rm(conn, playlist_name)
    );

    return true;
}

static bool handle_playlist_save(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    const char *as_name = argv[0];
    COMMAND(
        mpd_run_save(conn, as_name),
        mpd_send_save(conn, as_name)
    );

    return true;
}

///////////////////

static bool handle_seek(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    intarg_named(argv[0], pos);
    intarg_named(argv[1], seconds);

    COMMAND(
        mpd_run_seek_pos(conn, pos, seconds),
        mpd_send_seek_pos(conn, pos, seconds)
    );

    return true;
}

///////////////////

static bool handle_seek_id(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    intarg_named(argv[0], id);
    intarg_named(argv[1], seconds);

    COMMAND(
        mpd_run_seek_id(conn, id, seconds),
        mpd_send_seek_id(conn, id, seconds)
    );

    return true;
}

///////////////////

static bool handle_seekcur(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    intarg_named(argv[0], seconds);

    /* there is 'seekcur' in newer mpd versions,
     * but we can emulate it easily */
    if(mc_is_connected(self))
    {
        int curr_id = 0;
        struct mpd_song * current_song = mc_lock_current_song(self);
        if(current_song != NULL) {
            curr_id = mpd_song_get_id(current_song);
            mc_unlock_current_song(self);
        } else {
            mc_unlock_current_song(self);
            return false;
        }

        COMMAND(
            mpd_run_seek_id(conn, curr_id, seconds),
            mpd_send_seek_id(conn, curr_id, seconds)
        )
    }

    return true;
}

///////////////////

static bool handle_setvol(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    intarg_named(argv[0], volume);
    COMMAND(
        mpd_run_set_volume(conn, volume),
        mpd_send_set_volume(conn, volume)
    );

    return true;
}

///////////////////

static bool handle_queue_shuffle(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    COMMAND(
        mpd_run_shuffle(conn),
        mpd_send_shuffle(conn)
    );
    return true;
}

///////////////////

static bool handle_single(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    intarg_named(argv[0], mode);
    COMMAND(
        mpd_run_single(conn, mode),
        mpd_send_single(conn, mode)
    );

    return true;
}

///////////////////

static bool handle_stop(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    COMMAND(
        mpd_run_stop(conn),
        mpd_send_stop(conn)
    );

    return true;
}

///////////////////

static bool handle_queue_swap(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    intarg_named(argv[0], pos_a);
    intarg_named(argv[1], pos_b);

    COMMAND(
        mpd_run_swap(conn, pos_a, pos_b),
        mpd_send_swap(conn, pos_a, pos_b)
    );

    return true;
}

///////////////////

static bool handle_queue_swap_id(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    intarg_named(argv[0], id_a);
    intarg_named(argv[1], id_b);

    COMMAND(
        mpd_run_swap_id(conn, id_a, id_b),
        mpd_send_swap_id(conn, id_a, id_b)
    );

    return true;
}

///////////////////

static bool handle_database_update(mc_Client *self, struct mpd_connection *conn, const char **argv)
{
    const char *path = argv[0];
    COMMAND(
        mpd_run_update(conn, path),
        mpd_send_update(conn, path)
    );

    return true;
}

//////////////////////////////////////
//                                  //
//       Private Command Logic      //
//                                  //
//////////////////////////////////////

typedef bool (*mc_ClientHandler)(
        mc_Client *self,             /* Client to operate on */
        struct mpd_connection *conn, /* Readily prepared connection */
        const char **args            /* Arguments as string */
);

///////////////////

typedef struct {
    const char *command;
    int num_args;
    mc_ClientHandler handler;
} mc_HandlerField;

///////////////////

static const mc_HandlerField HandlerTable[] = {
    {"consume",            1,  handle_consume},
    {"crossfade",          1,  handle_crossfade},
    {"database_rescan",    1,  handle_database_rescan},
    {"database_update",    1,  handle_database_update},
    {"mixramdb",           1,  handle_mixramdb},
    {"mixramdelay",        1,  handle_mixramdelay},
    {"next",               0,  handle_next},
    {"output_switch",      2,  handle_output_switch},
    {"password",           1,  handle_password},
    {"pause",              0,  handle_pause},
    {"play",               0,  handle_play},
    {"play_id",            1,  handle_play_id},
    {"playlist_add",       2,  handle_playlist_add},
    {"playlist_clear",     1,  handle_playlist_clear},
    {"playlist_delete",    2,  handle_playlist_delete},
    {"playlist_load",      1,  handle_playlist_load},
    {"playlist_move",      3,  handle_playlist_move},
    {"playlist_rename",    2,  handle_playlist_rename},
    {"playlist_rm",        1,  handle_playlist_rm},
    {"playlist_save",      1,  handle_playlist_save},
    {"previous",           0,  handle_previous},
    {"prio",               2,  handle_prio},
    {"prio_id",            2,  handle_prio_id},
    {"prio_range",         3,  handle_prio_range},
    {"queue_add",          1,  handle_queue_add},
    {"queue_clear",        0,  handle_queue_clear},
    {"queue_delete",       1,  handle_queue_delete},
    {"queue_delete_id",    1,  handle_queue_delete_id},
    {"queue_delete_range", 2,  handle_queue_delete_range},
    {"queue_move",         2,  handle_queue_move},
    {"queue_move_range",   3,  handle_queue_move_range},
    {"queue_shuffle",      0,  handle_queue_shuffle},
    {"queue_swap",         2,  handle_queue_swap},
    {"queue_swap_id",      2,  handle_queue_swap_id},
    {"random",             1,  handle_random},
    {"repeat",             1,  handle_repeat},
    {"replay_gain_mode",   1,  handle_replay_gain_mode},
    {"seek",               2,  handle_seek},
    {"seekcur",            1,  handle_seekcur},
    {"seek_id",            2,  handle_seek_id},
    {"setvol",             1,  handle_setvol},
    {"single",             1,  handle_single},
    {"stop",               0,  handle_stop},
    {NULL,                 0,  NULL}
};

///////////////////

static const mc_HandlerField * mc_client_find_handler(const char *command)
{
    for(int i = 0; HandlerTable[i].command; ++i) {
        if(g_ascii_strcasecmp(command, HandlerTable[i].command) == 0) {
            return &HandlerTable[i];
        }
    }
    return NULL;
}

///////////////////

/**
 * @brief Parses a string like "command arg1 /// arg2" to a string vector of
 * command, arg1, arg2
 *
 * @param input the whole command string
 *
 * @return a newly allocated vector of strings
 */
static char ** mc_client_parse_into_parts(const char * input)
{
    if(input == NULL) {
        return NULL;
    }
    
    /* Fast forward to the first non-space char */
    while(g_ascii_isspace(*input)) {
        ++input;
    }

    /* Extract the command part */
    char * first_space = strchr(input, ' ');
    char * command = g_strndup(
            input,
            (first_space) ? (gsize)ABS(first_space - input) : strlen(input)
    );

    if(command == NULL) {
        return NULL;
    }

    if(first_space != NULL) {
        /* Now split the arguments */
        char ** args = g_strsplit(first_space + 1, " /// ", -1);
        guint args_len = g_strv_length(args);

        /* Allocate the result vector (command + NULL) */
        char ** result_split = g_malloc0(sizeof(char *) * (args_len + 2));

        /* Copy all data to the new vector */
        result_split[0] = command;
        memcpy(&result_split[1], args, sizeof(char * ) * args_len);

        /* Free the old vector, but not strings in it */
        g_free(args);
        return result_split;
    } else {
        /* Only a single command */
        char ** result = g_malloc0(sizeof(char *) * 2);
        result[0] = command;
        return  result;
    }
}

///////////////////

static bool mc_client_execute(
        mc_Client *self,
        const char *input,
        struct mpd_connection *conn
)
{
    g_assert(conn);

    /* success state of the command */
    bool result = false;

    /* Argument vector */
    char **parts = mc_client_parse_into_parts(input);

    /* find out what handler to call */
    const mc_HandlerField *handler = mc_client_find_handler(g_strstrip(parts[0]));

    if(handler != NULL) {
        /* Count arguments */
        int arguments = 0;
        while(parts[arguments++]);

        /* -2 because: -1 for off-by-one and -1 for not counting the command itself */
        if((arguments - 2) >= handler->num_args) {
            if(mc_is_connected(self)) {
                result = handler->handler(self, conn, (const char **)&parts[1]);
            }
        } else {
            mc_shelper_report_error_printf(self,
                    "API-Misuse: Too many arguments to %s: Expected %d, Got %d\n", 
                     parts[0], handler->num_args, arguments - 2
            );
        }
    }

    g_strfreev(parts);
    return result;
}

///////////////////

static char mc_client_command_list_is_start_or_end(const char *command)
{
    if(g_ascii_strcasecmp(command, "command_list_begin") == 0)
        return 1;

    if(g_ascii_strcasecmp(command, "command_list_end") == 0)
        return -1;

    return 0;
}

///////////////////

static void * mc_client_command_dispatcher(
        struct mc_JobManager *jm,
        volatile bool *cancel,
        void *user_data,
        void *job_data) 
{
    g_assert(user_data);
        
    bool result = false;

    if(job_data != NULL) {
        /* Client to operate on */
        mc_Client *self = user_data;  

        /* Input command left to parse */
        const char *input = job_data;

        /* Free input (since it was strdrup'd) */
        bool free_input = true;
        if(mc_is_connected(self) == false) {
            result = false;
        }
        else
        if(mc_client_command_list_is_start_or_end(input) == -1) {
            result = mc_client_command_list_commit(self);
        }
        else
        if(mc_client_command_list_is_active(self)) {
            mc_client_command_list_append(self, input);
            if(mc_is_connected(self)) {
                result = true;
                free_input = false;
            }
        }
        else 
        if(mc_client_command_list_is_start_or_end(input) == +1) {
            mc_client_command_list_begin(self);
            result = true;
        }
        else {
            struct mpd_connection * conn = mc_get(self);
            if(conn != NULL && mc_is_connected(self)) {
                result = mc_client_execute(self, input, conn);
                if (mpd_response_finish(conn) == false) {       
                    mc_shelper_report_error(self, conn);        
                }                                               
            }
            mc_put(self);
        }

        if(free_input) {
            /* Input was strdup'd */
            g_free((char *) input);
        }
    }

    return GINT_TO_POINTER(result);
}

//////////////////////////////////////
//                                  //
//Code Pretending to do Command List//
//                                  //
//////////////////////////////////////


/**
 * A note about the implementation of command_list_{begin, end}:
 *
 * Currently libmoosecat buffers command list commandos.
 * Instead of sending them actually to the server they are stored in a List.
 * Once command_list_end is found, the list is executed as once. 
 * (Sending all commandoes at once, and calling mpd_response_finish to wait for
 * the OK or ACK)
 *
 * This has been decided after testing this (with MPD 0.17.0):
 *  
 *   Telnet Session #1              Telnet Session #2
 *  
 *   > status                       > command_list_begin
 *   repeat 0                       > repeat 1
 *   > status                       > 
 *   repeat 0                       > command_list_end
 *   > status                       > OK
 *   repeat 1
 *
 * The server seems to queue the statements anyway. So no reason to implement
 * something more sophisticated here.
*/

static bool mc_client_command_list_begin(mc_Client *self)
{
    g_assert(self);

    g_mutex_lock(&self->command_list.is_active_mtx);
    self->command_list.is_active = 1;
    g_mutex_unlock(&self->command_list.is_active_mtx);

    return mc_client_command_list_is_active(self);
}

///////////////////

static void mc_client_command_list_append(mc_Client *self, const char *command)
{
    g_assert(self);

    /* prepend now, reverse later on commit */
    self->command_list.commands = g_list_prepend(
            self->command_list.commands,
            (gpointer)command
    );
}

///////////////////

static bool mc_client_command_list_commit(mc_Client *self)
{
    g_assert(self);

    /* Elements were prepended, so we'll just reverse the list */
    self->command_list.commands = g_list_reverse(self->command_list.commands);

    struct mpd_connection * conn = mc_get(self);  
    if(conn != NULL) {
        if(mpd_command_list_begin(conn, false) != false) {
            for(GList *iter = self->command_list.commands; iter != NULL; iter = iter->next) {
                const char *command = iter->data;
                mc_client_execute(self, command, conn);
                g_free((char *) command);
            }

            if(mpd_command_list_end(conn) == false) {
                mc_shelper_report_error(self, conn);        
            }
        } else {
            mc_shelper_report_error(self, conn);        
        }

        if (mpd_response_finish(conn) == false) {       
            mc_shelper_report_error(self, conn);        
        }                                               
    }                   

    g_list_free(self->command_list.commands);
    self->command_list.commands = NULL;

    /* Put mutex back */
    mc_put(self);                                 

    g_mutex_lock(&self->command_list.is_active_mtx);
    self->command_list.is_active = 0;
    g_mutex_unlock(&self->command_list.is_active_mtx);


    return !mc_client_command_list_is_active(self);
}

//////////////////////////////////////
//                                  //
//    Public Function Interface     //
//                                  //
//////////////////////////////////////


void mc_client_init(mc_Client *self)
{
    g_assert(self);

    g_mutex_init(&self->command_list.is_active_mtx);
    self->jm = mc_jm_create(mc_client_command_dispatcher, self);
}

///////////////////

void mc_client_destroy(mc_Client *self)
{
    g_assert(self);

    mc_jm_close(self->jm);
    self->jm = NULL;

    g_mutex_clear(&self->command_list.is_active_mtx);
}

///////////////////

long mc_client_send(mc_Client *self, const char *command)
{
    g_assert(self);

    return mc_jm_send(self->jm, 0, (void *)g_strdup(command));
}

///////////////////

bool mc_client_recv(mc_Client *self, long job_id)
{
    g_assert(self);

    mc_jm_wait_for_id(self->jm, job_id);
    return GPOINTER_TO_INT(mc_jm_get_result(self->jm, job_id));
}

///////////////////


void mc_client_wait(mc_Client *self)
{
    g_assert(self);

    mc_jm_wait(self->jm);
}

///////////////////

bool mc_client_run(mc_Client *self, const char *command)
{
    return mc_client_recv(self, mc_client_send(self, command));
}

///////////////////

bool mc_client_command_list_is_active(mc_Client *self)
{
    g_assert(self);

    bool rc = false;

    g_mutex_lock(&self->command_list.is_active_mtx);
    rc = self->command_list.is_active;
    g_mutex_unlock(&self->command_list.is_active_mtx);

    return rc;
}

///////////////////

void mc_client_begin(mc_Client *self)
{
    g_assert(self);

    mc_client_send(self, "command_list_begin");
}

///////////////////

long mc_client_commit(mc_Client *self)
{
    g_assert(self);

    return mc_client_send(self, "command_list_end");
}
