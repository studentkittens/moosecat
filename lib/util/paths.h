#ifndef MC_PATHS_HH
#define MC_PATHS_HH

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
int mc_path_get_depth (const char * dir_path);

#endif /* end of include guard: MC_PATHS_HH */

