#ifndef MOOSE_GZIP_HH
#define MOOSE_GZIP_HH

#include <glib.h>

G_BEGIN_DECLS

#define MOOSE_GZIP_ENDING ".zip"

/**
 * SECTION: moose-misc-gzip:
 * @short_description: `gzip/gunzip` like utility-functions.
 *
 * These two functions work like the standard unix gunzip/gzip tools.
 * They take a path to a file which has no .zip ending and compresses it.
 * The compressed file will be written to file_path.zip. The original file is
 * removed.
 */

/**
 * moose_gzip:
 * @file_path: Path to the file which should be zipped.
 *
 * Returns: True when the original file was removed.
 */
gboolean moose_gzip(const char * file_path);

/**
 * moose_gunzip:
 * @file_path: Path to the file which should be unzipped.
 *
 * The file_path needs a .gzip ending, otherwise it is ignored and
 * False is returned.
 *
 * Returns: True when the filepath could've been unzipped, and the .gzip file
 *          was removed.
 */
gboolean moose_gunzip(const char * file_path);

G_END_DECLS

#endif /* end of include guard: MOOSE_GZIP_HH */
