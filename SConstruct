#!/usr/bin/env python
# encoding: utf-8

import os
import re
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


def DownloadSqlite(context, url):
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
    rev = subprocess.check_output('git log --pretty=format:"%h" -n 1', shell=True)
    context.Result(rev)
    conf.env['gitrev'] = rev
    return rev


def BuildConfigTemplate(target, source, env):
    with codecs.open(str(source[0]), 'r') as handle:
        text = handle.read()

    with codecs.open(str(target[0]), 'w') as handle:
        handle.write(text.format(
            HAVE_GLIB=conf.env['glib'],
            HAVE_GTK3=conf.env['gtk'],
            HAVE_ZLIB=conf.env['zlib'],
            HAVE_AVAHI_GLIB=conf.env['avahi_glib'],
            VERSION_MAJOR=0,
            VERSION_MINOR=0,
            VERSION_PATCH=1,
            VERSION_GIT_REVISION=env['gitrev']
        ))


def FindLibmoosecatSource():
    c_files = []
    c_files += Glob('lib/mpd/*.c')
    c_files += Glob('lib/mpd/pm/*.c')
    c_files += Glob('lib/misc/*.c')
    c_files += Glob('lib/store/*.c')
    c_files += Glob('lib/gtk/*.c')
    c_files += Glob('lib/store/libart/*.c')
    c_files += ['ext/sqlite/sqlite3.c']
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
    CPPPATH=['ext/sqlite/inc'],
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
    'DownloadSqlite': DownloadSqlite
})

if not conf.CheckCC():
    print('Error: Your compiler and/or environment is not correctly configured.')
    Exit(1)

SQLITE_AMALGAMATION = 'http://sqlite.org/snapshot/sqlite-amalgamation-201404281756.zip'
conf.DownloadSqlite(SQLITE_AMALGAMATION)
conf.CheckGitRev()
conf.CheckPKGConfig('0.15.0')

# Pkg-config to internal name
DEPS = {
    'glib-2.0 >= 2.40': 'glib',
    'gtk+-3.0 >= 3.1.2': 'gtk',
    'libmpdclient >= 2.9': 'libmpdclient',
    'avahi-glib >= 0.6.31': 'avahi_glib',
    'avahi-client': 'avahi_client',
    'zlib': 'zlib',
    'python3 >= 3.2': 'python3'
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

# Needed/Adviceable flags:
conf.env.Append(CCFLAGS=[
    '-std=c11', '-pipe', '-fPIC', '-g', '-O1'
])

# Optional flags:
conf.env.Append(CFLAGS=[
    '-Wall', '-W', '-Wextra',
    '-Wwrite-strings',
    '-Winit-self',
    # '-Wcast-align',
    '-Wcast-qual',
    '-Wpointer-arith',
    '-Wstrict-aliasing',
    '-Wformat=2',
    '-Wmissing-declarations',
    '-Wmissing-include-dirs',
    '-Wno-unused-parameter',
    '-Wuninitialized',
    '-Wold-style-definition',
    '-Wstrict-prototypes',
    #'-Wmissing-prototypes',
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

lib = env.StaticLibrary('moosecat', FindLibmoosecatSource())


for node in Glob('lib/samples/test-*.c'):
    name = str(node).split('/')[-1]
    if name.endswith('.c'):
        name = name[:-2]
    env.Program(
        name, [str(node)],
        LIBS=env['LIBS'] + [lib, 'm', 'dl'],
        LIBPATH='.'
    )


# Method that calls routines neccessary to create shared library.
def cythonPseudoBuilder(env, source):
    c_code = env.CythonBuilder(source)

    # Hack: Find out the python version:
    py_version = '34m'
    for entry in env['CPPPATH']:
        match = re.match('.*python(\d\.\d.*)', entry)
        if match:
            py_version = match.group(1).replace('.', '')
            break

    cython_lib = env.SharedLibrary(
        source, c_code,
        LIBS=env['LIBS'] + [lib, 'm', 'dl'],
        LIBPATH='.'
    )

    # This is currently Unix-specific.
    py_extension_file = '{name}.cpython-{version}.so'.format(
        name=os.path.basename(source),
        version=py_version
    )
    py_extension_file = os.path.join(os.path.dirname(source), py_extension_file)
    Command(
        target=py_extension_file,
        source=cython_lib[0],
        action=Move("$TARGET", "$SOURCE")
    )

env.AddMethod(cythonPseudoBuilder, 'Cython')
# env.Cython('moosecat/core/moose')
