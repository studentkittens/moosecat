#ifndef MOOSE_DB_QUERY_PASRSER_HH
#define MOOSE_DB_QUERY_PASRSER_HH

#include "../mpd/moose-song.h"

G_BEGIN_DECLS

/**
 * SECTION: moose-store-query-parser
 * @short_description: Special syntax for a lightweight query language
 *
 * Provide extended Syntax for FTS Query.
 *
 * Query may contain following things:
 *
 *    # : Select everything
 *    - : Negate the following term (NOT)
 *    | : logic OR the following term
 *    + : logic AND the following term
 *
 * Things already found in normal FTS Queries:
 *
 *    ():  Overwrite precedence of a expression.
 *    artist: Search for artist only. There are more tag-specifiers,
 *    which also may just be written with only one letter (a: for artist:):
 *
 *          ['a'] = "artist"
 *          ['b'] = "album"
 *          ['c'] = "album_artist"
 *          ['d'] = "duration"
 *          ['g'] = "genre"
 *          ['n'] = "name"
 *          ['p'] = "performer"
 *          ['r'] = "track"
 *          ['s'] = "disc"
 *          ['t'] = "title"
 *          ['u'] = "uri"
 *
 * Note: The database stores some more tags - these work too, but are not
 *       important for the user.
 *
 * Additionally an expression without a tag gets converted to this:
 *
 *    Kno* ->  (artist:Kno* OR album:Kno* OR title:Kno*)
 *
 * Example:
 *
 *      # -Jasper     : Search everything but exclude Jasper.
 *
 * Bugs:
 *
 *      Many.
 *
 * This assumes that SQLite was compiled with -DSQLITE_ENABLE_FTS3_PARENTHESIS
 * Moosecat comes with it's custom SQLITE bundled, so don't worry.
 */

/**
 * moose_store_qp_parse:
 * @query: A String with query extensions.
 * @warning: (out): If input is invalid a warning is placed (still a result is returned)
 * @warning_pos: (out): If warning is filled, the position inside the string is written in here.
 *
 * Parses a MooseQuerty into a valid FTS-Query. If the compilation fails,
 * @warning and @warning_pos are filled appropiately. All Store functions
 * use this function implicitly. You can use this function to validate the
 * query without executing it.
 *
 * Returns: (transfer full): A newly allocated string containing a valid FTS Query.
 */
char * moose_store_qp_parse(const char * query, const char ** warning, int * warning_pos);

/**
 * moose_store_qp_tag_abbrev_to_full:
 * @token: The token to convert.
 * @len: The length of the tag.
 *
 * Tags may be abbreviated to save time. This function gives you the full
 * version. "artist:" may be written as "a:".
 *
 * Returns: (transfer none): The full tag-name
 */
const char * moose_store_qp_tag_abbrev_to_full(const char * token, size_t len);

/**
 * moose_store_qp_is_valid_tag:
 * @tag: A tag-name.
 * @len: length of the tag.
 *
 * Also works for abbreviated tags.
 *
 * Returns: True if it is an valid tag.
 */
gboolean moose_store_qp_is_valid_tag(const char * tag, size_t len);

/**
 * moose_store_qp_str_to_tag_enum:
 * @tag: Convert a tag string (as used in the parser) to a MOOSE_TAG_* ID.
 *
 * Returns: a #MooseTagType
 */
MooseTagType moose_store_qp_str_to_tag_enum(const char * tag);

G_END_DECLS

#endif /* end of include guard: MOOSE_DB_QUERY_PASRSER_HH */
