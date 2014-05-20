#ifndef MOOSE_GZIP_HH
#define MOOSE_GZIP_HH

#include <stdbool.h>

#define MOOSE_GZIP_ENDING ".zip"

/**
 * @brief Compresses file_path to file_path.gz
 *
 * Uses zlib
 *
 * @return true on success
 */
bool moose_gzip(const char * file_path);

/**
 * @brief Inflates file_path to file_path - gz
 *
 * Uses zlib
 *
 * @return  true on success
 */
bool moose_gunzip(const char * file_path);

#endif /* end of include guard: MOOSE_GZIP_HH */
