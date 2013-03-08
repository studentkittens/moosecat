#!/usr/bin/env python
#encoding: utf-8
import os
import subprocess

from waflib import Logs
from waflib import Task
from waflib.TaskGen import feature, after_method

# Global:

APPNAME = 'moosecat'
VERSION = '0.0.1'

# Needed/Adviceable flags:
CFLAGS = ['-std=c99', '-pipe', '-fPIC', '-O']

# Optional flags:
CFLAGS += ['-ggdb3', '-Wall', '-W', '-Wno-unused-parameter']

# These files are not built into libmoosecat.so
EXCLUDE_FILES = []

# Use SPLINT to statically check libmoosecat? (this will fail currently)
USE_SPLINT = False

def options(opt):
        opt.load('compiler_c')
        opt.load('python')
        opt.load('cython')
        #opt.load('cython_cache', tooldir='.')

        opt.add_option('--cython-dev', action='store', default=True, help='Compile all Cython Files')
        opt.add_option('--runtests', action='store', default=False, help='Build & Run tests')


def define_config_h(conf):

    # non-optional dependencies
    conf.define(conf.have_define('GLIB'), 1)
    conf.define(conf.have_define('SQLITE3'), 1)

    version_numbers = [int(v) for v in VERSION.split('.')]
    conf.define('MC_VERSION', VERSION)
    conf.define('MC_VERSION_MAJOR', version_numbers[0])
    conf.define('MC_VERSION_MINOR', version_numbers[1])
    conf.define('MC_VERSION_PATCH', version_numbers[2])

    if not conf.env.GIT == []:
        current_rev = subprocess.check_output([conf.env.GIT, 'log', '--pretty=format:"%h"', '-n 1'])
        conf.define('MC_VERSION_GIT_REVISION', str(current_rev, 'UTF-8')[1:-1])
    else:
        conf.define('MC_VERSION_GIT_REVISION', '[unknown]')

    # Template it!
    return """
#ifdef MC_CONFIG_H
#define MC_CONFIG_H

{content}


/* Might come in useful */
#define MC_CHECK_VERSION(X,Y,Z) (0  \\
    || X <= MC_VERSION_MAJOR        \\
    || Y <= MC_VERSION_MINOR        \\
    || Z <= MC_VERSION_MICRO)       \\

#endif
""".format(content=conf.get_config_header())


def configure(conf):
    conf.start_msg(' => configuring the project in ')
    conf.end_msg(conf.path.abspath())


    conf.start_msg(' => install prefix is ')
    conf.end_msg(conf.options.prefix)

    conf.load('compiler_c')
    conf.load('python')
    conf.check_python_headers()
    try:
        conf.load('cython')
    except conf.errors.ConfigurationError:
        Logs.warn('Cython was not found, using the cache')

    # Try to find git
    conf.find_program('git', mandatory=False)

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

    conf.check_cfg(
            package='gtk+-2.0',
            uselib_store='GTK3',
            args='--libs --cflags'
    )

    conf.check_cfg(
            package='zlib',
            uselib_store='ZLIB',
            args='--libs --cflags'
    )

    # Find the static C source checker
    conf.find_program('splint', var='SPLINT', mandatory=False)

    # Create lib/config.h. Includes basic Version/Feature Info.
    conf.path.make_node('lib/config.h').write(define_config_h(conf))


def _find_libmoosecat_src(ctx):
    """
    Find all .c files in lib/ which
    are supposed to built libmoosecat.

    If files are in there that shouldn't be built,
    fill them in the exclude_files list.
    """
    c_files = ctx.path.ant_glob('lib/**/*.c')

    for exclude in EXCLUDE_FILES:
        exclude_node = ctx.path.make_node(exclude)

        try:
            c_files.remove(exclude_node)
        except ValueError:
            pass

    return c_files


def _find_cython_src(ctx):
    '''
    Find all .pyx files in moosecat/core,
    and return them.
    '''
    #if ctx.options.cython_dev:
    #    return ctx.path.ant_glob('moosecat/core/*.pyx')
    #else:
    return 'moosecat/core/moose.pyx'


def build(bld):
    LIBS = ['mpdclient'] + ['curses'] + bld.env.LIB_GLIB + bld.env.LIB_SQLITE3 + bld.env.LIB_ZLIB
    INCLUDES = bld.env.INCLUDES_GLIB + bld.env.INCLUDES_SQLITE3 + bld.env.INCLUDES_ZLIB

    def build_test_program(sources, target_name, libraries=LIBS, includes_h=INCLUDES):
        bld.program(
                source=sources,
                target=target_name,
                libdir=['build'],
                includes=includes_h,
                lib=libraries,
                use='moosecat',
                cflags=CFLAGS
        )

        EXCLUDE_FILES.append(sources)

    build_test_program('lib/main.c', 'moosecat_runner')
    build_test_program('lib/test_idle.c', 'test_idle')
    build_test_program('lib/test_command_list.c', 'test_command_list')
    build_test_program('lib/time_playlist.c', 'time_playlist')
    build_test_program('lib/db_test.c', 'db_test')
    build_test_program('lib/test_outputs.c', 'test_outputs')
    build_test_program('lib/event_test.c', 'event_test')
    build_test_program('lib/gtk_db.c', 'gtk_db',
            libraries=LIBS + bld.env.LIB_GTK3,
            includes_h=INCLUDES + bld.env.INCLUDES_GTK3
    )

    bld.stlib(
            source=_find_libmoosecat_src(bld),
            target='moosecat',
            install_path='bin',
            includes=INCLUDES,
            lib=LIBS,
            cflags=CFLAGS
    )

    # Compile the cython code into build/moose.something.so
    bld(
        features='c cshlib pyext',
        source=_find_cython_src(bld),
        #source=['moosecat/core/moose.pyx', 'moosecat/core/client.pyx'],
        target='moosecat/core/moose',
        use='moosecat',
        lib=LIBS,
        includes=['moosecat'] + INCLUDES,
        cflags=CFLAGS
    )

    if bld.options.runtests:
        for path in bld.path.ant_glob('test/lib/**/*.c'):
            bld.program(
                    features='test',
                    source=path.relpath(),
                    target='test_' + os.path.basename(path.abspath()),
                    includes=INCLUDES,
                    lib=LIBS,
                    install_path='bin',
                    use='moosecat',
                    cflags=CFLAGS
            )

        from waflib.Tools import waf_unit_test
        bld.add_post_fun(waf_unit_test.summary)


# I think this should be removed.

@feature('c')
@after_method('process_source')
def add_files_to_lint(self):
    if not USE_SPLINT:
        return

    for x in self.compiled_tasks:
        c_file = repr(x.inputs[0])
        # Only check files in lib/
        if 'lib' in c_file.split(os.path.sep):
            self.create_task('Lint', src=x.inputs[0])



class Lint(Task.Task):
    run_str = '${SPLINT} +charindex -nullpass ${CPPPATH_ST:INCPATHS} ${SRC[0].abspath()}'
    ext_in = ['.h']  # guess why this line..
    before = ['c']

    def __init__(self, *args, **kwargs):
        Task.Task.__init__(self, *args, **kwargs)
