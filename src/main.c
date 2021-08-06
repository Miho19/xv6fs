#include "xv6fuse.h"

struct io_ops io;

int main(int argc, char **argv) {
    
    FILE *f = 0;

    memset(&io, 0, sizeof io);


    f = fopen("fs.img", "rb+");        

    if(!f) {
        printf("Could not open fs.img\n");
        return 1;
    }

    io.f = f;
    io.rsec = &rsec;
    io.wsec = &wsec;


    xv6_fuse_run(argc, argv);
    
    return 0;    

}
