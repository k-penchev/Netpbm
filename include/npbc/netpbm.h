#ifndef NPBC_NETPBM_H
#define NPBC_NETPBM_H

#include <stdio.h>

#include "npbc/image.h"
#include "npbc/status.h"

NpbcStatus npbc_netpbm_read(FILE *stream, NpbcImage *image);
NpbcStatus npbc_netpbm_write(FILE *stream, const NpbcImage *image);

#endif
