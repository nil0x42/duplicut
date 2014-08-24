SHELL = /bin/sh
CC    = clang
 
CFLAGS       = -std=gnu99 -Iinclude -march=native -Wall -Wextra -pedantic
DEBUGFLAGS   = -O0 -D DEBUG -g3
RELEASEFLAGS = -O2 -D NDEBUG
 
TARGET  = neatdict
SOURCES = $(wildcard src/*.c)
COMMON  = include/definitions.h include/debug.h
HEADERS = $(wildcard include/*.h)
# OBJECTS = $(SOURCES:.c=.o)
OBJECTS = $(patsubst src/%.c, objects/%.o, $(SOURCES))

 
PREFIX = $(DESTDIR)/usr/local
BINDIR = $(PREFIX)/bin
 
 
all: $(TARGET)
 
$(TARGET): CFLAGS += $(DEBUGFLAGS)
$(TARGET): $(OBJECTS) $(COMMON)
	$(CC) $(FLAGS) $(CFLAGS) $(DEBUGFLAGS) -o $(TARGET) $(OBJECTS)

release: CFLAGS += $(RELEASEFLAGS)
release: $(SOURCES) $(HEADERS) $(COMMON)
	$(CC) $(FLAGS) $(CFLAGS) $(RELEASEFLAGS) -o $(TARGET) $(SOURCES)

objects/%.o: src/%.c
	mkdir -p `dirname $@`
	$(CC) $(FLAGS) $(CFLAGS) -c $< -o $@

profile: CFLAGS += -pg
profile: fclean $(TARGET)

 
install: release
	install -D $(TARGET) $(BINDIR)/$(TARGET)
 
install-strip: release
	install -D -s $(TARGET) $(BINDIR)/$(TARGET)

uninstall:
	-rm $(BINDIR)/$(TARGET)
 

clean:
	-rm -rf objects
	-rm -f gmon.out
 
distclean: clean
	-rm -f $(TARGET)

fclean: distclean


re: fclean all
 
.PHONY: all profile release install install-strip uninstall clean distclean
