#include "fs.h" 

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>


static struct superblock sb;


void superblock_init(FILE *f){
    struct superblock *s;
    unsigned char buffer[512];
    memset(buffer, 0, sizeof buffer);
    memset(&sb, 0, sizeof(struct superblock));

    rsec(1, buffer, f);

    s = (struct superblock *)buffer;

    printf("Superblock\n\tsize\t%d\n\tnblocks\t%d\n\tninodes\t%d\n\tnlog\t%d\n\n", s->size, s->nblocks, s->ninodes, s->nlog);
    
    sb.size = s->size;
    sb.nblocks = s->nblocks;
    sb.ninodes = s->ninodes;
    sb.nlog = s->nlog;
}


int rsec(int sec, void *buf, FILE *f) {
    int result = 0;

    if(sec < 0 || sec > 1024)
        return result;
    
    fseek(f, sec * BSIZE, 0);

    result = fread(buf, sizeof(char), BSIZE, f);

    return result;
}

int wsec(int sec, void *buf, FILE *f) {
    int result = 0;

    if (sec < 0 || sec > 1024)
        return result;
    
    fseek(f, sec * BSIZE, 0);

    result = fwrite(buf, sizeof(char), BSIZE, f);

    return result;
}




int iget(uint inum, struct inode *ip, FILE *f){
    struct dinode *diptr;
    int sector = 0;
    int result = 0;
    int i;

    char buffer[BSIZE];
    memset(buffer, 0, sizeof buffer);

    if(inum > sb.ninodes)
        return 1;
    if(!ip)
        return 1;

    sector = IBLOCK(inum);

    
    result = rsec(sector, buffer, f);
    if(result != BSIZE) {
        printf("Read Failure\t:%d\n", result);
        return 1;
    }

    printf("inum\t:%d\nsector\t:%d\n", inum, sector);

    diptr = (struct dinode *)buffer + (inum % IPB);
    


    printf("size\t:%d\ntype\t:%d\n", diptr->size, diptr->type);
    printf("Block Addresses:\n\t[");
    for(i=0;diptr->addrs[i] != 0 && i < NDIRECT;i++)
        printf(" %d ", diptr->addrs[i]);
    printf("]\n");

    //disk inode
    ip->size    = diptr->size;
    ip->type    = diptr->type;
    ip->nlink   = diptr->nlink;
    for(i=0;diptr->addrs[i] != 0 && i < NDIRECT; i++)
        ip->addrs[i] = diptr->addrs[i];
    
    // memory inode
    ip->inum    = inum;
    ip->ref     = 1;
    ip->dev     = 1;
    ip->flags   = 0;
   
    return 0;
}





