#include <criterion/criterion.h>

#include "fs.h"

FILE *f = 0;

struct inode ip;
struct inode pip;

unsigned char buffer[BSIZE];
unsigned char indirect[BSIZE];
uint *a = 0;


void startup(void){    
    f = fopen("fs.img", "rb+");
    superblock_init(f);
}

void shutdown(void){
    
}

TestSuite(fstests, .init = startup, .fini = shutdown);

Test(fstests, fsget){

    cr_expect(f != 0, "Could not open fs.img");   
}


Test(fstests, readsector){
    int result = 0;
    memset(buffer, 0, sizeof buffer);

    result = rsec(1, buffer, f);
    
    cr_expect(result == BSIZE, "Bytes Recieved were not BSIZE");
}

Test(fstests, readSB){
    struct superblock *sb;

    memset(buffer, 0, sizeof buffer);
    rsec(1, buffer, f);

    sb = (struct superblock *)buffer;
    cr_expect(sb->size == 1024, "Size of superblock incorrect");
}

Test(fstests, readROOT){
    struct dinode *diptr;
    uint inum = 1;

    memset(buffer, 0, sizeof buffer);
    rsec(IBLOCK(inum), buffer, f);

    diptr = (struct dinode *)buffer + inum % IPB;
    cr_expect(diptr->size == BSIZE && diptr->type == T_DIR, "Size or Type of ROOT inode is incorrect");

}

Test(fstests, getROOT){
    memset(&ip, 0, sizeof ip);
    cr_expect(iget(1, &ip, f) == 0, "Error getting root inode");   
}

Test(fstests, getROOTfailure){
    memset(&ip, 0, sizeof ip);
    cr_expect(iget(-1, &ip, f) != 0, "Error getting root inode");   
}





