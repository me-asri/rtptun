TARGET := rtptun

ifeq ($(VERSION),)
	VERSION := $(shell git describe --tags)
endif

SRCDIR := src
INCDIR := include
OBJDIR_REL := obj/rel
BINDIR_REL := bin/rel
OBJDIR_DBG := obj/dbg
BINDIR_DBG := bin/dbg

SRCEXT := c
DEPEXT := h
OBJEXT := o

CC := gcc

CFLAGS := -std=c11 -Wall
CFLAGS_REL := -Werror -O3
CFLAGS_DBG := -Og -g -DDEBUG

LDFLAGS :=

INC:= -I$(INCDIR)
LIB:= -lev -lsodium

ifneq ($(VERSION),)
	CFLAGS += -DBUILD_VERSION=\"$(VERSION)\"
endif

ifeq ($(DEBUG),1)
	CFLAGS += $(CFLAGS_DBG)
	OBJDIR := $(OBJDIR_DBG)
	BINDIR := $(BINDIR_DBG)
else
	CFLAGS += $(CFLAGS_REL)
	OBJDIR := $(OBJDIR_REL)
	BINDIR := $(BINDIR_REL)
endif

ifeq ($(STATIC),1)
	LDFLAGS += -static
endif

ifeq ($(PREFIX),)
	PREFIX := /usr/local
endif

SRCS := $(shell find $(SRCDIR) -name *.$(SRCEXT) -type f)
DEPS := $(shell find $(INCDIR) -name *.$(DEPEXT) -type f)
OBJS := $(patsubst $(SRCDIR)/%, $(OBJDIR)/%, $(SRCS:.$(SRCEXT)=.$(OBJEXT)))
BIN := $(BINDIR)/$(TARGET)

ARCH := $(shell uname -m)

ifeq ($(OS),Windows_NT)
	OSNAME := windows

	BIN := $(BIN).exe
	DLLS = $(shell ldd $(BIN) | grep -oP '/usr/bin/.*.dll')
else
	OSNAME := $(shell uname -o | tr A-Z a-z)

	DLLS :=
endif

.PHONY: all clean install uninstall archive

all: $(BIN)

$(BIN): $(OBJS)
	@mkdir -p $(BINDIR)

	$(CC) -o $(BIN) $^ $(LDFLAGS) $(LIB)

	@echo Compiled $(TARGET) $(VERSION)

$(OBJDIR)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXT) $(DEPS)
	@mkdir -p $(dir $@)

	$(CC) -c -o $@ $< $(CFLAGS) $(INC)

clean:
	rm -rf $(OBJDIR) $(BINDIR) $(TARGET)-$(OSNAME)-$(ARCH).zip

install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/

uninstall:
	rm $(DESTDIR)$(PREFIX)/bin/$(TARGET)

archive: $(BIN)
	zip -j $(TARGET)-$(OSNAME)-$(ARCH).zip ./README.md ./LICENSE $(BIN) $(DLLS)