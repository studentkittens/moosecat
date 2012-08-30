#!/usr/bin/env python
# encoding: utf-8

# Global:

APPNAME = 'moosecat'
VERSION = '0.1'

# Targets:
def options(opt):
        opt.load('compiler_c')


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

    conf.check(
            fragment    = 'int main() { return 0; }\n',
            define_name = 'FOO',
            mandatory   = True)

    conf.write_config_header('lib/config.h')


def _find_libmoosecat_src(ctx):
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
    bld.shlib(
            source = _find_libmoosecat_src(bld),
            target = 'moosecat',
            includes = bld.env.INCLUDES_GLIB + bld.env.INCLUDES_SQLITE3,
            lib = ['mpdclient', ] + bld.env.LIB_GLIB + bld.env.LIB_SQLITE3,
            install_path = 'bin',
            cflags = ['-Os', '-Wall', '-W', '-std=c99']
    )

    bld.program(
            source = 'lib/main.c',
            target = 'moosecat_runner',
            includes = bld.env.INCLUDES_GLIB + bld.env.INCLUDES_SQLITE3,
            lib = ['mpdclient', ] + bld.env.LIB_GLIB + bld.env.LIB_SQLITE3,
            use = 'moosecat',
            cflags = ['-Os', '-Wall', '-std=c99']
    )


def libmoosecat_test(ctx):
    """
    1) Create test/.clar-out if it doesn't exist yet.
    2) Copy all test-files (*.c) recursively to test/.clar-out/
    3) Run clar over the files.
    4) Compile all .c files in test/.clar-out, link with libmoosecat
    """
    for path in ctx.path.ant_glob('test/lib/**/*.c'):
        print(path.abspath())
