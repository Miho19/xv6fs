#include <criterion/criterion.h>

#include "fs.h"

FILE *f = 0;
struct inode ip;

unsigned char buffer[BSIZE];


void startup(void){    
    f = fopen("fs.img", "rb+");
}

void shutdown(void){
    
}

TestSuite(fstests, .init = startup, .fini = shutdown);

Test(fstests, fsget){
    cr_expect(f != 0, "Could not open fs.img");   
}


Test(fstests, readsector){
    memset(buffer, 0, sizeof buffer);    
    cr_expect(rsec(1, buffer, f) == BSIZE, "Bytes Recieved were not BSIZE");
}

Test(fstests, readSectorincorrect) {
    memset(buffer, 0, sizeof buffer);
    cr_expect(rsec(-1, buffer, f) == 0, "rsec failure should return zero");
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

    diptr = (struct dinode *)buffer + (inum % IPB);
    cr_expect(diptr->size == BSIZE && diptr->type == T_DIR && diptr->addrs[0] == 29, "Size/Type/disk address block of ROOT inode is incorrect");
}

Test(fstests, getROOT){
    memset(&ip, 0, sizeof ip);
    cr_expect(iget(1, &ip, f) == 0, "Error getting root inode");   
}

Test(fstests, getROOTfailure){
    memset(&ip, 0, sizeof ip);
    cr_expect(iget(0, &ip, f) == 1, "Error getting root inode");   
}

Test(fstests, getROOTfailure2){
    struct superblock sb;
    memset(&sb, 0, sizeof sb);
    memset(&ip, 0, sizeof ip);

    readsb(&sb, f);

    cr_expect(iget(sb.ninodes + 1, &ip, f) == 1, "Error getting root inode");   
}

Test(fstests, getROOTinum){
    
    memset(&ip, 0, sizeof ip);
    iget(1, &ip, f);

    cr_expect(ip.inum == 1, "Root inode inum error");   
}

Test(fstests, getROOTinumfailure){
    
    memset(&ip, 0, sizeof ip);
    iget(2, &ip, f);

    cr_expect(ip.inum != 1, "Inum should not have an inum of 1");   
}

Test(fstests, getROOTinumtype){
    
    memset(&ip, 0, sizeof ip);
    iget(1, &ip, f);
    
    cr_expect(ip.type == T_DIR, "Root inode should be a directory");   
}









