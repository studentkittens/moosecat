
#ifdef MC_CONFIG_H
#define MC_CONFIG_H

#define PYTHONDIR "/usr/local/lib/python3.3/site-packages"
#define PYTHONARCHDIR "/usr/local/lib/python3.3/site-packages"
#define HAVE_PYTHON_H 1
#define HAVE_PYEMBED 1
#define HAVE_PYEXT 1
#define HAVE_GLIB 1
#define HAVE_GTK3 1
#define HAVE_ZLIB 1
#define HAVE_AVAHI_GLIB 1
#define MC_VERSION "0.0.1"
#define MC_VERSION_MAJOR 0
#define MC_VERSION_MINOR 0
#define MC_VERSION_PATCH 1
#define MC_VERSION_GIT_REVISION "39f30b7"


/* Might come in useful */
#define MC_CHECK_VERSION(X,Y,Z) (0  \
    || X <= MC_VERSION_MAJOR        \
    || Y <= MC_VERSION_MINOR        \
    || Z <= MC_VERSION_MICRO)       \

#endif
