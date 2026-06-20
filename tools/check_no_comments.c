#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    SCAN_NORMAL,
    SCAN_STRING,
    SCAN_CHARACTER,
    SCAN_LINE_COMMENT,
    SCAN_BLOCK_COMMENT
} ScanState;

static int uses_hash_comments(const char *path) {
    size_t length = strlen(path);

    if (strcmp(path, "Makefile") == 0) {
        return 1;
    }
    if (length >= 3 && strcmp(path + length - 3, ".sh") == 0) {
        return 1;
    }

    return 0;
}

static int is_shell_boundary(int character) {
    return character == 0
        || character == ' '
        || character == '\t'
        || character == '\r'
        || character == '\n'
        || character == ';'
        || character == '|'
        || character == '&'
        || character == '('
        || character == ')';
}

static int scan_hash_comments(FILE *stream, const char *path) {
    int character;
    int previous;
    int quote;
    int escaped;
    int make_style;
    size_t line;

    previous = 0;
    quote = 0;
    escaped = 0;
    make_style = strcmp(path, "Makefile") == 0;
    line = 1;
    while ((character = fgetc(stream)) != EOF) {
        if (character == '\n') {
            line++;
            quote = 0;
            escaped = 0;
            previous = character;
            continue;
        }
        if (escaped) {
            escaped = 0;
            previous = character;
            continue;
        }
        if (character == '\\') {
            escaped = 1;
            previous = character;
            continue;
        }
        if (quote != 0) {
            if (character == quote) {
                quote = 0;
            }
            previous = character;
            continue;
        }
        if (character == '\'' || character == '"') {
            quote = character;
            previous = character;
            continue;
        }
        if (
            character == '#'
            && (make_style || (previous != '$' && is_shell_boundary(previous)))
        ) {
            fprintf(stderr, "%s:%zu: forbidden comment\n", path, line);
            return 1;
        }
        previous = character;
    }

    return 0;
}

static int scan_c_comments(FILE *stream, const char *path) {
    ScanState state;
    int character;
    int previous;
    int escaped;
    size_t line;
    size_t comment_line;

    state = SCAN_NORMAL;
    previous = 0;
    escaped = 0;
    line = 1;
    comment_line = 0;

    while ((character = fgetc(stream)) != EOF) {
        if (state == SCAN_LINE_COMMENT) {
            if (character == '\n') {
                fprintf(
                    stderr,
                    "%s:%zu: forbidden comment\n",
                    path,
                    comment_line
                );
                return 1;
            }
            continue;
        }

        if (state == SCAN_BLOCK_COMMENT) {
            if (previous == '*' && character == '/') {
                fprintf(
                    stderr,
                    "%s:%zu: forbidden comment\n",
                    path,
                    comment_line
                );
                return 1;
            }
            if (character == '\n') {
                line++;
            }
            previous = character;
            continue;
        }

        if (state == SCAN_STRING || state == SCAN_CHARACTER) {
            if (character == '\n') {
                line++;
            }
            if (escaped) {
                escaped = 0;
                continue;
            }
            if (character == '\\') {
                escaped = 1;
                continue;
            }
            if (
                (state == SCAN_STRING && character == '"')
                || (state == SCAN_CHARACTER && character == '\'')
            ) {
                state = SCAN_NORMAL;
            }
            continue;
        }

        if (previous == '/' && character == '/') {
            state = SCAN_LINE_COMMENT;
            comment_line = line;
            previous = 0;
            continue;
        }
        if (previous == '/' && character == '*') {
            state = SCAN_BLOCK_COMMENT;
            comment_line = line;
            previous = 0;
            continue;
        }
        if (character == '"') {
            state = SCAN_STRING;
            previous = 0;
            continue;
        }
        if (character == '\'') {
            state = SCAN_CHARACTER;
            previous = 0;
            continue;
        }
        if (character == '\n') {
            line++;
        }
        previous = character == '/' ? character : 0;
    }

    if (state == SCAN_LINE_COMMENT || state == SCAN_BLOCK_COMMENT) {
        fprintf(stderr, "%s:%zu: forbidden comment\n", path, comment_line);
        return 1;
    }

    return 0;
}

static int scan_file(const char *path) {
    FILE *stream;
    int result;

    stream = fopen(path, "rb");
    if (stream == NULL) {
        fprintf(stderr, "%s: cannot open\n", path);
        return 1;
    }

    if (uses_hash_comments(path)) {
        result = scan_hash_comments(stream, path);
    } else {
        result = scan_c_comments(stream, path);
    }

    if (fclose(stream) != 0) {
        fprintf(stderr, "%s: cannot close\n", path);
        return 1;
    }

    return result;
}

int main(int argc, char **argv) {
    int index;
    int failed;

    failed = 0;
    for (index = 1; index < argc; index++) {
        failed |= scan_file(argv[index]);
    }

    return failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
