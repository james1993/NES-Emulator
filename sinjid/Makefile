# Sinjid: Shadow of the Warrior -- raylib/C fan remake
# Build:  make            (uses pkg-config raylib, or RAYLIB=/path/to/raylib)
#         make RAYLIB=../raylib
CC      ?= cc
CFLAGS  ?= -std=c99 -O2 -Wall -Wextra -Wno-unused-parameter
LDFLAGS ?=
SRC     := $(wildcard src/*.c)
OBJ     := $(SRC:.c=.o)
BIN     := sinjid

ifdef RAYLIB
  CFLAGS  += -I$(RAYLIB)/src
  LDLIBS  := $(RAYLIB)/src/libraylib.a -lm -lpthread -ldl -lrt -lX11
else
  RAYLIB_CFLAGS := $(shell pkg-config --cflags raylib 2>/dev/null)
  RAYLIB_LIBS   := $(shell pkg-config --libs raylib 2>/dev/null)
  ifeq ($(RAYLIB_LIBS),)
    RAYLIB_LIBS := -lraylib -lm -lpthread -ldl -lrt -lX11
  endif
  CFLAGS += $(RAYLIB_CFLAGS)
  LDLIBS := $(RAYLIB_LIBS) -lm
endif

all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

%.o: %.c src/game.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN)

.PHONY: all clean
