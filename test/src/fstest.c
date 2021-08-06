#include <criterion/criterion.h>
#include <string.h>

#include "fs.h"

FILE *f = 0;

struct io_ops io;

void startup(void){    
    

    f = fopen("fs.img", "rb+");

    memset(&io, 0, sizeof io);

    io.f = f;
    io.rsec = 0;
    io.wsec = 0;
}

void shutdown(void){
    
}

TestSuite(fstests, .init = startup, .fini = shutdown);

Test(fstests, test_1){
    cr_expect(io.f != 0, "Could not open fs.img");   
}


Test(fstests, test_2){
    unsigned char buffer[BSIZE];
    memset(buffer, 0, sizeof buffer);    
    cr_expect(rsec(1, buffer) == BSIZE, "Bytes Recieved were not BSIZE");
}

Test(fstests, test_3) {
    unsigned char buffer[BSIZE];
    memset(buffer, 0, sizeof buffer);
    cr_expect(rsec(-1, buffer) == 0, "rsec failure should return zero");
}

Test(fstests, test_4){
    struct superblock *sb;
    unsigned char buffer[BSIZE];
    memset(buffer, 0, sizeof buffer);


    rsec(1, buffer);
    sb = (struct superblock *)buffer;
    cr_expect(sb->size == 1024, "Size of superblock incorrect");
}

Test(fstests, test_5){
    struct dinode *diptr;
    uint inum = 1;
    unsigned char buffer[BSIZE];
    memset(buffer, 0, sizeof buffer);

    rsec(IBLOCK(inum), buffer);

    diptr = (struct dinode *)buffer + (inum % IPB);
    cr_expect(diptr->size == BSIZE && diptr->type == T_DIR && diptr->addrs[0] == 29, "Size/Type/disk address block of ROOT inode is incorrect");
}

Test(fstests, test_6){
    struct inode ip;
    memset(&ip, 0, sizeof ip);
    cr_expect(iget(1, &ip) == 0, "Error getting root inode");   
}

Test(fstests, test_7){
    struct inode ip;
    memset(&ip, 0, sizeof ip);
    cr_expect(iget(0, &ip) == 1, "Error getting root inode");   
}

Test(fstests, test_8){
    struct superblock sb;
    struct inode ip;

    memset(&sb, 0, sizeof sb);
    memset(&ip, 0, sizeof ip);
    readsb(&sb);

    cr_expect(iget(sb.ninodes + 1, &ip) == 1, "Error getting root inode");   
}

Test(fstests, test_9){
    struct inode ip;
    memset(&ip, 0, sizeof ip);

    iget(1, &ip);

    cr_expect(ip.inum == 1, "Root inode inum error");   
}

Test(fstests, test_10){
    struct inode ip;
    memset(&ip, 0, sizeof ip);
    

    iget(2, &ip);
    cr_expect(ip.inum != 1, "Inum should not have an inum of 1");   
}

Test(fstests, test_11){
    struct inode ip;
    memset(&ip, 0, sizeof ip);

    iget(1, &ip);
    cr_expect(ip.type == T_DIR, "Root inode should be a directory");   
}

Test(fstests, test_12){
    struct inode ip;
    memset(&ip, 0, sizeof ip);

    ialloc(&ip, T_FILE);

    cr_expect(ip.type == T_FILE && ip.nlink == 1 && ip.size == 0, "Should be able to add a new file");

}

Test(fstests, test_13){
    struct inode ip;
    char name[DIRSIZ];

    memset(&ip, 0, sizeof ip);
    memset(name, 0, sizeof name);
    strncpy(name, "hello.txt", DIRSIZ);
    
    iget(16, &ip);

   cr_expect(ilink(1, name, &ip) == 0, "Should be able to add direnty to root inode");

}

Test(fstests, test_14){
    struct inode ip;
    
    unsigned char data[] = "Hello world!";

    memset(&ip, 0, sizeof ip);

    iget(16, &ip);

    cr_expect(iwrite(&ip, data, strlen((char *)data) + 1, 0) == (int)(strlen((char *)data) + 1), "Write should return length of hello world!");
}

Test(fstests, test_15){
    struct inode ip;
    char name[DIRSIZ];


    memset(&ip, 0, sizeof ip);

    memset(name, 0, sizeof name);
    strncpy(name, "hello.txt", DIRSIZ);
   
    nparent(1, name, &ip, 0);
    

    cr_expect( ip.inum == 16, "Should get the new file by name");
}

Test(fstests, test_16) {
    char name[DIRSIZ];
    struct inode ip;

    memset(&ip, 0, sizeof ip);
    memset(name, 0, sizeof name);
    iparent(1, 16, name, &ip,0);

    cr_expect(strncmp(name, "hello.txt", DIRSIZ) == 0, "Name mismatch from getting new inode");
}

Test(fstests, test_17) {
    unsigned char data[BSIZE];
    struct inode ip;

    memset(&ip, 0, sizeof ip);
    memset(data, 0, sizeof data);

    iget(16, &ip);

    iread(&ip, data, strlen("Hello world!") + 1, 0);

    cr_expect(strncmp((char *)data, "Hello world!", DIRSIZ) == 0, "Mismatch of text");

}













