#ifndef MOOSE_PATHS_HH
#define MOOSE_PATHS_HH

/* @brief Count the 'depth' of a path.
 * @param dir_path the path to count
 *
 * Examples:
 *
 *  Path:     Depth:
 *  /         0
 *  a/        0
 *  a         0
 *  a/b       1
 *  a/b/      1
 *  a/b/c     2
 *
 *  @return the depth.
 */
int moose_path_get_depth(const char *dir_path);

#endif /* end of include guard: MOOSE_PATHS_HH */

