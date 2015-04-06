#!/usr/bin/env python
# encoding: utf-8

import os
import sys
import shutil
import codecs
import subprocess

import urllib
import traceback
import zipfile
import glob

###########################################################################
#                            Utility Functions                            #
###########################################################################


def download_sqlite(context, url):
    try:
        context.Message('Downloading SQLite Amalgamation... ')
        if os.path.exists('ext/sqlite'):
            context.Result('already there. cool.')
        else:
            context.Result('processing.')
            print('  [DOWNLOADING]: {url}'.format(url=url))
            zipped_file = download(url)
            print('  [EXTRACTING]: {zipfl}'.format(zipfl=zipped_file))
            zipfile.ZipFile(zipped_file, 'r').extractall()
            extracted_files = glob.glob('sqlite3*.[ch]')
            print('  [COPYING]: {files}'.format(files=extracted_files))
            copy(extracted_files)
            print('  [CLEANING]')
            clean(zipped_file)
    except:
        print('Error during getting SQLite:')
        traceback.print_exc()
        print('Maybe no Internet connection?')
        context.Result(False)
        Exit(1)


def download(url):
    try:
        local, headers = urllib.urlretrieve(url, os.path.basename(url))
        return local
    except urllib.URLError as err:
        print('Error: Invalid URL? {error}'.format(error=err))


def copy(files):
    try:
        cpath = 'ext/sqlite'
        hpath = 'ext/sqlite/inc'

        try:
            os.makedirs('ext/sqlite/inc')
        except os.error:
            pass

        for f in files:
            if f.endswith('c'):
                shutil.copy(f, cpath)
            elif f.endswith('h'):
                shutil.copy(f, hpath)
    except shutil.Error as err:
        print('Error during copy process. {error}'.format(error=err))


def clean(zfile):
    try:
        os.remove(zfile)
        os.remove('shell.c')
    except shutil.Error as err:
        print('Error during clean up. {error}'.format(error=err))


def CheckPKGConfig(context, version):
    context.Message('Checking for pkg-config... ')
    command = 'pkg-config --atleast-pkgconfig-version=%s' % version
    ret = context.TryAction(command)[0]
    context.Result(ret)
    return ret


def CheckPKG(context, name, varname, required=True):
    context.Message('Checking for %s... ' % name)
    rc, text = context.TryAction('pkg-config --exists \'%s\'' % name)
    context.Result(rc)

    # Remember we have it:
    conf.env[varname] = True

    # 0 is defined as error by TryAction
    if rc is 0:
        print('Error: ' + name + ' not found.')
        if required:
            Exit(1)
    return rc, text


def CheckGitRev(context):
    context.Message('Checking for git revision... ')
    rev = subprocess.check_output(
        'git log --pretty=format:"%h" -n 1', shell=True
    )
    context.Result(rev)
    conf.env['gitrev'] = rev
    return rev


def BuildConfigTemplate(target, source, env):
    with codecs.open(str(source[0]), 'r') as handle:
        text = handle.read()

    with codecs.open(str(target[0]), 'w') as handle:
        handle.write(text.format(
            HAVE_GLIB=conf.env['glib'],
            HAVE_GTK3=0,
            # HAVE_GTK3=conf.env['gtk'],
            HAVE_ZLIB=conf.env['zlib'],
            HAVE_AVAHI_GLIB=conf.env['avahi_glib'],
            VERSION_MAJOR=0,
            VERSION_MINOR=0,
            VERSION_PATCH=1,
            VERSION_GIT_REVISION=env['gitrev']
        ))


def FindLibmoosecatSource(suffix='.c', extensions=True):
    c_files = []
    c_files += Glob('lib/mpd/*' + suffix)
    c_files += Glob('lib/mpd/pm/*' + suffix)
    c_files += Glob('lib/misc/*' + suffix)
    c_files += Glob('lib/store/moose-store' + suffix)
    c_files += Glob('lib/store/moose-store-playlist' + suffix)
    c_files += Glob('lib/store/moose-store-completion' + suffix)
    c_files += Glob('lib/store/moose-store-query-parser' + suffix)
    c_files += Glob('lib/gtk/*' + suffix)
    c_files += Glob('lib/*' + suffix)
    if extensions:
        c_files += ['ext/sqlite/sqlite3.c', 'ext/libart/art.c']

    return c_files

###########################################################################
#                                 Colors!                                 #
###########################################################################


colors = {
    'cyan': '\033[96m',
    'purple': '\033[95m',
    'blue': '\033[94m',
    'green': '\033[92m',
    'yellow': '\033[93m',
    'red': '\033[91m',
    'end': '\033[0m'
}

# If the output is not a terminal, remove the colors
if not sys.stdout.isatty():
    for key, value in colors.iteritems():
        colors[key] = ''

compile_source_message = '%sCompiling %s==> %s$SOURCE%s' % \
    (colors['blue'], colors['purple'], colors['yellow'], colors['end'])

compile_shared_source_message = '%sCompiling shared %s==> %s$SOURCE%s' % \
    (colors['blue'], colors['purple'], colors['yellow'], colors['end'])

link_program_message = '%sLinking Program %s==> %s$TARGET%s' % \
    (colors['red'], colors['purple'], colors['yellow'], colors['end'])

link_library_message = '%sLinking Static Library %s==> %s$TARGET%s' % \
    (colors['red'], colors['purple'], colors['yellow'], colors['end'])

ranlib_library_message = '%sRanlib Library %s==> %s$TARGET%s' % \
    (colors['red'], colors['purple'], colors['yellow'], colors['end'])

link_shared_library_message = '%sLinking Shared Library %s==> %s$TARGET%s' % \
    (colors['red'], colors['purple'], colors['yellow'], colors['end'])


# General Environment
options = dict(
    CXXCOMSTR=compile_source_message,
    CCCOMSTR=compile_source_message,
    SHCCCOMSTR=compile_shared_source_message,
    SHCXXCOMSTR=compile_shared_source_message,
    ARCOMSTR=link_library_message,
    RANLIBCOMSTR=ranlib_library_message,
    SHLINKCOMSTR=link_shared_library_message,
    LINKCOMSTR=link_program_message,
    CPPPATH=['ext/sqlite/inc', 'ext/libart/'],
    BUILDERS={'CythonBuilder': Builder(
        action='cython $SOURCE',
        suffix='.c',
        src_suffix='.pyx'
    )},
    ENV={
        'PATH': os.environ['PATH'],
        'TERM': os.environ['TERM'],
        'HOME': os.environ['HOME']
    }
)

if ARGUMENTS.get('VERBOSE') == "1":
    del options['CCCOMSTR']
    del options['LINKCOMSTR']

env = Environment(**options)


###########################################################################
#                              Actual Script                              #
###########################################################################

# Configuration:
conf = Configure(env, custom_tests={
    'CheckPKGConfig': CheckPKGConfig,
    'CheckPKG': CheckPKG,
    'CheckGitRev': CheckGitRev,
    'DownloadSqlite': download_sqlite
})

if not conf.CheckCC():
    print('Error: Your compiler and/or environment is not correctly configured.')
    Exit(1)

SQLITE_AMALGAMATION = 'https://www.sqlite.org/snapshot/sqlite-amalgamation-201504011321.zip'

conf.DownloadSqlite(SQLITE_AMALGAMATION)
conf.CheckGitRev()
conf.CheckPKGConfig('0.15.0')

# Pkg-config to internal name
DEPS = {
    'glib-2.0 >= 2.32': 'glib',
    'gobject-2.0': 'gobject',
    'gmodule-2.0': 'gmodule',
    'gobject-introspection-1.0': 'gobject_introspection',
    'gio-2.0': 'gio',
    'gtk+-3.0': 'gtk',
    'libmpdclient >= 2.3': 'libmpdclient',
    'avahi-glib >= 0.6.30': 'avahi_glib',
    'avahi-client': 'avahi_client',
    'zlib': 'zlib'
    # 'python3 >= 3.2': 'python3'
}

for pkg, name in DEPS.items():
    conf.CheckPKG(pkg, name)

packages = []
for pkg in DEPS.keys():
    packages .append(pkg.split()[0])


if 'CC' in os.environ:
    conf.env.Replace(CC=os.environ['CC'])
    print(">> Using compiler: " + os.environ['CC'])


if 'CFLAGS' in os.environ:
    conf.env.Append(CCFLAGS=os.environ['CFLAGS'])
    print(">> Appending custom build flags : " + os.environ['CFLAGS'])


if 'LDFLAGS' in os.environ:
    conf.env.Append(LINKFLAGS=os.environ['LDFLAGS'])
    print(">> Appending custom link flags : " + os.environ['LDFLAGS'])

# conf.env.Append(LINKFLAGS=['-ld'])
# Needed/Adviceable flags:
conf.env.Append(CCFLAGS=[
    '-std=c11', '-pipe', '-fPIC', '-g'
])

# Optional flags:
conf.env.Append(CFLAGS=[
    '-Wall', '-W', '-Wextra',
    '-Winit-self',
    '-Wstrict-aliasing',
    '-Wmissing-include-dirs',
    '-Wuninitialized',
    '-Wstrict-prototypes',
])

if conf.env['CC'] == 'gcc':
    # GCC-Specific Options.
    conf.env.Append(CCFLAGS=['-lto'])

# Sqlite Flags (needed):
conf.env.Append(CFLAGS=[
    '-DSQLITE_ALLOW_COVERING_INDEX_SCAN=1',
    '-DSQLITE_ENABLE_FTS3',
    '-DSQLITE_ENABLE_FTS3_PARENTHESIS'
])

env.ParseConfig('pkg-config --cflags --libs ' + ' '.join(packages))

# Your extra checks here
env = conf.Finish()

env.AlwaysBuild(
    env.Command(
        'lib/moose-config.h', 'lib/moose-config.h.in', BuildConfigTemplate
    )
)

lib = env.SharedLibrary(
    'moosecat',
    FindLibmoosecatSource(),
    LIBS=env['LIBS'] + ['m', 'dl']
)

# Sample programs:
for node in Glob('lib/samples/test-*.c'):
    name = str(node).split('/')[-1]
    if name.endswith('.c'):
        name = name[:-2]
    env.Program(
        name, [str(node)],
        LIBS=env['LIBS'] + [lib, 'm', 'dl'],
        LIBPATH='.'
    )


def BuildTest(target, source, env):
    subprocess.call([
        '/usr/bin/gtester', '--verbose',
        '-o', str(target[0]),
        str(source[0])
    ])


MOOSE_GIR_NAME = 'Moose'
MOOSE_GIR_VERSION = '0.1'
MOOSE_GIR = 'Moose' + '-' + MOOSE_GIR_VERSION


def BuildGir(target, source, env):
    files = [str(node) for node in source]
    subprocess.call([
            'g-ir-scanner',
            '--include=GObject-2.0',
            '--namespace=' + MOOSE_GIR_NAME,
            '--nsversion=' + MOOSE_GIR_VERSION,
            '--output=' + MOOSE_GIR + '.gir',
            '--warn-all',
            '-L.', '--library=moosecat',
        ] + files,
        env=dict(os.environ.items(), LD_LIBRARY_PATH='.')
    )
    subprocess.call([
        'g-ir-compiler',
        'Moose-0.1.gir',
        '--output',
        MOOSE_GIR + '.typelib'
    ])

TEST_COMMANDS, TEST_PROGRAMS = [], []

# test_alias = Alias('test', [program], program[0].abspath)
for node in Glob('lib/tests/test-*.c'):
    name = str(node).split('/')[-1]
    if name.endswith('.c'):
        name = name[:-2]
    prog = env.Program(
        name, [str(node)],
        LIBS=env['LIBS'] + [lib, 'm', 'dl'],
        LIBPATH='.'
    )
    TEST_PROGRAMS.append(prog)

    if 'test' in COMMAND_LINE_TARGETS:
        command = env.Command(name + '.log', name, BuildTest)
        env.Depends(command, prog)
        TEST_COMMANDS.append(command)

if 'build-test' in COMMAND_LINE_TARGETS:
    env.AlwaysBuild(env.Alias('build-test', [TEST_PROGRAMS]))

if 'test' in COMMAND_LINE_TARGETS:
    env.AlwaysBuild(env.Alias('test', [TEST_COMMANDS]))

if 'gir' in COMMAND_LINE_TARGETS:
    sources = FindLibmoosecatSource('.h', False)
    command = env.Command(MOOSE_GIR + '.typelib', sources, BuildGir)
    env.AlwaysBuild(env.Alias('gir', [command]))
