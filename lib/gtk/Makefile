CC=gcc
C_INCLUDES=`pkg-config --cflags gtk+-3.0`
CFLAGS=$(C_INCLUDES) -g -W -std=c99
LIBS=`pkg-config --libs gtk+-3.0`
LIBDIR=/usr/lib

OBJECTS=moose-list-model.lo
SOURCES=moose-list-model.c moose-list-model.h 

NAMESPACE=Moose
NSVERSION=0.1
GIR_FILE=$(NAMESPACE)-$(NSVERSION).gir
TYPELIB_FILE=$(NAMESPACE)-$(NSVERSION).typelib

all: libmooselistmodel.la $(TYPELIB_FILE)

libmooselistmodel.la: $(OBJECTS)	
	libtool link $(CC) $(LIBS) -rpath $(LIBDIR) $(OBJECTS) -o $@ 

$(TYPELIB_FILE): $(GIR_FILE)
	g-ir-compiler $(GIR_FILE) --output=$(TYPELIB_FILE)

$(GIR_FILE): moose-list-model.c moose-list-model.h
	libtool exec g-ir-scanner $^ --library=mooselistmodel $(C_INCLUDES)  \
	--include=GObject-2.0 --namespace=$(NAMESPACE) \
	--nsversion=$(NSVERSION) --output=$@ --warn-all

moose-list-model.lo: moose-list-model.c moose-list-model.h
	libtool compile $(CC) $(CFLAGS) -c $< -o $@

clean:
	-rm *.lo *.la $(TYPELIB_FILE) $(GIR_FILE)
	-rm -rf .libs


