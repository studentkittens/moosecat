#!/bin/sh

# Source code formatter.
# See:
# http://astyle.sourceforge.net/astyle.html

astyle --style=kr \
    --pad-header \
    --indent-col1-comments  \
    --unpad-paren\
    --break-blocks \
    --align-pointer=name \
    --mode=c  \
    --formatted \
    $(find lib -iname '*.[ch]')
