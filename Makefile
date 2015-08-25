#
# Makefile
# Adrian Perez, 2015-08-25 11:58
#

CFLAGS ?= -Os -Wall

PYTHON        ?= python3
PKG_MODULES   := pygobject-3.0 webkit2gtk-web-extension-4.0 ${PYTHON}
WEB_EXT_FLAGS := $(shell pkg-config ${PKG_MODULES} --cflags)
WEB_EXT_LIBS  := $(shell pkg-config ${PKG_MODULES} --libs)

CPPFLAGS += ${WEB_EXT_FLAGS}
LDLIBS   += ${WEB_EXT_LIBS}

all: pythonloader.so

pythonloader.so: pythonloader.o
	${LD} ${LDFLAGS} -fPIC -shared -o $@ $^ ${LDLIBS}
pythonloader.so: CFLAGS += -fPIC

clean:
	${RM} pythonloader.o pythonloader.so

# vim:ft=make
#
