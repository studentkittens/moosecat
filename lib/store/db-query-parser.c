#include <glib.h>
#include <stdbool.h>
#include <string.h>


// TODO: What about Phrase Queries? "Word_a Word_b" returns Word_a OR Word_b currently

/* Stores everything the parinsg routines need to know */
typedef struct {
    /* Pointer to start of query */
    const char *query;

    /* Pointer to current element */
    const char *iter;

    /* strlen(query) */
    size_t query_len;

    /* Output Buffer */
    GString *output;

    /* Last character investigated */
    gunichar current_char;
    gunichar previous_char;

    struct {
        /* Previous token was a tag (a:) */
        bool was_tag;

        /* Missing operable before operand */
        bool bad_conjunction;

        /* Balance of brackets (should be 0) */
        int bracket_balance;
    } check;

    /* For warning messages */
    struct  {
        int *pos;
        const char **msg;
    } warning;

} mc_StoreParseData;

///////////////////

#define WARNING(data, pmsg)                                                        \
    if(data->warning.msg != NULL) *data->warning.msg = pmsg;                       \
    if(data->warning.pos != NULL) *data->warning.pos = (data->iter - data->query); \
 
///////////////////

static bool mc_store_qp_is_valid_tag(const char *tag, size_t len)
{
    static const char *tags[] = {
        "uri", "start", "end", "duration",
        "last_modifie", "artist", "album", "title",
        "album_artist", "track", "name", "genre",
        "composer", "performer", "comment", "disc",
        "musicbrainz_artist_id", "musicbrainz_album_id",
        "musicbrainz_albumartist_id", "musicbrainz_track",
        "queue_pos", "queue_idx", "always_dummy", "uri_depth",
        NULL
    };

    const char **iter = tags;

    while (*iter != NULL) {
        if (strncmp(*iter, tag, MAX(len, strlen(*iter))) == 0) {
            return true;
        }

        ++iter;
    }

    return false;
}

///////////////////

static const char *mc_store_qp_is_tag_text(const char *text, size_t len)
{
    if (len == 0)
        return NULL;

    char *iter = (char *)text + len;;

    while ((iter = g_utf8_find_prev_char(text, iter))) {
        if (g_utf8_get_char(iter) == ':') {
            char *pprev = g_utf8_find_prev_char(text, iter);

            if (pprev == NULL || g_utf8_get_char(pprev) != '\\') {
                return iter;
            }
        }
    }

    return NULL;
}

///////////////////

static const char *mc_store_qp_tag_abbrev_to_full(const char *token, size_t len)
{
    static const char *abbrev_list[] = {
        ['a'] = "artist:",
        ['b'] = "album:",
        ['c'] = "album_artist:",
        ['d'] = "duration:",
        ['g'] = "genre:",
        ['n'] = "name:",
        ['p'] = "performer:",
        ['r'] = "track:",
        ['s'] = "disc:",
        ['t'] = "title:",
        ['u'] = "uri:",
        ['~'] = NULL
    };

    if (len >= 1 && token[1] == ':') {
        if (*token < '~') {
            return abbrev_list[(int)*token];
        }
    }

    return NULL;
}

///////////////////

static void mc_store_qp_write_string(mc_StoreParseData *data, const char *string, size_t len)
{
    const char *iter = string;

    while (*iter && (iter - string) < (int)len) {
        gunichar c = g_utf8_get_char(iter);

        switch (c) {
        default:
            g_string_append_unichar(data->output, c);
        }

        iter = g_utf8_next_char(iter);
    }
}

///////////////////

void mc_store_qp_process_single_word(mc_StoreParseData *data, const char *text, size_t len)
{
    if (len < 1) {
        return;
    }

#define TAG(name, last)                            \
        g_string_append(data->output, name);       \
        mc_store_qp_write_string(data, text, len); \
        if(text[len-1] != '*') {                   \
            g_string_append_c(data->output, '*');  \
        }                                          \
        if(!last) {                                \
            g_string_append(data->output, " OR "); \
        }                                          \
 
    g_string_append_c(data->output, '(');
    {
        TAG("artist:", false);
        TAG("album_artist:", false);
        TAG("album:", false);
        TAG("title:", true);
    }
    g_string_append_c(data->output, ')');
}

///////////////////

void mc_store_qp_process_text(mc_StoreParseData *data, const char *text, size_t len)
{
    bool is_tag = false;
    const char *tag_middle = NULL;

    if ((tag_middle = mc_store_qp_is_tag_text(text, len)) != NULL) {
        const char *full = mc_store_qp_tag_abbrev_to_full(text, len);
        int rest_len = 0;

        if (full == NULL) {
            if (mc_store_qp_is_valid_tag(text, tag_middle - text) == false) {
                WARNING(data, "Invalid tag name.");
            }

            mc_store_qp_write_string(data, text, tag_middle-text + 1);
        } else {
            g_string_append(data->output, full);
        }

        tag_middle++;
        rest_len = len - (tag_middle - text);

        /* is_tag is true if the expression is something like "a:" */
        is_tag = !rest_len;

        if (data->check.was_tag && is_tag) {
            WARNING(data, "After a tag, there must not be another tag.");
        }

        /* Skip : */
        mc_store_qp_write_string(data, tag_middle, rest_len);
        g_string_append_unichar(data->output, ' ');
    } else if (data->check.was_tag) {
        mc_store_qp_write_string(data, text, len);
    } else {
        mc_store_qp_process_single_word(data, text, len);
        g_string_append_unichar(data->output, ' ');
    }

    data->check.was_tag = is_tag;
}

///////////////////



static const char *mc_store_qp_special_char_to_full(char c)
{
    switch (c) {
    case '+':
        return " AND ";
    case '|':
        return " OR ";
        break;
    case '!':
        return " NOT ";
    default:
        return "";
    }
}

///////////////////

bool mc_store_qp_check_is_soft_token(const char *iter)
{
    gunichar c = g_utf8_get_char(iter);

    switch (c) {
    case '(':
    case ')':
        return true;
    default:
        return g_unichar_isspace(c);
    }
}

///////////////////

bool mc_store_qp_check_is_near_token(char *iter)
{
    gunichar c = g_utf8_get_char(iter);

    switch (c) {
    case '|':
    case '+':
    case '!':
    case '\0':
        return true;

    default:
        return mc_store_qp_check_is_soft_token(iter);
    }
}

///////////////////

gunichar mc_store_qp_get_prev_char(mc_StoreParseData *data, const char *current_pos)
{
    char *pprev = g_utf8_find_prev_char(data->query, current_pos);

    if (pprev != NULL) {
        return g_utf8_get_char(pprev);
    }

    return 0;
}

///////////////////

bool mc_store_qp_check_if_single_token(mc_StoreParseData *data)
{
    char *pprev = g_utf8_find_prev_char(data->query, data->iter);
    char *pnext = g_utf8_next_char(data->iter);

    bool left  = (pprev == NULL) || mc_store_qp_check_is_near_token(pprev);
    bool right = (pnext == NULL) || mc_store_qp_check_is_near_token(pnext);

    return left && right;
}

///////////////////

void mc_store_qp_process_star(mc_StoreParseData *data)
{
    /* If a star is a single word it matches everything */
    if (mc_store_qp_check_if_single_token(data)) {
        g_string_append(data->output, "always_dummy:0 ");
        data->check.was_tag = false;
    } else if (data->previous_char == '\\') {
        g_string_append_unichar(data->output, '*');
    }

    /* An operator may follow. */
    data->check.bad_conjunction = false;
}

///////////////////

void mc_store_qp_process_operand(mc_StoreParseData *data)
{
    /* You cannot do something like "a: AND b",
     * Since an empty tag is not valid */
    if (data->check.was_tag == true) {
        WARNING(data, "(AND OR NOT) should not be placed after a tag.");
    }

    /* If no operable was found before, we assume it is a "*" */
    if (data->check.bad_conjunction == true) {
        g_string_append(data->output, "always_dummy:0 ");
    }

    /* Prevent double operators from being possible e.g. "AND AND" */
    data->check.bad_conjunction = true;

    /* Handle escaping */
    if (data->previous_char == '\\') {
        g_string_append_unichar(data->output, data->current_char);
    } else {
        g_string_append(data->output, mc_store_qp_special_char_to_full(data->current_char));
        data->check.was_tag = false;
    }
}

///////////////////

void mc_store_qp_process_token(mc_StoreParseData *data)
{
    int token_len = 0;
    const char *end_token = data->iter;

    for(;;) {
        /* Jump to the next token */
        end_token = strpbrk(end_token, " ()\t\r+|!");

        /* If one was found, we may need to repeat, due to a escaping \ */
        if (end_token != NULL) {
            if (mc_store_qp_get_prev_char(data, end_token) == '\\') {
                end_token++;
                continue;
            } else {
                token_len = end_token - data->iter;
            }
        } else {
            /* We're at the end of the string, so see it at the last token */
            token_len = (data->query + data->query_len) - data->iter;
        }

        break;
    }


    if (strncmp(data->iter, "NOT", token_len) == 0 ||
            strncmp(data->iter, "AND", token_len) == 0 ||
            strncmp(data->iter, "OR", token_len)  == 0) {
        /* If no operable was found before, we assume it is a "*" */
        if (data->check.bad_conjunction == true) {
            g_string_append(data->output, "always_dummy:0 ");
        }

        /* Append the output */
        g_string_append_unichar(data->output, ' ');
        g_string_append_len(data->output, data->iter, token_len);
        g_string_append_unichar(data->output, ' ');
        data->check.bad_conjunction = true;
    } else {
        /* Next token to parse is assumed to be
         * valid to (AND OR NOT) */
        data->check.bad_conjunction = false;
        mc_store_qp_process_text(data, data->iter, token_len);
    }

    /* Jump over the parsed area */
    data->iter += MAX(0, token_len - 1);
}

///////////////////

void mc_store_qp_process_empty(mc_StoreParseData *data)
{
    switch (data->current_char) {
        /* Make some statistics */
    case '(':
    case ')':
        if (data->current_char == '(') {
            /* After a ( no (AND NOT OR) should come */
            data->check.bad_conjunction = true;
            data->check.bracket_balance++;
        } else {
            /* Somebody forgot to give a (AND NOT OR) a r-operand */
            if (data->check.bad_conjunction) {
                WARNING(data, "Dangling (AND OR NOT) at closing bracket.");
            }

            data->check.bracket_balance--;
        }

        /* Obviously, a ) might not come before a ( */
        if (data->check.bracket_balance < 0) {
            WARNING(data, "Closing bracket before opening.");
        }

        /* Add it to the string, no parsing done with brackets otherwise */
        g_string_append_unichar(data->output, data->current_char);

        if (data->check.was_tag) {
            WARNING(data, "Unclosed tag before/after bracket.");
        }

        /* A tag may follow after thisnow */
        data->check.was_tag = false;
    }
}

///////////////////

bool mc_store_qp_str_is_empty(const char *str) 
{
    if (str != NULL) {
        while(*str) {
            if(!g_unichar_isspace(g_utf8_get_char(str))) {
                return false;
            }
            str = g_utf8_next_char(str);
        }
    }

    return true;
}

///////////////////

static gboolean mc_store_qp_quote_eval_cb(const GMatchInfo *info, GString  *res, gpointer data) 
{
    char * tag = g_match_info_fetch(info, 1);
    if(tag == NULL || *tag == 0) {
        g_free(tag);
        tag = g_match_info_fetch(info, 2);
    }

    if(tag == NULL || *tag == 0) {
        g_free(tag);
        return FALSE;
    }

    tag = g_strstrip(tag);

    char * content = g_match_info_fetch(info, 3);
    char ** vector = g_strsplit(content, " ", -1);

    if(*content) {
        g_string_append(res, " (");
        for(int i = 0; vector[i]; i++) {
            g_string_append(res, tag);
            g_string_append(res, g_strstrip(vector[i]));
            g_string_append(res, " ");
        }
        g_string_append(res, ") ");
    } 
    
    g_free(tag);
    g_free(content);
    g_strfreev(vector);
    return FALSE;
}


static char * mc_store_qp_preprocess_quotationmarks(const char *query) 
{
    if(query == NULL) 
        return NULL;

    GRegex * regex = g_regex_new(
        "(\\w:\\s*|)\"(\\s*\\w:\\s*|)(.*?)\"", 0, 0, NULL
    );

    char * result = g_regex_replace_eval(
        regex, query, -1, 0, 0, mc_store_qp_quote_eval_cb, NULL, NULL
    );
    g_regex_unref(regex);
    return result;
}

///////////////////

static gboolean mc_store_qp_range_eval_cb(const GMatchInfo *info, GString  *res, gpointer data) 
{
    char * tag = g_match_info_fetch(info, 1);
    if(tag != NULL && *tag == 0) {
        g_free(tag);
        tag = NULL;
    } else {
        tag = g_strstrip(tag);
    }

    char *start_str = g_match_info_fetch(info, 2);
    char *stop_str = g_match_info_fetch(info, 3);

    long start = g_ascii_strtoll(start_str, NULL, 10);
    long stop = g_ascii_strtoll(stop_str, NULL, 10);

    /* Limit the max range to 1000,
     * Values bigger than that confuse the shit out of 
     * */
    if(start < stop && (ABS(start - stop) <= 1000)) {
        g_string_append(res, " (");
        for(long i = start; i < stop; i++) {
            if(tag != NULL) {
                g_string_append(res, tag);
            }
            g_string_append_printf(res, "%ld", i);

            if((i + 1) < stop) {
                g_string_append(res, " OR ");
            }
        }
        g_string_append(res, ") ");
    }

    g_free(tag); g_free(start_str); g_free(stop_str);
    return FALSE;
}

/* Note: 
 *
 * The implemenation sucks and you will laugh out loud.
 *
 * I was too lazy to filter the songs that were queries (which would be faster),
 * since this would require some postprocessing step.
 *
 * This works well enough for the small numbers we usually have for music.
 * */
static char * mc_store_qp_preprocess_ranges(const char *query) 
{
    if(query == NULL) 
        return NULL;

    GRegex * regex = g_regex_new(
        "(\\w:|)(\\d+)\\.\\.(\\d+)", 0, 0, NULL
    );

    char * result = g_regex_replace_eval(
        regex, query, -1, 0, 0, mc_store_qp_range_eval_cb, NULL, NULL
    );
    g_regex_unref(regex);
    return result;
}

///////////////////

static char * mc_store_qp_preprocess(const char *query)
{
    char * step_one = mc_store_qp_preprocess_quotationmarks(query);
    char * step_two = mc_store_qp_preprocess_ranges(step_one);
    g_free(step_one);
    return step_two;
}

///////////////////

char *mc_store_qp_parse(const char *query, const char **warning, int *warning_pos)
{
    mc_StoreParseData sdata;
    mc_StoreParseData *data = &sdata;
    memset(data, 0, sizeof(mc_StoreParseData));

    /* Make warnings work */
    data->warning.msg = warning;
    data->warning.pos = warning_pos;

    if (query == NULL)
        return NULL;

    if (g_utf8_validate(query, -1, NULL) == false) {
        WARNING(data, "Invalid utf-8 query.");
        return NULL;
    }

    if(mc_store_qp_str_is_empty(query)) {
        return NULL;
    }

    /* Everything else is 0 for now */
    data->query = mc_store_qp_preprocess(query);
    data->query_len = strlen(query);
    data->iter = data->query;
    data->output = g_string_sized_new(data->query_len);

    /* "runtime" checks */
    data->check.bad_conjunction = true;

    /* Loop over all utf8 glyphs */
    while (*(data->iter)) {
        data->current_char = g_utf8_get_char(data->iter);
        data->previous_char = mc_store_qp_get_prev_char(data, data->iter);

        if (!mc_store_qp_check_is_soft_token(data->iter)) {
            switch (data->current_char) {
            case '*':
                mc_store_qp_process_star(data);
                break;
            case '+':
            case '|':
            case '!':
                mc_store_qp_process_operand(data);
                break;

            default: {
                mc_store_qp_process_token(data);
                break;
            }
            }
        } else {
            mc_store_qp_process_empty(data);
        }

        /* Hop to the next char or stop */
        if (*data->iter == 0) {
            break;
        } else {
            data->iter = g_utf8_next_char(data->iter);
        }
    }

    /*
     * A last error checking parcour.
     * Basically all the stuff we can only have a statistic during
     * runtime, but know how it went after parsing
     */
    if (data->check.bad_conjunction) {
        WARNING(data, "Dangling (AND OR NOT) at end of string.");
    }

    if (data->check.was_tag) {
        WARNING(data, "Unfilled tag specification at end of string.");
    }

    if (data->check.bracket_balance != 0) {
        if (data->check.bracket_balance > 0) {
            WARNING(data, "Too many opening brackets.");
        } else {
            WARNING(data, "Too many closing brackets.");
        }
    }

    g_free((char *)data->query);
    return g_strstrip(g_string_free(data->output, false));
}
