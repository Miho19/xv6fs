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


    struct dirent *d;
    
    unsigned char buffer[BSIZE];

    memset(buffer, 0, sizeof buffer);

    if(inum > sb.ninodes)
        return 1;
    if(!ip)
        return 1;

    sector = IBLOCK(inum);

    printf("Sector %d inum %d\n", sector, inum);
    result = rsec(sector + 1, buffer, f);

    printf("Result = %d\n", result);

    diptr = (struct dinode *)buffer;
    


    printf("type %d\naddrs: %d\n", diptr->type, diptr->addrs[0]);
    memset(buffer, 0, sizeof buffer);


    result = rsec(diptr->addrs[0], buffer, f);

    d = (struct dirent *) buffer;

    printf("%s\n", d->name);

    while(1) {
        if(getc(stdin))
            break;
    }





    return 0;
}





