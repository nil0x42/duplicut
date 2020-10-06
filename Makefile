TARGET       = duplicut
SHELL        = /bin/sh

# debug level
level        = 1
 
CFLAGS       = -Iinclude -Wall -Wextra \
			   -Wdisabled-optimization -Winline \
			   -Wdouble-promotion -Wunknown-pragmas \
			   -Wno-implicit-fallthrough \
			   -Wno-error=implicit-function-declaration \
			   -mtune=native -ffast-math
LDFLAGS      = -lm -pthread
RELEASEFLAGS = -O2 -D NDEBUG
DEBUGFLAGS   = -O0 -D DEBUG=$(level) -std=gnu99 -g3
 
SOURCES      = main.c thpool.c file.c chunk.c line.c dedupe.c \
			   optparse.c config.c error.c memstate.c meminfo.c bytesize.c \
			   hmap.c status.c uinput.c \

COMMON       = include/const.h include/debug.h
OBJECTS      = $(patsubst %.c, objects/%.o, $(SOURCES))
 
PREFIX       = $(DESTDIR)/usr/local
BINDIR       = $(PREFIX)/bin
 
 
all: $(TARGET)
release: $(TARGET)
re: $(TARGET)
 
debug: CFLAGS += $(DEBUGFLAGS)
debug: distclean $(OBJECTS) $(COMMON)
	-ctags src/* include/*
	$(CC) $(FLAGS) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

$(TARGET): CFLAGS += $(RELEASEFLAGS)
$(TARGET): distclean $(OBJECTS) $(COMMON)
	$(CC) $(FLAGS) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)
	strip --strip-all $(TARGET) || strip -Sx $(TARGET) # osx fallback

profile: CFLAGS += -pg
profile: distclean $(TARGET)

profile-generate: CFLAGS += $(RELEASEFLAGS) -fprofile-generate -fprofile-arcs
profile-generate: distclean $(OBJECTS) $(COMMON)
	$(CC) $(FLAGS) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

rm-objects:
	rm objects/*.o

profile-use: CFLAGS += $(RELEASEFLAGS) -fprofile-use -fprofile-arcs
profile-use: rm-objects $(OBJECTS) $(COMMON)
	$(CC) $(FLAGS) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

objects/%.o: src/%.c
	mkdir -p `dirname $@`
	$(CC) $(FLAGS) $(CFLAGS) -c $< -o $@
 
install: release
	install -D $(TARGET) $(BINDIR)/$(TARGET)
 
uninstall:
	rm $(BINDIR)/$(TARGET)

test:
	./test/run.sh

clean:
	-rm -rf objects/
	-rm -f gmon.out
	-rm -f tags
 
distclean: clean
	-rm -f $(TARGET)

 
.PHONY: all release profile clean distclean \
        install uninstall test
