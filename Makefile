PROGRAM := motify-send
PKGCONFIG_LIBS := glib-2.0 gio-2.0
SOURCES := $(wildcard *.c)
OBJECTS := $(patsubst %.c,%.o,$(SOURCES))
CFLAGS := -Wall -Wextra -std=c99 -O2 $(shell pkg-config --cflags $(PKGCONFIG_LIBS))
CPPFLAGS := -D_POSIX_C_SOURCE=200809L -DPROGRAM_NAME=\"$(PROGRAM)\"
LDLIBS := $(shell pkg-config --libs $(PKGCONFIG_LIBS))

$(PROGRAM): $(OBJECTS)

clean:
	$(RM) $(PROGRAM) $(OBJECTS)

.PHONY: clean
