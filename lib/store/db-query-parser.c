#include <glib.h>
#include <stdbool.h>
#include <string.h>


static const char *mc_store_qp_abbrev_list[] = {
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

///////////////////

static bool mc_store_qp_is_valid_tag(const char *tag, size_t len)
{
    static const char *tags[] = {
        "uri", "start", "end", "duration",
        "last_modifie", "artist", "album", "title",
        "album_artist", "track", "name", "genre",
        "composer", "performer", "comment", "disc",
        "musicbrainz_artist_id", "musicbrainz_album_id", "musicbrainz_albumartist_id", "musicbrainz_track",
        "queue_pos", "queue_idx", "always_dummy", "uri_depth", NULL
    };

    const char ** iter = tags;
    while(*iter != NULL) {
        if(strncmp(*iter, tag, MAX(len, strlen(*iter))) == 0) {
            g_print("%s\n", *iter);
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
    
    while((iter = g_utf8_find_prev_char(text, iter))) {
        if(g_utf8_get_char(iter) == ':') {
            char *pprev = g_utf8_find_prev_char(text, iter);
            if(pprev == NULL || g_utf8_get_char(pprev) != '\\') {
                return iter;
            }
        }
    }

    return NULL;
}

///////////////////

static const char *mc_store_qp_tag_abbrev_to_full(const char * token, size_t len) 
{
    if(len >= 1 && token[1] == ':') {
        if(*token < '~') {
            return mc_store_qp_abbrev_list[(int)*token];
        }
    }
    return NULL;
}

///////////////////

static void mc_store_qp_write_string(GString *output, const char *string, size_t len)
{
    const char *iter = string;
    
    while(*iter && (iter - string) < (int)len) {
        gunichar c = g_utf8_get_char(iter);
        switch(c) {
            case '"':
                break;
            default:
                g_string_append_unichar(output, c);
        }

        iter = g_utf8_next_char(iter);
    }
}

///////////////////

void mc_store_qp_process_single_word(const char *text, size_t len, GString *output)
{
    if(len < 1) {
        return;
    }

    g_string_append_c(output, '(');
    g_string_append(output, "artist:");
    mc_store_qp_write_string(output, text, len);
    g_string_append_c(output, '*');
    g_string_append(output, " OR ");
    g_string_append(output, "album_artist:");
    mc_store_qp_write_string(output, text, len);
    g_string_append_c(output, '*');
    g_string_append(output, " OR ");
    g_string_append(output, "album:");
    mc_store_qp_write_string(output, text, len);
    g_string_append_c(output, '*');
    g_string_append(output, " OR ");
    g_string_append(output, "title:");
    mc_store_qp_write_string(output, text, len);
    g_string_append_c(output, '*');
    g_string_append_c(output, ')');
}

///////////////////

#define WARNING(pwarn, ppost, msg, pos) \
    if(pwarn != NULL) *pwarn = msg;     \
    if(ppost != NULL) *ppost = pos;     \

///////////////////

bool mc_store_qp_process_text(const char *text, size_t len, bool prev_was_tag, GString *output, const char *query, const char **warning, int *warning_pos)
{
    bool is_tag = false;
    const char *tag_middle = NULL;

    if(strncmp(text, "NOT", len) == 0) {
        g_string_append(output, " NOT ");
    } else
    if(strncmp(text, "AND", len) == 0) {
        g_string_append(output, " AND ");
    } else
    if(strncmp(text, "OR", len) == 0) {
        g_string_append(output, " OR ");
    } else {
        if((tag_middle = mc_store_qp_is_tag_text(text, len)) != NULL) {
            const char *full = mc_store_qp_tag_abbrev_to_full(text, len);
            int rest_len = 0;

            if(full == NULL) {
                if(mc_store_qp_is_valid_tag(text, tag_middle - text) == false) {
                    WARNING(warning, warning_pos, "Warning: Invalid tag name.", (tag_middle - query));
                }
                mc_store_qp_write_string(output, text, tag_middle-text + 1);
            } else {
                g_string_append(output, full);
            }
            
            tag_middle++;
            rest_len = len - (tag_middle - text);

            /* is_tag is true if the expression is something like "a:" */
            is_tag = !rest_len;
            
            if(prev_was_tag && is_tag) {
                WARNING(warning, warning_pos, "Warning: Double Tag.", (tag_middle - query));
            }

            /* Skip : */
            mc_store_qp_write_string(output, tag_middle, rest_len);
        } else if(prev_was_tag) {
            mc_store_qp_write_string(output, text, len);
        } else {
            mc_store_qp_process_single_word(text, len, output);
        }

        g_string_append_unichar(output, ' ');
    }

    return is_tag;
}

///////////////////

static const char *mc_store_qp_special_char_to_full(char c)
{
    switch(c) {
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

bool mc_store_qp_check_is_soft_token(char *iter)
{
    gunichar c = g_utf8_get_char(iter);
    switch(c) {
        case '(': case ')':
            return true;
        default:
            return g_unichar_isspace(c);
    }
}

///////////////////

bool mc_store_qp_check_is_near_token(char *iter)
{
    gunichar c = g_utf8_get_char(iter);
    switch(c) {
        case '|': case '+': case '!': case '"':
            return true;
        default:
            return mc_store_qp_check_is_soft_token(iter);
    }
}

///////////////////


gunichar mc_store_qp_get_prev_char(const char *query, const char *iter)
{
    char *pprev = g_utf8_find_prev_char(query, iter);
    if(pprev != NULL) {
        return g_utf8_get_char(pprev);
    }

    return 0;
}

///////////////////

bool mc_store_qp_check_if_single_token(const char *query, char *iter)
{
    char *pprev = g_utf8_find_prev_char(query, iter);
    char *pnext = g_utf8_next_char(iter);
    bool left  = (pprev == NULL) || mc_store_qp_check_is_near_token(pprev);
    bool right = (pnext == NULL) || mc_store_qp_check_is_near_token(pnext);

    return left && right;
}

///////////////////

char * mc_store_qp_parse_full(const char *query, const char **warning, int *warning_pos)
{
    if (query == NULL || g_utf8_validate (query, -1, NULL) == false)
        return NULL;

    size_t query_len = strlen(query);
    GString *output = g_string_sized_new(query_len);

    bool prev_was_tag = false;
    char *iter = (char *)query;

    /* Static Query Check */
    int check_bracket_balance = 0, check_quote_counter = 0;
    bool check_at_end = true;

    while(*iter) {
        gunichar c = g_utf8_get_char(iter);
        gunichar prev_char = mc_store_qp_get_prev_char(query, iter);

        if(!mc_store_qp_check_is_soft_token(iter)) {
            switch(c) {
                case '*':
                    if (mc_store_qp_check_if_single_token(query, iter)) {
                        g_string_append(output, "always_dummy:0 ");
                        prev_was_tag = false;
                    } else if(prev_was_tag || prev_char == '\\') {
                        g_string_append_unichar(output, c);
                    }
                    check_at_end = false;
                    break;
                case '+':
                case '|':
                case '!':
                    if(prev_was_tag == true || check_at_end == true) {
                        WARNING(warning, warning_pos, "Warning: (AND OR NOT) should not be placed there.", iter - query + 1);
                    }
                    check_at_end = true;

                    if(prev_char == '\\') {
                        g_string_append_unichar(output, c);
                    } else {
                        g_string_append(output, mc_store_qp_special_char_to_full(c));
                        prev_was_tag = false;
                    }
        

                    break;
                case '"':
                    check_quote_counter++;
                    break;
                default: {
                    int token_len = 0;
                    char *end_token = iter; 
                    
                    check_at_end = false;

                    while(true) {
                        end_token = strpbrk(end_token, " ()\t\r+|!*");
                        if(end_token != NULL) {
                            if(mc_store_qp_get_prev_char(query, end_token) == '\\') {
                                end_token++;
                                continue;
                            } else {
                                token_len = end_token - iter;
                            }
                        } else {
                            token_len = (query + query_len) - iter;
                        }
                        break;
                    }

                    prev_was_tag = mc_store_qp_process_text(iter, token_len, prev_was_tag, output, query, warning, warning_pos);
                    iter += MAX(0, token_len - 1);
                    break;
                }
            }
        } else {
            switch(c) {
                case '(': case ')':
                    if(c == '(') {
                        check_at_end = true;
                        check_bracket_balance++;
                    } else {
                        if(check_at_end) {
                            WARNING(warning, warning_pos, "Warning: Dangling (AND OR NOT) at closing bracket.", iter - query);
                        }
                        check_bracket_balance--;
                    }

                    if(check_bracket_balance < 0) {
                        WARNING(warning, warning_pos, "Warning: Closing bracket before opening.", (iter - query) + 1); 
                    }
                    g_string_append_unichar(output, c);
                    prev_was_tag = false;
            }
        }

        if(*iter == 0) {
            break;
        } else {
            iter = g_utf8_next_char(iter);
        }
    }

    if(check_at_end) {
        WARNING(warning, warning_pos, "Warning: Dangling (AND OR NOT) at end of string.", (iter - query));
    }

    if(check_bracket_balance != 0) {
        if(check_bracket_balance > 0) {
            WARNING(warning, warning_pos, "Warning: Too many opening brackets.", (iter - query));
        } else {
            WARNING(warning, warning_pos, "Warning: Too many closing brackets.", (iter - query));
        }
    }

    return g_strstrip(g_string_free(output, false));
}

///////////////////

char * mc_store_qp_parse(const char *query)
{
    return mc_store_qp_parse_full(query, NULL, NULL);
}

///////////////////

#if 0
int main(int argc, char const *argv[])
{
    if(argc < 2)
        return -1;

    int pos = -1;
    const char *warning = NULL;
    char *query = mc_store_qp_parse_full(argv[1], &warning, &pos);


    if(warning == NULL) {
        g_print("%s\n", query);
    } else {
        g_print("W: %s\n", argv[1]);
        for(int i = 0; i < pos + 2; i++) {
            g_print(" ");
        }
        g_print("^\n");
        for(int i = 0; i < pos + 2; i++) {
            g_print(" ");
        }
        g_print("%s\n", warning);
        g_print("\nHere's the generated output anyway:\n  %s\n", query);
    }
    
    g_free(query);
    return 0;
}
#endif
