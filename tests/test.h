#ifndef NPBC_TEST_H
#define NPBC_TEST_H

#include <stddef.h>
#include <string.h>

int npbc_test_failure(const char *file, int line, const char *expression);
int npbc_test_buffer(void);
int npbc_test_cli(void);
int npbc_test_container(void);
int npbc_test_crc32(void);
int npbc_test_huffman(void);
int npbc_test_image(void);
int npbc_test_netpbm(void);
int npbc_test_rle(void);
int npbc_test_robustness(void);
int npbc_test_util(void);

#define NPBC_ASSERT(expression) \
    do { \
        if (!(expression)) { \
            return npbc_test_failure(__FILE__, __LINE__, #expression); \
        } \
    } while (0)

#define NPBC_ASSERT_BYTES(left, right, count) \
    do { \
        if ((count) != 0 && memcmp((left), (right), (count)) != 0) { \
            return npbc_test_failure(__FILE__, __LINE__, "byte comparison"); \
        } \
    } while (0)

#endif
