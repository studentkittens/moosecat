from setuptools import setup, find_packages
from setuptools.extension import Extension
from Cython.Distutils import build_ext

import subprocess
import os
import fnmatch


#def generate_config_h():


def pkgconfig(*packages, **kw):
    flag_map = {'-I': 'include_dirs', '-L': 'library_dirs', '-l': 'libraries'}
    for token in subprocess.getoutput("pkg-config --libs --cflags %s" % ' '.join(packages)).split():
        kw.setdefault(flag_map.get(token[:2]), []).append(token[2:])
    return kw


def recursive_glob(topdir='.', pattern='*', excludes=[]):
    finds = []
    for root, dirs, files in os.walk(topdir):
        for path in files:
            if not path in excludes:
                if fnmatch.fnmatch(path, pattern):
                    finds.append(os.path.join(root, path))
    return finds


def merge_options(d1, d2):
    result = dict(d2)
    for key, value in d1.items():
        options = result.setdefault(key, [])
        options += value

    return result

ext_compiler_flags = {
    'extra_compile_args': ['-std=c99']
}


ext_modules = [
    Extension(
        "moosecat.core",
        ["moosecat/core/moose.pyx"],
        **pkgconfig('glib-2.0')
    ),
    Extension(
        "libmoosecat",
        recursive_glob(
            topdir='lib/',
            pattern='*.c',
            excludes=[
                'db_test.c',
                'gtk_db.c',
                'event_test.c',
                'test_outputs.c'
            ]),
        **merge_options(
            pkgconfig('glib-2.0'),
            ext_compiler_flags
        )
    )
]


#generate_config_h()

setup(
    name="moosecat",
    version="0.1",
    cmdclass = {'build_ext': build_ext},
    ext_modules=ext_modules,
    packages=find_packages(exclude=['moosecat.test.*'])
)
