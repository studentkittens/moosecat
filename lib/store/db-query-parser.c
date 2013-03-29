#include <glib.h>
#include <stdbool.h>
#include <string.h>


static const char *abbrev_list[] = {
    ['a'] = "artist",
    ['b'] = "album",
    ['c'] = "album_artist",
    ['d'] = "duration",
    ['g'] = "genre",
    ['n'] = "name",
    ['p'] = "performer",
    ['r'] = "track",
    ['s'] = "disc",
    ['t'] = "title",
    ['u'] = "uri",
    ['~'] = NULL
};

///////////////////

static bool mc_store_qp_is_tag_token(const char *token)
{
    int len = strlen(token);
    if (len == 0) 
        return false;

    for(int i = len - 1; i != 0; --i) {
        if(token[i] == ':') {
            if(i-1 >= 0 && token[i-1] != '\\') {
                return true;
            }
        }
    }

    return false;
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
        case '-':
            return " NOT ";
        default:
            return "";
    }
}

///////////////////

static char *mc_store_qp_sanitize(const char *token)
{
    if (token == NULL || g_utf8_validate (token, -1, NULL) == false)
        return NULL;

    GString *output = g_string_new("");
    const char *iter = token;

    while(*iter) {
        gunichar c = g_utf8_get_char(iter);

        switch(c) {
            case '\\':
                /* Copy next char manually */
                iter = g_utf8_next_char(iter);
                if(*iter) {
                    g_string_append_unichar(output, g_utf8_get_char(iter));
                } else {
                    goto success;
                }
                break;
            default:
                g_string_append_unichar(output, c);
                break;
        }

        iter = g_utf8_next_char(iter);
    } 

    success:
        return g_string_free(output, false);
}

///////////////////

static char *mc_store_qp_tag_abbrev_to_full(const char * token) 
{
    char *output = NULL;
    if (token[1] == ':') {
        const char * full = abbrev_list[(int)*token];
        if(full != NULL) {
            char *sane = mc_store_qp_sanitize(strrchr(token, ':') + 1);
            output = g_strdup_printf("%s:%s", full, sane);
            g_free(sane);
        }
    }

    /* take old token hoping it's correct */
    return output;
}

///////////////////

static char *mc_store_qp_construct_default_or_clause(const char *sane_token)
{
    return g_strdup_printf("(artist:%s OR album:%s OR title:%s)",
            sane_token,
            sane_token,
            sane_token);
}

///////////////////

static bool mc_store_qp_process_token(const char *token, bool prev_was_tag, GString **output) 
{
    bool cur_is_tag = mc_store_qp_is_tag_token(token);
    bool do_sanitize = !cur_is_tag && !prev_was_tag;

    if (cur_is_tag && prev_was_tag) {
        /* Uhm. Wrong you did! Skip this. */
    } else if (cur_is_tag && !prev_was_tag) {
        /* It is a tag spec, make sure its valid */
        char *sane_tag = mc_store_qp_tag_abbrev_to_full(token);
        if(sane_tag != NULL) {
            *output = g_string_append(*output, sane_tag);
            g_free(sane_tag);
        } else {
            do_sanitize = true;
        }
    } 
    
    if(do_sanitize) {
        char *sanitized = mc_store_qp_sanitize(token);
        if (sanitized != NULL) {
            if(!cur_is_tag && !prev_was_tag) {
                char *or_clause = mc_store_qp_construct_default_or_clause(sanitized);
                *output = g_string_append(*output, or_clause);
                g_free(or_clause);
            } else {
                *output = g_string_append(*output, sanitized);
            }

            g_free(sanitized);

            /* Make it a bit more readable */
            if(prev_was_tag) {
                g_string_append_c(*output, ' ');
            }
        }
    }

    return cur_is_tag;
}

///////////////////

char * mc_store_qp_parse(const char *query)
{
    if (query != NULL) {
        /* Bit of a hack: End the string with a delimiter */
        char *cpy_query = g_strdup_printf("%s ", query);

        GString *output = g_string_new("");
        bool prev_was_tag = false;
        
        char *iter = (char *)cpy_query;
        char *token = iter;

        while((iter = strpbrk(iter, " \t()-|!+#"))) {
            char copy_char = 0;
            bool whitespace = false;
            switch(*iter) {
                    break;
                case '(':
                case ')':
                    /* Just copy them to the result */ 
                    copy_char = *iter;
                    break;
                case '+':
                case '|':
                case '-':
                    prev_was_tag = false;
                    g_string_append(output, mc_store_qp_special_char_to_full(*iter));
                    break;
                case '#':
                    prev_was_tag = false;
                    g_string_append(output, "always_dummy:0 ");
                    break;
                case '\t':
                case ' ':
                    whitespace = true;
                default:
                    /* Do not add those to the output */
                    break;
            }
            
            /* "Split" string and jump to next token */
            *iter++ = 0;

            if (token != iter && *token) {
                prev_was_tag = mc_store_qp_process_token(token, prev_was_tag, &output);
                if(whitespace) {
                    output = g_string_append_c(output, ' ');
                }
            }

            if (copy_char) {
                output = g_string_append_c(output, copy_char);
            }

            token = iter;
        }

        g_free((char*)cpy_query);
        return  g_string_free(output, false);
    }
    return NULL;
}
