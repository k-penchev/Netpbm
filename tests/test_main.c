#include "test.h"

#include <stdio.h>

int npbc_test_failure(const char *file, int line, const char *expression) {
    fprintf(stderr, "%s:%d: assertion failed: %s\n", file, line, expression);
    return 1;
}

int main(void) {
    int failed;

    failed = 0;
    failed += npbc_test_buffer();
    failed += npbc_test_cli();
    failed += npbc_test_container();
    failed += npbc_test_crc32();
    failed += npbc_test_huffman();
    failed += npbc_test_image();
    failed += npbc_test_netpbm();
    failed += npbc_test_rle();
    failed += npbc_test_robustness();
    failed += npbc_test_util();

    if (failed != 0) {
        fprintf(stderr, "%d test group(s) failed\n", failed);
        return 1;
    }

    puts("tests passed");
    return 0;
}
