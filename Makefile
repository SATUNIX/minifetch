CC ?= cc
CPPFLAGS ?=
CFLAGS ?=
LDFLAGS ?=

CPPFLAGS += -Iinclude
CFLAGS += -std=c89 -Wall -Wextra -Werror -pedantic -D_POSIX_C_SOURCE=200809L

BUILD_DIR ?= build
LOGO_TXT = frames/logo.txt
LOGO_SRC = $(BUILD_DIR)/logo_data.c

SRC_BASE = \
	src/main.c \
	src/cli.c \
	src/core.c \
	src/linux_extras.c \
	src/compat.c \
	src/term.c

SRCS = $(SRC_BASE) $(LOGO_SRC)

.PHONY: all clean

all: minifetch

minifetch: $(LOGO_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(SRCS) -o $@

minifetch-linux: $(LOGO_SRC)
	$(CC) $(CPPFLAGS) $(CFLAGS) -DMINIFETCH_LINUX_EXT=1 $(LDFLAGS) $(SRCS) -o $@

$(LOGO_SRC): $(LOGO_TXT) tools/embed_logo.sh | $(BUILD_DIR)
	tools/embed_logo.sh $(LOGO_TXT) $(LOGO_SRC)

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

clean:
	rm -f minifetch minifetch-linux
	rm -rf $(BUILD_DIR)
