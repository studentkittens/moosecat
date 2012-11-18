#ifndef MC_GZIP_HH
#define MC_GZIP_HH

#include <stdbool.h>

#define MC_GZIP_ENDING ".zip"

/**
 * @brief Compresses file_path to file_path.gz
 *
 * Uses zlib
 *
 * @return true on success
 */
bool mc_gzip (const char * file_path);

/**
 * @brief Inflates file_path to file_path - gz
 *
 * Uses zlib
 *
 * @return  true on success
 */
bool mc_gunzip (const char * file_path);

#endif /* end of include guard: MC_GZIP_HH */
