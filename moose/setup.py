from distutils.core import setup
from distutils.extension import Extension
from Cython.Distutils import build_ext

import subprocess

try:
    with open('README.txt') as file:
        long_description = file.read()
except IOError:
    long_description = '(N/A) [No README]'

def pkgconfig(*packages, **kw):
    flag_map = {'-I': 'include_dirs', '-L': 'library_dirs', '-l': 'libraries'}
    for token in subprocess.getoutput("pkg-config --libs --cflags %s" % ' '.join(packages)).split():
        kw.setdefault(flag_map.get(token[:2]), []).append(token[2:])

    return kw


moose_conf = {
    'libraries': ['mpdclient', 'moosecat'],
    'library_dirs': ['../build'],
    'include_dirs': []
}

moose_conf = pkgconfig('glib-2.0', **moose_conf)

setup(
    cmdclass = {'build_ext': build_ext},
    ext_modules = [Extension("moose", ["status.pyx"], **moose_conf)],
    name='moose',
    version='0.0.1',
    author='Christopher Pahl',
    author_email='sahib@online.de',
    url='http://sahib.github.com/studentkittens/moosecat',
    long_description=long_description
)
