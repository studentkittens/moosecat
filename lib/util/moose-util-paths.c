#include <glib.h>

int moose_path_get_depth(const char *dir_path)
{
    int dir_depth = 0;
    char *cursor = (char *) dir_path;

    if (dir_path == NULL)
        return -1;

    for (;;) {
        gunichar curr = g_utf8_get_char_validated(cursor, -1);
        cursor = g_utf8_next_char(cursor);

        if (*cursor != (char)0) {
            if (curr == (gunichar)'/')
                ++dir_depth;
        } else {
            break;
        }
    }

    return dir_depth;
}