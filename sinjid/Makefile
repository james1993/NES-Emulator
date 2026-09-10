# Sinjid: Shadow of the Warrior -- raylib/C fan remake
# Requires raylib 6.0 or newer (developed against 6.1-dev).
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

HDRS := $(wildcard src/*.h)

%.o: %.c $(HDRS)
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(BIN)

# The prebuilt binary in bin/ is checked in so the game can be run straight
# from a clone.  It is not rebuilt by `make`, so it silently goes stale unless
# this is run: do it in any commit that touches src/.
PREBUILT := bin/sinjid-linux-x86_64

$(PREBUILT): $(BIN)
	@mkdir -p bin
	cp $(BIN) $@
	@echo "prebuilt refreshed; needs glibc $$(objdump -T $@ | \
	  grep -o 'GLIBC_[0-9.]*' | sort -uV | tail -1)"

prebuilt: $(PREBUILT)

.PHONY: all clean prebuilt
