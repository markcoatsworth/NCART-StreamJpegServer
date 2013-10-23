#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

//Liblo includes
#include "lo/lo.h"

void error(int num, const char *m, const char *path);

int generic_handler(const char *path, const char *types, lo_arg **argv,
                    int argc, void *data, void *user_data);

int foo_handler(const char *path, const char *types, lo_arg **argv, int argc,
                void *data, void *user_data);

int capture_handler(const char *path, const char *types, lo_arg **argv, int argc,
                    void *data, void *user_data);

int quit_handler(const char *path, const char *types, lo_arg **argv, int argc,
                 void *data, void *user_data);

void read_stdin(void);
