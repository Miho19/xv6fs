#ifndef XV6_FUSE_H_
#define XV6_FUSE_H_

#define FUSE_USE_VERSION 34 

#include <fuse_lowlevel.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "fs.h"



int xv6_fuse_run(int argc, char **argv);



#endif