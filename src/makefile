# luasocket src/makefile
#
# Definitions in this section can be overriden on the command line or in the
# environment.
#
# These are equivalent:
#
#    export PLAT=linux DEBUG=DEBUG LUAV=5.2 prefix=/sw
#    make
#
# and
#
#   make PLAT=linux DEBUG=DEBUG LUAV=5.2 prefix=/sw

# PLAT: linux macosx win32
# platform to build for
PLAT?=linux

# LUAV: 5.1 5.2
# lua version to build against
LUAV?=5.1

# DEBUG: NODEBUG DEBUG
# debug mode causes luasocket to collect and returns timing information useful
# for testing and debugging luasocket itself
DEBUG?=NODEBUG

# prefix: /usr/local /usr /opt/local /sw
# the top of the default install tree
prefix?=/usr/local

# where lua headers are found for macosx builds
# LUAINC_macosx: /opt/local/include
LUAINC_macosx?=/opt/local/include
# FIXME default should be where fink puts lua

# LUAINC_linux: /usr/include/lua$(LUAV) /usr/local/include /usr/local/include/lua$(LUAV)
# where lua headers are found for linux builds
LUAINC_linux?=/usr/include/lua$(LUAV)

# LUAINC_win32:
# LUALIB_win32:
# where lua headers and libraries are found for win32 builds
LUAINC_win32?="../../lua-5.1.3/src"
LUALIB_win32?="../../lua-5.1.3"
# FIXME default should be where lua-for-windows puts lua

# DESTDIR: (no default)
# used by package managers to install into a temporary destination
DESTDIR=

#------
# Definitions below can be overridden on the make command line, but
# shouldn't have to be.

print:
	@echo PLAT=$(PLAT)
	@echo LUAV=$(LUAV)
	@echo DEBUG=$(DEBUG)
	@echo prefix=$(prefix)
	@echo LUAINC_$(PLAT)=$(LUAINC_$(PLAT))
	@echo LUALIB_$(PLAT)=$(LUALIB_$(PLAT))

#------
# Install directories
#

INSTALL_DATA=cp
INSTALL_EXEC=cp
INSTALL_TOP=$(DESTDIR)$(prefix)

INSTALL_TOP_SHARE=$(INSTALL_TOP)/share/lua/$(LUAV)
INSTALL_TOP_LIB=$(INSTALL_TOP)/lib/lua/$(LUAV)

INSTALL_SOCKET_SHARE=$(INSTALL_TOP_SHARE)/socket
INSTALL_SOCKET_LIB=$(INSTALL_TOP_LIB)/socket
INSTALL_MIME_SHARE=$(INSTALL_TOP_SHARE)/mime
INSTALL_MIME_LIB=$(INSTALL_TOP_LIB)/mime

#------
# Supported platforms
#
PLATS= macosx linux win32

#------
# Compiler and linker settings
# for Mac OS X
SO_macosx=so
O_macosx=o
CC_macosx=gcc
DEF_macosx= -DLUASOCKET_$(DEBUG) -DUNIX_HAS_SUN_LEN -DLUA_COMPAT_MODULE \
	-DLUASOCKET_API='__attribute__((visibility("default")))' \
	-DMIME_API='__attribute__((visibility("default")))'
CFLAGS_macosx= -I$(LUAINC) $(DEF) -pedantic -Wall -O2 -fno-common \
	-fvisibility=hidden
LDFLAGS_macosx= -bundle -undefined dynamic_lookup -o 
LD_macosx= export MACOSX_DEPLOYMENT_TARGET="10.3"; gcc
SOCKET_macosx=usocket.o

#------
# Compiler and linker settings
# for Linux
SO_linux=so
O_linux=o
CC_linux=gcc
DEF_linux=-DLUASOCKET_$(DEBUG) \
	-DLUASOCKET_API='__attribute__((visibility("default")))' \
	-DMIME_API='__attribute__((visibility("default")))'
CFLAGS_linux= -I$(LUAINC) $(DEF) -pedantic -Wall -Wshadow -Wextra -Wimplicit -O2 -ggdb3 -fpic \
	-fvisibility=hidden
LDFLAGS_linux=-O -shared -fpic -o 
LD_linux=gcc
SOCKET_linux=usocket.o

#------
# Compiler and linker settings
# for Win32
SO_win32=dll
O_win32=obj
CC_win32=cl
DEF_win32= /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /D "_USRDLL" \
     /D "LUASOCKET_API=__declspec(dllexport)" /D "LUASOCKET_$(DEBUG)" \
     /D "_CRT_SECURE_NO_WARNINGS" /D "_WINDLL"
CFLAGS_win32=/I$(LUAINC) $(DEF) /O2 /Ot /MD /W3 /nologo
LDFLAGS_win32= /nologo /link /NOLOGO /DLL /INCREMENTAL:NO \
    /LIBPATH:$(LUALIB) \
    /MANIFEST \
    /MANIFESTFILE:"intermediate.manifest" \
    /MANIFESTUAC:"level='asInvoker' uiAccess='false'" \
	/SUBSYSTEM:WINDOWS /OPT:REF /OPT:ICF /DYNAMICBASE:NO \
    /MACHINE:X86 ws2_32.lib lua$(LUAV).lib /OUT:
LD_win32=cl
SOCKET_win32=wsocket.obj

.SUFFIXES: .obj

.c.obj:
	$(CC) $(CFLAGS) /Fo"$@" /c $<

#------
# Output file names
#
SO=$(SO_$(PLAT))
O=$(O_$(PLAT))
SOCKET_V=2.1
MIME_V=1.0.3
SOCKET_SO=socket.$(SO).$(SOCKET_V)
MIME_SO=mime.$(SO).$(MIME_V)
SOCKET=$(SOCKET_$(PLAT))

#------
# Settings selected for platform
#
CC=$(CC_$(PLAT))
DEF=$(DEF_$(PLAT))
CFLAGS=$(CFLAGS_$(PLAT))
LDFLAGS=$(LDFLAGS_$(PLAT))
LD=$(LD_$(PLAT))
LUAINC= $(LUAINC_$(PLAT))
LUALIB= $(LUALIB_$(PLAT))

#------
# Modules belonging to socket-core
#
SOCKET_OBJS= \
	luasocket.$(O) \
	timeout.$(O) \
	buffer.$(O) \
	io.$(O) \
	auxiliar.$(O) \
	options.$(O) \
	inet.$(O) \
	$(SOCKET) \
	except.$(O) \
	select.$(O) \
	tcp.$(O) \
	udp.$(O)

ifneq ($(PLAT),win32)
	SOCKET_OBJS += unix.$(O) serial.$(O)
endif

#------
# Modules belonging mime-core
#
MIME_OBJS= \
	mime.$(O)

#------
# Files to install
#
TO_SOCKET_SHARE= \
	http.lua \
	url.lua \
	tp.lua \
	ftp.lua \
	headers.lua \
	smtp.lua

TO_TOP_SHARE= \
	ltn12.lua \
	socket.lua \
	mime.lua

#------
# Targets
#
default: $(PLAT)

macosx:
	$(MAKE) all PLAT=macosx

win32:
	$(MAKE) all PLAT=win32

linux:
	$(MAKE) all PLAT=linux

none:
	@echo "Please run"
	@echo "   make PLATFORM"
	@echo "where PLATFORM is one of these:"
	@echo "   $(PLATS)"

all: $(SOCKET_SO) $(MIME_SO)

$(SOCKET_SO): $(SOCKET_OBJS)
	$(LD) $(SOCKET_OBJS) $(LDFLAGS)$@ 

$(MIME_SO): $(MIME_OBJS)
	$(LD) $(MIME_OBJS) $(LDFLAGS)$@ 

install: 
	mkdir -p $(INSTALL_TOP_SHARE)
	$(INSTALL_DATA) $(TO_TOP_SHARE) $(INSTALL_TOP_SHARE)
	mkdir -p $(INSTALL_SOCKET_SHARE)
	$(INSTALL_DATA) $(TO_SOCKET_SHARE) $(INSTALL_SOCKET_SHARE)
	mkdir -p $(INSTALL_SOCKET_LIB)
	$(INSTALL_EXEC) $(SOCKET_SO) $(INSTALL_SOCKET_LIB)/core.$(SO)
	mkdir -p $(INSTALL_MIME_LIB)
	$(INSTALL_EXEC) $(MIME_SO) $(INSTALL_MIME_LIB)/core.$(SO)

local:
	$(MAKE) install INSTALL_TOP_LIB=.. INSTALL_TOP_SHARE=..

clean:
	rm -f $(SOCKET_SO) $(SOCKET_OBJS)
	rm -f $(MIME_SO) $(MIME_OBJS)

.PHONY: all $(PLATS) default clean echo none

#------
# List of dependencies
#
auxiliar.$(O): auxiliar.c auxiliar.h
buffer.$(O): buffer.c buffer.h io.h timeout.h
except.$(O): except.c except.h
inet.$(O): inet.c inet.h socket.h io.h timeout.h usocket.h
io.$(O): io.c io.h timeout.h
luasocket.$(O): luasocket.c luasocket.h auxiliar.h except.h \
	timeout.h buffer.h io.h inet.h socket.h usocket.h tcp.h \
	udp.h select.h unix.h serial.h
mime.$(O): mime.c mime.h
options.$(O): options.c auxiliar.h options.h socket.h io.h \
	timeout.h usocket.h inet.h
select.$(O): select.c socket.h io.h timeout.h usocket.h select.h
serial.$(O): serial.c auxiliar.h socket.h io.h timeout.h usocket.h \
  options.h unix.h buffer.h
tcp.$(O): tcp.c auxiliar.h socket.h io.h timeout.h usocket.h \
	inet.h options.h tcp.h buffer.h
timeout.$(O): timeout.c auxiliar.h timeout.h
udp.$(O): udp.c auxiliar.h socket.h io.h timeout.h usocket.h \
	inet.h options.h udp.h
unix.$(O): unix.c auxiliar.h socket.h io.h timeout.h usocket.h \
	options.h unix.h buffer.h
usocket.$(O): usocket.c socket.h io.h timeout.h usocket.h
wsocket.$(O): wsocket.c socket.h io.h timeout.h usocket.h
