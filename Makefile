RELEASENAME = brogue-1.7.2-svg2

MACHINE_NAME = $(shell uname -m)
SYSTEM_NAME = $(shell uname -s)

ifneq ($(filter %86,${MACHINE_NAME}),)
	# For x86, we will set the machine type to enable MMX
	# (It is enabled by default for x86_64, so we don't need to 
	# do anything special there.)
	PLATFORM_CFLAGS = -mpentium-mmx
else
	PLATFORM_CFLAGS = 
endif

BASE_CFLAGS = -O2 -g -Isrc/brogue -Isrc/platform 

ifeq (${SYSTEM_NAME},Darwin)
	FRAMEWORK_FLAGS = \
		-framework SDL \
		-framework SDL_ttf \
		-framework Cocoa
	SDL_CFLAGS = -DBROGUE_SDL -F osx-build
	SDL_LIB = -F osx-build ${FRAMEWORK_FLAGS} -Wl,-rpath,@executable_path/../Frameworks
	PKG_CONFIG = osx-build/gtk/bin/pkg-config
	OBJC_SOURCE = src/platform/SDLMain.m
else
	SDL_CFLAGS = -DBROGUE_SDL
	SDL_LIB = -lSDL -lSDL_ttf
	PKG_CONFIG = pkg-config
	OBJC_SOURCE = 
endif

RSVG_CFLAGS = $(shell ${PKG_CONFIG} --cflags librsvg-2.0 cairo)
RSVG_LIB = $(shell ${PKG_CONFIG} --libs librsvg-2.0 cairo)

CFLAGS = \
	${BASE_CFLAGS} ${PLATFORM_CFLAGS} \
	-Wall -Wno-parentheses \
	${SDL_CFLAGS} ${RSVG_CFLAGS}
LDFLAGS = ${SDL_LIB} ${RSVG_LIB}

TARFLAGS = --transform 's,^,${RELEASENAME}/,'
TARFILE = dist/${RELEASENAME}-source.tar.gz
ZIPFILE = dist/${RELEASENAME}-win32.zip
DMGFILE = dist/${RELEASENAME}-osx.dmg
PATCHFILE = dist/${RELEASENAME}.patch.gz

GLOBAL_HEADERS = \
	src/brogue/IncludeGlobals.h \
	src/brogue/Rogue.h

SOURCE = \
	src/brogue/Architect.c \
	src/brogue/Combat.c \
	src/brogue/Dijkstra.c \
	src/brogue/Globals.c \
	src/brogue/IO.c \
	src/brogue/Items.c \
	src/brogue/Light.c \
	src/brogue/Monsters.c \
	src/brogue/Buttons.c \
	src/brogue/Movement.c \
	src/brogue/Recordings.c \
	src/brogue/RogueMain.c \
	src/brogue/Random.c \
	src/brogue/MainMenu.c \
	src/brogue/Grid.c \
	\
	src/platform/main.c \
	src/platform/platformdependent.c \
	src/platform/sdl-platform.c \
        src/platform/sdl-keymap.c \
	src/platform/sdl-svgset.c 

DISTFILES = \
	readme \
	brogue \
	Makefile \
	Makefile.win32 \
	Makefile.osx \
	package-dylibs.py \
	$(wildcard *.sh) \
	$(wildcard *.rtf) \
	$(wildcard *.txt) \
	bin/keymap \
	bin/icon.bmp \
	bin/brogue-icon.png \
	bin/brogue-icon.ico \
	bin/Andale_Mono.ttf \
	$(wildcard src/brogue/*.[mch]) \
	$(wildcard src/platform/*.[mch]) \
	src/platform/brogue.rc \
	$(wildcard svg/*.svg) \
	$(wildcard res/*)

SRCSVG = $(wildcard svg/*.svg)
DSTSVG = $(SRCSVG:%=bin/%)

OBJS = $(SOURCE:%.c=%.${SYSTEM_NAME}.o) $(OBJC_SOURCE:%.m=%.${SYSTEM_NAME}.o)

.PHONY: all clean debdepends linux win32 osx dist distdir svgdir

ifeq (${SYSTEM_NAME},Darwin)
all: osx
else
all: linux
endif

include Makefile.win32
include Makefile.osx

linux: debdepends bin/brogue svgdir

debdepends:
	./checkdeps.sh librsvg2-dev libsdl-ttf2.0-dev libsdl1.2-dev

bin/brogue: ${OBJS}
	gcc -o bin/brogue ${OBJS} ${LDFLAGS}

svgdir: bin/svg $(DSTSVG)

bin/svg: 
	mkdir -p bin/svg

bin/svg/%.svg: svg/%.svg
	cp $< $@

%.${SYSTEM_NAME}.o: %.c ${GLOBAL_HEADERS}
	gcc ${CFLAGS} -c $< -o $@ 

%.${SYSTEM_NAME}.o: %.m ${GLOBAL_HEADERS}
	gcc ${CFLAGS} -c $< -o $@ 

clean: osxdist_ssh_clean
	rm -fr src/brogue/*.o src/platform/*.o bin/brogue bin/svg win32 Brogue.app osx osx-build

objclean:
	rm -fr src/brogue/*.o src/platform/*.o

dist: ${TARFILE} ${ZIPFILE} ${PATCHFILE} osxdist_ssh

distdir:
	mkdir -p dist

tar: ${TARFILE} ${PATCHFILE}

${TARFILE}: distdir all
	rm -f ${TARFILE}
	tar ${TARFLAGS} -czf ${TARFILE} ${DISTFILES}

${PATCHFILE}: distdir
	git diff upstream HEAD | gzip >${PATCHFILE}
