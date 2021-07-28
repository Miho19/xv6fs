#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "fs.h" 

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

    printf("Press any key to continue...\n");
    
    while(1) {
        if(getc(stdin))
            break;
    }


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

int iupdate(struct inode *ip, FILE *f){
    struct dinode *diptr;
    int sector = 0;
    int result = 0;
    int i;
    

    unsigned char buffer[BSIZE];

    printf("iupdate: changing inode %d\n", ip->inum);

    memset(buffer, 0, sizeof buffer);

     if(!ip)
        return 1;

    if(ip->inum > sb.ninodes)
        return 1;
   

    sector = IBLOCK(ip->inum);

    result = rsec(sector, buffer, f);

    if(result != BSIZE)
        return 1;
    
    diptr = (struct dinode *)buffer + (ip->inum % IPB);

    diptr->size     = ip->size;
    diptr->nlink    = ip->nlink;
    diptr->type     = ip->type;

    for(i=0;i<NDIRECT && diptr->addrs[i] != 0; i++){
        ip->addrs[i] = diptr->addrs[i];
    }
    
    diptr->addrs[NDIRECT] = ip->addrs[NDIRECT];



    result = wsec(sector, buffer, f);

    if(result != BSIZE){
        printf("Updating inode %d failed\n", ip->inum);
        return 1;
    }


    return 0;
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
    printf("]");

    diptr->addrs[NDIRECT] ? printf("[%d]\n", diptr->addrs[NDIRECT]) : printf("\n") ;


    

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


static void blkzero(uint bn, FILE *f){
    unsigned char buffer[BSIZE];
    memset(buffer, 0, sizeof buffer);
    wsec(bn, buffer, f);
}

uint blkalloc(FILE *f){
    uint bn = 0;
    uint bit = 0;
    uint bit_index = 0;
    uint mask = 0;
    unsigned char buffer[BSIZE];

    for(bit = 0; bit < sb.size; bit += BPB){
        bn = BBLOCK(bit, sb.ninodes);
        memset(buffer, 0, sizeof buffer);
        rsec(bn, buffer, f);

        for(bit_index = 0; bit_index < BPB && bit_index + bit < sb.size;bit_index++){
            mask = 1 << (bit_index % 8);
            if((buffer[bit_index/8] & mask) == 0) {
                buffer[bit_index/8] |= mask;
                wsec(bn, buffer, f);
                blkzero(bit_index + bit, f);
                return bit_index + bit;
            }
        }
    }
    printf("Block Allocation error: out of blocks\n");
    return 0;
}



int ialloc(struct inode *ip, short type, FILE *f){
    uint inum;
    struct dinode *diptr;
    uint i;

    unsigned char buffer[BSIZE];

    memset(buffer, 0, sizeof buffer);
    

    if(!ip) {
        printf("ialloc: Invalid inode pointer\n");
        return 1;
    }
        
    
    if(type != T_DIR && type != T_FILE){
        printf("ialloc: File type not recognised: %d\n", type);
        return 1;
    }

    for(inum = 1; inum < sb.ninodes; inum++){
        rsec(IBLOCK(inum), buffer, f);

        diptr = (struct dinode *)buffer + (inum % IPB);

        if(diptr->type == 0) {
            
            diptr->type = type;
            diptr->nlink = 1;
            diptr->size = 0;
            diptr->major = 0;
            diptr->minor = 0;
            
            for(i=0;i<NDIRECT;i++)
                diptr->addrs[i] = 0;
            diptr->addrs[NDIRECT] = 0;

            wsec(IBLOCK(inum), buffer, f);
            return iget(inum, ip, f);
        }
    }

    printf("Ionde Allocation Error: Out of inodes\n");
    return 1;
}

static void blkfree(uint bn, FILE *f) {
    unsigned char buffer[BSIZE];
    uint bit_index = 0;
    uint mask = 0;

    bit_index = bn % BPB;

    rsec(BBLOCK(bn, sb.ninodes), buffer, f);

    mask = 1 << (bit_index % 8);

    if((buffer[bit_index / 8] & mask) == 0){
        printf("Freeing free block\n");
        return;
    }

    buffer[bit_index / 8] &= ~mask;
    wsec(BBLOCK(bn, sb.ninodes), buffer, f);
}


int iremove(struct inode *ip, FILE *f){
    
    unsigned char buffer[BSIZE];
    struct dinode *diptr;
    uint i;

     unsigned char indirect[BSIZE];
     uint *a;

    memset(buffer, 0, sizeof buffer);

    if(!ip)
        return 1;
    if(ip->inum == 1)
        return 1;

    rsec(IBLOCK(ip->inum), buffer, f);

    diptr = (struct dinode *)(buffer + (ip->inum % IPB));

    if(diptr->type != ip->type)
        return 1;
    
    for(i=0;i<NDIRECT && diptr->addrs[i] != 0;i++){
        blkfree(diptr->addrs[i], f);
    }

    if(diptr->addrs[NDIRECT] != 0){
        memset(indirect, 0, sizeof indirect);
        a = (uint *) (indirect);

        for(i=0;i<BSIZE && a[i] != 0;i++){
            blkfree(a[i], f);
        }

        blkfree(diptr->addrs[NDIRECT], f);
    }
    
    memset(diptr, 0, sizeof *diptr);
    wsec(IBLOCK(ip->inum), buffer, f);
    return 0;
}

int ilink(uint parent, const char *name, struct inode *ip, FILE *f){
    struct inode pip;
    unsigned char buffer[BSIZE];
    struct dirent *d;

    unsigned char indirect[BSIZE];
    uint indirect_block = 0;
    uint *a;

    uint bn = 0;

    uint index = 0;
    uint offset = 0;

    memset(buffer, 0, sizeof buffer);
    memset(&pip, 0, sizeof pip);
   
    
    if(parent < 1){
        printf("ilink: parent ino error %d\n", parent);
        return 1;
    }
        
    
    iget(parent, &pip, f);

    if(pip.type != T_DIR) {
        printf("ilink: parent is not a directory: %d\n", pip.type);
        return 1;
    }
    
    for(offset = 0; index < NDIRECT && pip.addrs[index] != 0; index++){
        index = 0;
        rsec(pip.addrs[index], buffer, f);
        d = (struct dirent *)(buffer);

        while(offset < BSIZE) {

            if(d->inum == 0) {
                memset(d, 0, sizeof *d);
                d->inum = ip->inum;
                strncpy(d->name, name, DIRSIZ);
                wsec(pip.addrs[index], buffer, f);
                pip.size += sizeof(struct dirent);
                iupdate(&pip, f);
                return 0;
            }

            offset += sizeof(struct dirent);
            d = (struct dirent *)(buffer + offset);
        }

    }
    
    if(index < NDIRECT && pip.addrs[index] == 0) {
        bn = blkalloc(f);

        if(bn) {
            pip.addrs[index] = bn; 
        } else {
            printf("ilink: Could not allocate a new block\n");
            return 0;
        }
        
        iupdate(&pip, f);
        memset(buffer, 0, sizeof buffer);
        rsec(pip.addrs[index], buffer, f);
        d = (struct dirent *)(buffer);
        d->inum = ip->inum;
        strncpy(d->name, name, DIRSIZ);
        wsec(pip.addrs[index], buffer, f);
        pip.size += sizeof(struct dirent);
        iupdate(&pip, f);
        return 0;
    }

    if(index == NDIRECT && pip.addrs[NDIRECT]) {
        memset(indirect, 0, sizeof indirect);
        rsec(pip.addrs[NDIRECT], indirect, f);
        a = (uint *)(indirect);

        for(indirect_block = 0; indirect_block < BSIZE && a[indirect_block] != 0; indirect_block++){
            rsec(a[indirect_block], buffer, f);
            d = (struct dirent *)(buffer);
            offset = 0;
            while(offset < BSIZE) {
                if(d->inum == 0) {
                    memset(d, 0, sizeof *d);
                    d->inum = ip->inum;
                    strncpy(d->name, name, DIRSIZ);
                    wsec(a[indirect_block], buffer, f);
                    pip.size += sizeof(struct dirent);
                    iupdate(&pip, f);
                    return 0;
                }
                offset += sizeof(struct dirent);
                d = (struct dirent *)(buffer + offset);
            }
        }
    }

    if(index == NDIRECT && pip.addrs[NDIRECT] == 0) {
         bn = blkalloc(f);

        if(bn) {
            pip.addrs[NDIRECT] = bn; 
        } else {
            printf("ilink: Could not allocate a new indirect block\n");
            return 0;
        }
        iupdate(&pip, f);

        rsec(pip.addrs[NDIRECT], indirect, f);
        
        a = (uint *)(indirect);
        
        bn = blkalloc(f);

        if(bn) {
            a[0] = bn; 
        } else {
            printf("ilink: Could not allocate a new block within indirect block\n");
            return 0;
        }
        wsec(pip.addrs[NDIRECT], indirect, f);
        memset(buffer, 0, sizeof buffer);
        rsec(a[0], buffer, f);

        d = (struct dirent *)(buffer);
        d->inum = ip->inum;
        strncpy(d->name, name, DIRSIZ);
        wsec(a[0], buffer, f);
        pip.size += sizeof(struct dirent);
        iupdate(&pip, f);
        return 0;
    }
    printf("ilink: Couldn't add %s to parent %d likely no space left\n", name, pip.inum);
    return 1;
}

