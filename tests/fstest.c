#include <criterion/criterion.h>

#include "../include/fs.h"

FILE *f = 0;
struct superblock sb;

unsigned char buffer[BSIZE];
unsigned char indirect[BSIZE];
uint *a = 0;

void startup(void){    
    memset(&sb, 0, sizeof sb);
    memset(buffer, 0, sizeof buffer);
}

void shutdown(void){
    
}

TestSuite(fstests, .init = startup, .fini = shutdown);

Test(fstests, openFS){
    f = fopen("fs.img", "rb+");        
    if(!f) {
        printf("Could not open fs.img\n");
    }

    cr_expect(f != 0, "Could not open fs.img");
   
}
