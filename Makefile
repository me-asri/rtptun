TARGET := rtptun

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
	TARGET := $(TARGET)-static
endif

ifeq ($(PREFIX),)
	PREFIX := /usr/local
endif

SRCS := $(wildcard $(SRCDIR)/*.$(SRCEXT)) $(wildcard $(SRCDIR)/**/*.$(SRCEXT))
DEPS := $(wildcard $(INCDIR)/*.$(DEPEXT)) $(wildcard $(INCDIR)/**/*.$(DEPEXT))
OBJS := $(patsubst $(SRCDIR)/%, $(OBJDIR)/%, $(SRCS:.$(SRCEXT)=.$(OBJEXT)))
BIN := $(BINDIR)/$(TARGET)

.PHONY: all clean install

all: $(BIN)

$(BIN): $(OBJS)
	@mkdir -p $(BINDIR)

	$(CC) -o $(BIN) $^ $(LDFLAGS) $(LIB)

	@echo Compiled $(TARGET)

$(OBJDIR)/%.$(OBJEXT): $(SRCDIR)/%.$(SRCEXT) $(DEPS)
	@mkdir -p $(dir $@)

	$(CC) -c -o $@ $< $(CFLAGS) $(INC)

clean:
	rm -rf $(OBJDIR) $(BINDIR)

install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin/
	install -m 755 $(BIN) $(DESTDIR)$(PREFIX)/bin/