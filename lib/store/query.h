#include <glib.h>

/**
 * @brief An opaque structure representing a Store Query.
 *
 * In SQL Terms it would be something like this:
 *
 * select <columns> from songs where <where_clauses>;
 */

typedef struct
{
    GArray * columns;
    GArray * where_clauses;
} Store_Query;

/**
 * @brief Create a new Query
 *
 * @param search_string a specially encoded string defining the search.
 *
 * You can use the following syntax:
 *
 *   <search_term> := <term>|<search_term>
 *   <term>        := <column>[:<expression>]
 *   <column>      := A column-name in the song-table of the DB. <== TODO.
 *   <expression>  := Any SQL Expression that is applicable
 *
 * Basically, this string is parsed and put into a sql statement,
 * like this:
 *
 * select <column1>, <column2>, <column3> ... from songs where
 *        <column1> = <expression1>,
 *        <column2> = <expression2>
 *        -- expression3 was not given.
 *        ;
 *
 * @return
 */
Store_Query * store_query_create (const char * search_string);
