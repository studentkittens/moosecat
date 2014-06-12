#include <glib.h>
#include <glib/gstdio.h>
#include <gio/gio.h>

#include <stdio.h>
#include <string.h>

#include "../moose-config.h"
#include "moose-misc-gzip.h"

#define CHUNK_SIZE (2 << 14)  /* 32KB */

/*
 * If compress is TRUE:
 *    Take src as input and write zipped version at dst.
 * Else:
 *    Take src as input, and write unzipped version at dst.
 */
static gboolean transform(const char * src, const char * dst, gboolean compress) {
    GError * error = NULL;
    gsize bytes_read = 0;
    gboolean result = TRUE;
    GConverter * converter = NULL;
    GOutputStream * dst_stream = NULL;
    GInputStream * src_stream = NULL;
    GObject * conv_stream = NULL;

    GFile * src_file = g_file_new_for_path(src);
    GFile * dst_file = g_file_new_for_path(dst);

    src_stream = G_INPUT_STREAM(g_file_read(src_file, NULL, &error));
    if (src_stream == NULL) {
        moose_warning("Cannot create input stream: %s\n", error->message);
        g_error_free(error);
        result = FALSE;
        goto free_all;
    }

    dst_stream = G_OUTPUT_STREAM(g_file_create(
                                     dst_file,
                                     G_FILE_CREATE_REPLACE_DESTINATION | G_FILE_CREATE_PRIVATE,
                                     NULL, &error
                                 ));

    if (dst_stream == NULL) {
        moose_warning("Cannot create output stream: %s\n", error->message);
        g_error_free(error);
        result = FALSE;
        goto free_all;
    }

    GOutputStream * sink = dst_stream;
    GInputStream * source = src_stream;

    if (compress) {
        /* Use Fast compression */
        converter = G_CONVERTER(
                        g_zlib_compressor_new(G_ZLIB_COMPRESSOR_FORMAT_GZIP, 1)
                    );
        conv_stream = G_OBJECT(
                          g_converter_output_stream_new(G_OUTPUT_STREAM(dst_stream), converter)
                      );
        sink = G_OUTPUT_STREAM(conv_stream);
    } else {
        converter = G_CONVERTER(
                        g_zlib_decompressor_new(G_ZLIB_COMPRESSOR_FORMAT_GZIP)
                    );
        conv_stream = G_OBJECT(
                          g_converter_input_stream_new(G_INPUT_STREAM(src_stream), converter)
                      );
        source = G_INPUT_STREAM(conv_stream);
    }

    unsigned char buffer[CHUNK_SIZE];
    while ((bytes_read = g_input_stream_read(source, buffer, CHUNK_SIZE, NULL, &error)) > 0) {
        g_output_stream_write(sink, buffer, bytes_read, NULL, &error);
        if (error != NULL) {
            moose_warning("Error during writing: %s\n", error->message);
            g_error_free(error);
            error = NULL;
            result = FALSE;
            break;
        }
    }

free_all:
    if (src_stream) {
        g_object_unref(src_stream);
    }
    if (dst_stream) {
        g_object_unref(dst_stream);
    }
    if (src_file) {
        g_object_unref(src_file);
    }
    if (dst_file) {
        g_object_unref(dst_file);
    }
    if (converter) {
        g_object_unref(converter);
    }
    if (conv_stream) {
        g_object_unref(conv_stream);
    }
    return result;
}

gboolean moose_gzip(const char * file_path) {
    g_assert(file_path != NULL);
    gboolean result = FALSE;

    if (!g_str_has_suffix(file_path, MOOSE_GZIP_ENDING)) {
        char * with_ending = g_strdup_printf("%s%s", file_path, MOOSE_GZIP_ENDING);
        result = transform(file_path, with_ending, TRUE);
        g_remove((result) ? file_path : with_ending);
        g_free(with_ending);
    }
    return result;
}

gboolean moose_gunzip(const char * file_path) {
    g_assert(file_path != NULL);
    gboolean result = FALSE;

    if (g_str_has_suffix(file_path, MOOSE_GZIP_ENDING)) {
        char * with_ending = g_strndup(file_path, strlen(file_path) - strlen(MOOSE_GZIP_ENDING));
        result = transform(file_path, with_ending, FALSE);
        g_remove((result) ? file_path : with_ending);
        g_free(with_ending);
    }
    return result;
}

#if 0

int main(int argc, char const * argv[]) {
    if (argc < 3) {
        moose_message("Usage:\n");
        moose_message("  %s compress <file>\n", argv[0]);
        moose_message("  %s decompress <file>\n", argv[0]);
        return -1;
    }

    if (!strcmp(argv[1], "compress")) {
        if (moose_gzip(argv[2]) == FALSE) {
            moose_message("Nope.\n");
        }
    } else if (!strcmp(argv[1], "decompress")) {
        if (moose_gunzip(argv[2]) == FALSE) {
            moose_message("Nope.\n");
        }
    }

    return 0;
}

#endif
