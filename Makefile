CC ?= cc
PREFIX ?= /usr/local
BINDIR ?= $(PREFIX)/bin
DESTDIR ?=

CPPFLAGS ?=
CFLAGS ?= -std=c17 -O2 -Wall -Wextra -Wpedantic
LDFLAGS ?=
LDLIBS ?=

STRICT_CFLAGS := -std=c17 -O2 -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wformat=2 -Wundef -Wcast-qual -Wwrite-strings -Wstrict-prototypes -Wmissing-prototypes -Werror
SANITIZE_CFLAGS := -std=c17 -O1 -g3 -Wall -Wextra -Wpedantic -Werror -fno-omit-frame-pointer -fsanitize=address,undefined
SANITIZE_LDFLAGS := -fsanitize=address,undefined
ANALYZE_CFLAGS := -std=c17 -O0 -g3 -Wall -Wextra -Wpedantic -Werror -fanalyzer

BUILD_DIR := build
OBJECT_DIR := $(BUILD_DIR)/obj
TEST_OBJECT_DIR := $(BUILD_DIR)/test-obj
PROGRAM := $(BUILD_DIR)/npbc
TEST_PROGRAM := $(BUILD_DIR)/unit-tests
COMMENT_CHECKER := $(BUILD_DIR)/check-no-comments

SOURCES := \
	src/buffer.c \
	src/cli.c \
	src/compression.c \
	src/container.c \
	src/crc32.c \
	src/huffman.c \
	src/image.c \
	src/main.c \
	src/netpbm.c \
	src/rle.c \
	src/status.c \
	src/util.c

LIBRARY_SOURCES := \
	src/buffer.c \
	src/cli.c \
	src/compression.c \
	src/container.c \
	src/crc32.c \
	src/huffman.c \
	src/image.c \
	src/netpbm.c \
	src/rle.c \
	src/status.c \
	src/util.c

TEST_SOURCES := \
	tests/test_main.c \
	tests/unit/test_buffer.c \
	tests/unit/test_cli.c \
	tests/unit/test_container.c \
	tests/unit/test_crc32.c \
	tests/unit/test_huffman.c \
	tests/unit/test_image.c \
	tests/unit/test_netpbm.c \
	tests/unit/test_rle.c \
	tests/robustness/test_malformed.c \
	tests/unit/test_util.c

OBJECTS := $(SOURCES:src/%.c=$(OBJECT_DIR)/%.o)
TEST_LIBRARY_OBJECTS := $(LIBRARY_SOURCES:src/%.c=$(TEST_OBJECT_DIR)/src/%.o)
TEST_OBJECTS := $(TEST_SOURCES:%.c=$(TEST_OBJECT_DIR)/%.o)

COMMENT_FILES := \
	Makefile \
	$(shell find include src tests tools -type f \( -name '*.c' -o -name '*.h' -o -name '*.sh' -o -name '*.awk' -o -name '*.py' \) | sort)

.PHONY: all debug test check analyze sanitize check-comments install uninstall clean

all: $(PROGRAM)

$(PROGRAM): $(OBJECTS)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(OBJECT_DIR)/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -Iinclude $(CFLAGS) -MMD -MP -c $< -o $@

$(TEST_OBJECT_DIR)/src/%.o: src/%.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -Iinclude -Itests $(CFLAGS) -MMD -MP -c $< -o $@

$(TEST_OBJECT_DIR)/%.o: %.c
	@mkdir -p $(@D)
	$(CC) $(CPPFLAGS) -Iinclude -Itests $(CFLAGS) -MMD -MP -c $< -o $@

$(TEST_PROGRAM): $(TEST_LIBRARY_OBJECTS) $(TEST_OBJECTS)
	@mkdir -p $(@D)
	$(CC) $(LDFLAGS) $^ $(LDLIBS) -o $@

$(COMMENT_CHECKER): tools/check_no_comments.c
	@mkdir -p $(@D)
	$(CC) -std=c17 -O2 -Wall -Wextra -Wpedantic -Werror $< -o $@

debug:
	$(MAKE) clean
	$(MAKE) CFLAGS="-std=c17 -O0 -g3 -Wall -Wextra -Wpedantic" all

test: $(PROGRAM) $(TEST_PROGRAM) $(COMMENT_CHECKER)
	$(TEST_PROGRAM)
	mkdir -p $(BUILD_DIR)/test-cli
	sh tests/integration/test_cli.sh $(PROGRAM) $(BUILD_DIR)/test-cli
	mkdir -p $(BUILD_DIR)/test-comment-checker
	sh tests/integration/test_comment_checker.sh $(COMMENT_CHECKER) $(BUILD_DIR)/test-comment-checker
	sh tests/integration/test_install.sh $(PROGRAM) $(BUILD_DIR)/test-install "$(CURDIR)"

check-comments: $(COMMENT_CHECKER)
	$(COMMENT_CHECKER) $(COMMENT_FILES)

check:
	$(MAKE) clean
	$(MAKE) CFLAGS="$(STRICT_CFLAGS)" test
	$(MAKE) analyze
	$(MAKE) check-comments

analyze:
	@if printf '%s\n' 'int value;' | $(CC) -x c - -fanalyzer -fsyntax-only >/dev/null 2>&1; then \
		for source in $(SOURCES); do \
			$(CC) $(CPPFLAGS) -Iinclude $(ANALYZE_CFLAGS) -c "$$source" -o /dev/null || exit 1; \
		done; \
	else \
		printf '%s\n' 'npbc: compiler static analyzer unavailable; analysis skipped' >&2; \
	fi

sanitize:
	@mkdir -p $(BUILD_DIR)
	@printf '%s\n' 'int main(void) { return 0; }' | $(CC) -x c - $(SANITIZE_CFLAGS) $(SANITIZE_LDFLAGS) -o $(BUILD_DIR)/sanitize-probe 2>/dev/null || { printf '%s\n' 'npbc: compiler does not support AddressSanitizer and UndefinedBehaviorSanitizer' >&2; exit 1; }
	@ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(BUILD_DIR)/sanitize-probe >/dev/null 2>&1 || { rm -f $(BUILD_DIR)/sanitize-probe; printf '%s\n' 'npbc: sanitizer runtime is unavailable' >&2; exit 1; }
	@if ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 $(BUILD_DIR)/sanitize-probe >/dev/null 2>&1; then \
		detect_leaks=1; \
	else \
		detect_leaks=0; \
		printf '%s\n' 'npbc: LeakSanitizer unavailable; continuing with AddressSanitizer and UndefinedBehaviorSanitizer' >&2; \
	fi; \
	rm -f $(BUILD_DIR)/sanitize-probe; \
	$(MAKE) clean; \
	ASAN_OPTIONS=detect_leaks=$$detect_leaks:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 $(MAKE) CFLAGS="$(SANITIZE_CFLAGS)" LDFLAGS="$(SANITIZE_LDFLAGS)" test

install: $(PROGRAM)
	mkdir -p "$(DESTDIR)$(BINDIR)"
	cp "$(PROGRAM)" "$(DESTDIR)$(BINDIR)/npbc"
	chmod 755 "$(DESTDIR)$(BINDIR)/npbc"

uninstall:
	rm -f "$(DESTDIR)$(BINDIR)/npbc"

clean:
	rm -rf "$(BUILD_DIR)"

-include $(OBJECTS:.o=.d)
-include $(TEST_LIBRARY_OBJECTS:.o=.d)
-include $(TEST_OBJECTS:.o=.d)
