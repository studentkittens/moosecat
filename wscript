#!/usr/bin/env python
#encoding: utf-8
import os
import traceback

# Global:

APPNAME = 'moosecat'
VERSION = '0.0.1'

# Needed flags:
CFLAGS = ['-std=c99']

# Optional flags:
CFLAGS += ['-Os', '-Wall', '-W']


def options(opt):
        opt.load('compiler_c')
        opt.add_option('--runtests', action='store', default=False, help='Build & Run tests')


def define_config_h(conf):
    version_numbers = [int(v) for v in VERSION.split('.')]
    conf.define('MC_VERSION', VERSION)
    conf.define('MC_VERSION_MAJOR', version_numbers[0])
    conf.define('MC_VERSION_MINOR', version_numbers[1])
    conf.define('MC_VERSION_PATCH', version_numbers[2])

    return """
#ifdef MC_CONFIG_H
#define MC_CONFIG_H

{content}

/* Might come in useful */
#define MC_CHECK_VERSION(X,Y,Z) (0
    || X <= MC_VERSION_MAJOR
    || Y <= MC_VERSION_MINOR
    || Z <= MC_VERSION_MICRO)

#endif
""".format(content = conf.get_config_header())


def configure(conf):
    conf.start_msg(' => configuring the project in ')
    conf.end_msg(conf.path.abspath())

    conf.start_msg(' => install prefix is ')
    conf.end_msg(conf.options.prefix)

    conf.load('compiler_c')

    # Use pkg-config to find out l/cflags
    conf.check_cfg(
            package='glib-2.0',
            uselib_store='GLIB',
            args='--libs --cflags'
    )

    conf.check_cfg(
            package='sqlite3',
            uselib_store='SQLITE3',
            args='--libs --cflags'
    )

    conf.path.make_node('lib/config.h').write(define_config_h(conf))


def _find_libmoosecat_src(ctx):
    """
    Find all .c files in lib/ which
    are supposed to built libmoosecat.

    If files are in there that shouldn't be built,
    fill them in the exclude_files list.
    """
    c_files = ctx.path.ant_glob('lib/**/*.c')

    exclude_files = ['lib/main.c']
    for exclude in exclude_files:
        exclude_node = ctx.path.make_node(exclude)

        try:
           c_files.remove(exclude_node)
        except ValueError:
            print('- Excluded file not in list:', exclude)

    return c_files


def build(bld):
    LIBS = ['mpdclient'] + bld.env.LIB_GLIB + bld.env.LIB_SQLITE3
    INCLUDES = bld.env.INCLUDES_GLIB + bld.env.INCLUDES_SQLITE3

    bld.stlib(
            source = _find_libmoosecat_src(bld),
            target = 'moosecat',
            install_path = 'bin',
            includes = INCLUDES,
            lib = LIBS,
            cflags = CFLAGS
    )

    bld.program(
            source = 'lib/main.c',
            target = 'moosecat_runner',
            libdir = ['build'],
            includes = INCLUDES,
            lib = LIBS,
            use = 'moosecat',
            cflags = CFLAGS
    )

    if bld.options.runtests:
        for path in bld.path.ant_glob('test/lib/**/*.c'):
            bld.program(
                    features = 'test',
                    source = path.relpath(),
                    target = 'test_' + os.path.basename(path.abspath()),
                    includes = INCLUDES,
                    lib = LIBS,
                    install_path = 'bin',
                    use = 'moosecat',
                    cflags = CFLAGS
            )

        from waflib.Tools import waf_unit_test
        bld.add_post_fun(waf_unit_test.summary)
