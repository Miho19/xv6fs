#include "xv6fuse.h"

#include "xv6usb.h"

struct io_ops io;

int main(int argc, char **argv) {
    
    FILE *f = 0;
    int result = 0;

    memset(&io, 0, sizeof io);

    result = usb_init();

    if(!result){
        io.f = 0;
        io.rsec = &read_sector;
        io.wsec = &write_sector;
        printf("\n\nxv6fs: Using USB as disk\n\n");
    } else {
        f = fopen("fs.img", "rb+");        

        if(!f) {
            printf("Could not open fs.img\n");
            return 1;
        }

        io.f = f;
        io.rsec = &rsec;
        io.wsec = &wsec;
        printf("\n\nxv6fs: Using fs.img as disk\n");
    }

    printf("EXIT CODE\nFUSE\t(%d)\n", xv6_fuse_run(argc, argv));
    !result ? printf("USB\t(%d)\n", usb_close()) : printf("\t");

    return 0;    

}
