TARGET       = duplicut

SHELL        = /bin/sh
 
CFLAGS       = -Iinclude -Wall -Wextra \
			   -Wdisabled-optimization -Winline \
			   -Wunsuffixed-float-constants \
			   -Wdouble-promotion -Wunknown-pragmas \
			   -Wno-unknown-warning-option \
			   -mtune=native -ffast-math
LDFLAGS      = -lm -pthread
RELEASEFLAGS = -O2 -D NDEBUG
DEBUGFLAGS   = -O0 -D DEBUG -std=gnu99 -g3
 
SOURCES      = main.c thpool.c file.c chunk.c line.c tag_duplicates.c \
			   optparse.c config.c error.c memstate.c meminfo.c bytesize.c \
			   hmap.c hash.c fasthash.c murmur3.c \

COMMON       = include/const.h include/debug.h
OBJECTS      = $(patsubst %.c, objects/%.o, $(SOURCES))
 
PREFIX       = $(DESTDIR)/usr/local
BINDIR       = $(PREFIX)/bin
 
 
all: $(TARGET)
 
$(TARGET): CFLAGS += $(DEBUGFLAGS)
$(TARGET): $(OBJECTS) $(COMMON)
	-ctags -R .
	$(CC) $(FLAGS) $(CFLAGS) $(DEBUGFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

release: CFLAGS += $(RELEASEFLAGS)
release: fclean $(OBJECTS) $(COMMON)
	$(CC) $(FLAGS) $(CFLAGS) $(RELEASEFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

profile: CFLAGS += -pg
profile: fclean $(TARGET)

objects/%.o: src/%.c
	mkdir -p `dirname $@`
	$(CC) $(FLAGS) $(CFLAGS) -c $< -o $@
 
install: release
	install -D $(TARGET) $(BINDIR)/$(TARGET)
 
install-strip: release
	install -D -s $(TARGET) $(BINDIR)/$(TARGET)

uninstall:
	rm $(BINDIR)/$(TARGET)

clean:
	-rm -rf objects/
	-rm -f gmon.out
	-rm -f tags
 
distclean: clean
	-rm -f $(TARGET)

fclean: distclean

re: fclean all
 
.PHONY: all release profile \
        clean distclean \
        install install-strip uninstall
