#ifndef MC_DB_QUERY_PASRSER_HH
#define MC_DB_QUERY_PASRSER_HH


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
 *
 * @query: A String with query extensions.
 * @warning: (OUT) If input is invalid a warning is placed (still a result is returned)
 * @warning_pos: (OUT) If warning is filled, the position inside the string is written in here.
 * @returns: A newly allocated string containing a valid FTS Query.
 */
char * mc_store_qp_parse(const char *query, const char **warning, int *warning_pos);


#endif /* end of include guard: MC_DB_QUERY_PASRSER_HH */

