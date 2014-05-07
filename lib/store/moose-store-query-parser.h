#ifndef MOOSE_DB_QUERY_PASRSER_HH
#define MOOSE_DB_QUERY_PASRSER_HH


/* Provide extended Syntax for FTS Query.
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
 *
 * @query: A String with query extensions.
 * @warning: (OUT) If input is invalid a warning is placed (still a result is returned)
 * @warning_pos: (OUT) If warning is filled, the position inside the string is written in here.
 * @returns: A newly allocated string containing a valid FTS Query.
 */
char *moose_store_qp_parse(const char *query, const char **warning, int *warning_pos);

/**
 * @brief Convert a abbrev-tag to a full-tag.
 *
 * Tags may be abbreviated to save time. This function gives you the full
 * version.
 *
 * @param token The abbrev-tag. 
 * @param len the length of the tag.
 *
 * @return A static string. Do not free.
 */
const char *moose_store_qp_tag_abbrev_to_full(const char *token, size_t len);

/**
 * @brief Check if this is an valid tag.
 *
 * @param tag The tag to check.
 * @param len The length of the tag.
 *
 * @return True if a known tag.
 */
bool moose_store_qp_is_valid_tag(const char *tag, size_t len);

/**
 * @brief Convert a tag string (as used in the parser) to a MPD_TAG_* ID.
 *
 * @param tag The string to parse. 
 *
 * @return a member of mpd_tag_type.
 */
enum mpd_tag_type moose_store_qp_str_to_tag_enum(const char *tag);


#endif /* end of include guard: MOOSE_DB_QUERY_PASRSER_HH */

