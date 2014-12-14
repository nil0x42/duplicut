SHELL        = /bin/sh
 
CFLAGS       = -Iinclude -march=native -Wall -Wextra -pedantic
LDFLAGS      = -lm
RELEASEFLAGS = -O2 -D NDEBUG
DEBUGFLAGS   = -O0 -D DEBUG -std=gnu99 -g3 -Wno-gnu-statement-expression
 
TARGET       = duplicut
SOURCES      = $(wildcard src/*.c)
COMMON       = include/definitions.h include/debug.h
HEADERS      = $(wildcard include/*.h)
OBJECTS      = $(patsubst src/%.c, objects/%.o, $(SOURCES))

 
PREFIX       = $(DESTDIR)/usr/local
BINDIR       = $(PREFIX)/bin
 
 
all: $(TARGET)
 
$(TARGET): CFLAGS += $(DEBUGFLAGS)
$(TARGET): $(OBJECTS) $(COMMON)
	-ctags -R .
	$(CC) $(FLAGS) $(CFLAGS) $(DEBUGFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS)

release: CFLAGS += $(RELEASEFLAGS)
release: fclean $(SOURCES) $(HEADERS) $(COMMON)
	$(CC) $(FLAGS) $(CFLAGS) $(RELEASEFLAGS) -o $(TARGET) $(SOURCES) $(LDFLAGS)

objects/%.o: src/%.c
	mkdir -p `dirname $@`
	$(CC) $(FLAGS) $(CFLAGS) -c $< -o $@ $(LDFLAGS)

profile: CFLAGS += -pg
profile: fclean $(TARGET)

 
install: release
	install -D $(TARGET) $(BINDIR)/$(TARGET)
 
install-strip: release
	install -D -s $(TARGET) $(BINDIR)/$(TARGET)

uninstall:
	rm $(BINDIR)/$(TARGET)
 

clean:
	-rm -rf objects
	-rm -f gmon.out
	-rm -f tags
 
distclean: clean
	-rm -f $(TARGET)

fclean: distclean


re: fclean all
 
.PHONY: all release profile tags \
        clean distclean \
        install install-strip uninstall
