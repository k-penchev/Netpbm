#ifndef NPBC_CLI_H
#define NPBC_CLI_H

#include "npbc/status.h"

int npbc_cli_exit_code(NpbcStatus status);
int npbc_cli_run(int argc, char **argv);

#endif
