#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "fs.h"


/** 
 * readsb.
 * Read the superblock into sb pointer.
*/
void readsb(struct superblock *sb){
    unsigned char buffer[BSIZE];
    struct superblock *s;

    memset(buffer, 0, sizeof buffer);
    rsec(1, buffer);

    s = (struct superblock *)buffer;

    sb->nblocks      = s->nblocks;
    sb->ninodes      = s->ninodes;
    sb->nlog         = s->nlog;
    sb->size         = s->size;
}
/** 
 * superblock_read.
 * Print out superblock data of the xv6 File System.
*/
void superblock_read(){
    struct superblock sb;
    memset(&sb, 0, sizeof(struct superblock));
    readsb(&sb);
    printf("Superblock\n\tsize\t%d\n\tnblocks\t%d\n\tninodes\t%d\n\tnlog\t%d\n\n", sb.size, sb.nblocks, sb.ninodes, sb.nlog);
}
/** 
 * rsec.
 * Read a sector from f.
 * Returns  0 on Failure.
 *          Number of bytes read on success.
*/
int rsec(int sec, void *buf) {


    if(sec < 0 || sec > 1024)
        return 0;
    
    if(io.f){
        fseek(io.f, sec * BSIZE, 0);
        return fread(buf, sizeof(char), BSIZE, io.f);
    }

    return io.rsec(sec, buf);
}
/** 
 * wsec.
 * Write a sector to f.
 * Returns  0 on Failure.
 *          Number of bytes written on success.
*/
int wsec(int sec, void *buf) {
    
    if (sec < 0 || sec > 1024)
        return 0;
    
    if(io.f){
        fseek(io.f, sec * BSIZE, 0);
        return fwrite(buf, sizeof(char), BSIZE, io.f);
    }

    return io.wsec(sec, buf);
}
/** 
 * iupdate.
 * Update the disk inode of inode ip.
 * Returns  1 on Failure.
 *          0 on Success.
*/
int iupdate(struct inode *ip){
    struct dinode *diptr;
    int sector = 0;
    int result = 0;
    int i;

    struct superblock sb;
    unsigned char buffer[BSIZE];

    memset(buffer, 0, sizeof buffer);
    memset(&sb, 0, sizeof sb);


    readsb(&sb);

    if(DEBUG)
        printf("iupdate: changing inode %d\n", ip->inum);

     if(!ip) {
        if(DEBUG)
            printf("iupdate: Invalid inode pointer\n");
        return 1;
     }

    if(ip->inum > sb.ninodes || ip->inum < 1) {
        if(DEBUG)
            printf("iupdate: inum # out of range: (%d) expected (1 - %d)\n", ip->inum, sb.ninodes);
        return 1;
    }
   
    sector = IBLOCK(ip->inum);
    result = rsec(sector, buffer);

    if(result != BSIZE){
        if(DEBUG)
            printf("iupdate: Reading sector(%d) returned %d bytes\n", sector, result);
        return 1;
    }
    
    diptr = (struct dinode *)buffer + (ip->inum % IPB);

    diptr->size     = ip->size;
    diptr->nlink    = ip->nlink;
    diptr->type     = ip->type;

    for(i=0;i<NDIRECT + 1; i++){
        diptr->addrs[i] = ip->addrs[i];
    }

    result = wsec(sector, buffer);

    if(result != BSIZE){
        if(DEBUG)
            printf("iupdate: Writing inode(%d) at sector(%d) returned %d bytes\n", ip->inum, sector, result);
        return 1;
    }


    return 0;
}
/** 
 * iget.
 * Will retrieve the disk inode of inum and set
 * the inode ip with its values.
 * Returns  1 on Failure.
 *          0 on Success.
*/
int iget(uint inum, struct inode *ip){
    struct dinode *diptr;
    int sector = 0;
    int result = 0;
    int i;

    unsigned char buffer[BSIZE];
    struct superblock sb;

    memset(&sb, 0, sizeof sb);
    memset(buffer, 0, sizeof buffer);

    readsb(&sb);

    if(inum > sb.ninodes || inum < 1) {
        if(DEBUG)
            printf("iget: inum # out of range: (%d) expected (1 - %d)\n", ip->inum, sb.ninodes);
        return 1;
    }

    if(!ip) {
        if(DEBUG)
            printf("iget: Invalid inode pointer\n");
        return 1;
    }
        

    sector = IBLOCK(inum);
    result = rsec(sector, buffer);

    if(result != BSIZE) {
        if(DEBUG)
            printf("iget: Reading sector(%d) returned(%d) bytes\n", sector, result);
        return 1;
    }

    diptr = (struct dinode *)buffer + (inum % IPB);


    if(DEBUG) {
        printf("Inode %d\n", inum);
        printf("size\t: %d\n", diptr->size);
        printf("type\t: %s\n", diptr->type == T_DIR ? "Directory" : "File");
        printf("nlink\t: %d\n", diptr->nlink);
        printf("Block Addresses:\n\t[");
        for(i=0;i<NDIRECT;i++){
            if(diptr->addrs[i]){
                printf(" %d ", diptr->addrs[i]);
            }
        }
        printf("]");
        diptr->addrs[NDIRECT] ? printf("[%d]\n", diptr->addrs[NDIRECT]) : printf("\n");
    }

    
    ip->size    = diptr->size;
    ip->type    = diptr->type;
    ip->nlink   = diptr->nlink;

    memmove(ip->addrs, diptr->addrs, sizeof(ip->addrs));

    
    ip->inum    = inum;
    ip->ref     = 1;
    ip->dev     = 1;
    ip->flags   = 0;
   
    return 0;
}

static void blkzero(uint bn){
    unsigned char buffer[BSIZE];
    memset(buffer, 0, sizeof buffer);
    wsec(bn, buffer);
}

/** 
 * blkalloc.
 * Allocate a new zeroed block.
 * Returns 0 on failure
 * Returns a disk address block number of success.
*/
uint blkalloc(){
    uint bn = 0;
    uint bit = 0;
    uint bit_index = 0;
    uint mask = 0;

    struct superblock sb;
    unsigned char buffer[BSIZE];

    memset(&sb, 0, sizeof sb);

    readsb(&sb);

    for(bit = 0; bit < sb.size; bit += BPB){
        bn = BBLOCK(bit, sb.ninodes);
        memset(buffer, 0, sizeof buffer);
        rsec(bn, buffer);

        for(bit_index = 0; bit_index < BPB && bit_index + bit < sb.size;bit_index++){
            mask = 1 << (bit_index % 8);
            if((buffer[bit_index/8] & mask) == 0) {
                buffer[bit_index/8] |= mask;
                wsec(bn, buffer);
                blkzero(bit_index + bit);
                return bit_index + bit;
            }
        }
    }
    if(DEBUG)
        printf("Block Allocation error: out of blocks\n");
    return 0;
}
/** 
 * ialloc.
 * Searches for an empty inode and claims it by setting
 *  initial values.
 * Will set ip inode pointer to new inode on Success.
 * Returns  1 on error.
 *          0 on success.
 * 
*/
int ialloc(struct inode *ip, short type){
    uint inum;
    struct dinode *diptr;
    uint i;

    struct superblock sb;

    unsigned char buffer[BSIZE];

    memset(buffer, 0, sizeof buffer);
    memset(&sb, 0, sizeof sb);

    readsb(&sb);

    if(!ip) {
        if(DEBUG)
            printf("ialloc: Invalid inode pointer\n");
        return 1;
    }

    if(type != T_DIR && type != T_FILE){
        if(DEBUG)
            printf("ialloc: File type not recognised: (%d)\n", type);
        return 1;
    }

    for(inum = 1; inum < sb.ninodes; inum++){
        rsec(IBLOCK(inum), buffer);

        diptr = (struct dinode *)buffer + (inum % IPB);

        if(diptr->type == 0) {
            diptr->type = type;
            diptr->nlink = 1;
            diptr->size = 0;
            diptr->major = 0;
            diptr->minor = 0;
            
            for(i=0;i<NDIRECT + 1;i++)
                diptr->addrs[i] = 0;
            wsec(IBLOCK(inum), buffer);
            return iget(inum, ip);
        }
    }

    if(DEBUG)
        printf("Ionde Allocation Error: Out of inodes\n");
    return 1;
}
/** 
 * blkfree.
 * Frees a block by toggling its bitmap index.
*/
static void blkfree(uint bn) {
    unsigned char buffer[BSIZE];
    struct superblock sb;
    uint bit_index = 0;
    uint mask = 0;

    memset(buffer, 0, sizeof buffer);
    memset(&sb, 0, sizeof sb);

    readsb(&sb);

    bit_index = bn % BPB;

    rsec(BBLOCK(bn, sb.ninodes), buffer);

    mask = 1 << (bit_index % 8);

    if((buffer[bit_index / 8] & mask) == 0){
        if(DEBUG)
            printf("blkfree: Freeing free block\n");
        return;
    }

    buffer[bit_index / 8] &= ~mask;
    wsec(BBLOCK(bn, sb.ninodes), buffer);
}
/** 
 * iremove.
 * Removes an inode ip from the File System.
 * Will free all data blocks associate with inode.
 * Returns  1 on error.
 *          0 on success.
*/
int iremove(struct inode *ip){
    
    unsigned char buffer[BSIZE];
    struct dinode *diptr;
    uint i;

    unsigned char indirect[BSIZE];
    uint *a;

    memset(buffer, 0, sizeof buffer);

    if(DEBUG){
        printf("iremove: Removing inode %d\n", ip->inum);
    }

    if(!ip) {
        if(DEBUG)
            printf("iremove: Invalid inode pointer\n");
        return 1;
    }

    if(ip->inum == 1) {
        if(DEBUG)
            printf("iremove: Can not remove root directory\n");
        return 1;
    }

    rsec(IBLOCK(ip->inum), buffer);

    diptr = (struct dinode *)buffer + (ip->inum % IPB);

    if(diptr->type != ip->type) {
        if(DEBUG)
            printf("iremove: TYPE DIFFERENCE found(%d) expected (%d)", diptr->type, ip->type);
        return 1;
    }
    
    for(i=0;i<NDIRECT;i++){
        if(diptr->addrs[i] == 0)
            continue;
        blkfree(diptr->addrs[i]);
    }

    if(diptr->addrs[NDIRECT]){
        memset(indirect, 0, sizeof indirect);
        a = (uint *) (indirect);
        for(i=0;i<BSIZE;i++){
            if(a[i] == 0)
                continue;
            
            blkfree(a[i]);
        }
        blkfree(diptr->addrs[NDIRECT]);
    }

    memset(diptr, 0, sizeof(struct dinode));
    wsec(IBLOCK(ip->inum), buffer);

    
    return 0;
}

/** 
 * ilink.
 * Finds an empty direnty within a parent and claims it for inode ip.
 * Increases nlink of parent.
 * Returns  1 on error.
 *          0 on success.
*/
int ilink(uint parent, const char *name, struct inode *ip){
   
    struct inode pip;
    struct superblock sb;

    struct dirent de;
    uint offset = 0;

    int result = 0;

    memset(&pip, 0, sizeof pip);
    memset(&sb, 0, sizeof sb);


    readsb(&sb);

    
    if(parent < 1 || parent > sb.ninodes){
        if(DEBUG)
            printf("ilink: inum # out of range: (%d) expected (1 - %d)\n", parent, sb.ninodes);
        return 1;
    }
    
    if(iget(parent, &pip)){
        if(DEBUG)
            printf("iupdate: Could not retrieve parent(%d)\n", parent);
        return 1;
    }

    if(pip.type != T_DIR) {
        if(DEBUG)
            printf("ilink: parent(%d) is not a directory(%d)\n", pip.inum, pip.type);
        return 1;
    }


    for(offset = 0; offset < pip.size; offset += sizeof(struct dirent)){
        result = iread(&pip, (unsigned char *)&de, sizeof(de), offset);
        if(result != sizeof(struct dirent)){
            if(DEBUG)
                printf("ilink: Reading inode returned (%d) expected (%ld)\n", result, sizeof(struct dirent));
            return 1;
        }
        
        if(de.inum == 0){
            if(DEBUG)
                printf("ilink: Empty direnty at offset (%d)\n", offset);
            break;
        }
    }

    if(de.inum != 0){
        if(DEBUG)
            printf("ilink: Could not find empty inode in parent(%d)\n", parent);
        return 1;
    }

    strncpy(de.name, name, DIRSIZ);
    de.inum = ip->inum;

    iwrite(&pip, (unsigned char *)&de, sizeof(struct dirent), offset);

    pip.nlink++;
    iupdate(&pip);

    return 0;

}

/** 
 *  nparent.
 *  Inspects a parent directory for an entry by name.
 *  Updates ip pointer and dirent_offset on success.
 *  Returns 1 on error. 
 *          0 on success.
 *  
*/  

int nparent(uint parent, const char *name, struct inode *ip, struct dirent_offset * doff){

    struct inode pip;
    struct dirent de;

    uint offset = 0;

    memset(&pip, 0, sizeof pip);


    if(!name){
        if(DEBUG)
            printf("nparent: Name pointer is not valid\n");
        return 1;
    }

    if(!ip){
        if(DEBUG)
            printf("nparent: inode pointer is not valid\n");
        return 1;
    }

    if(iget(parent, &pip)){
        if(DEBUG)
            printf("nparent: Could not retrieve parent(%d) inode\n", parent);
        return 1;
    }

    for(offset = 0; offset < pip.size; offset += sizeof(struct dirent)){
        memset(&de, 0, sizeof de);
        iread(&pip, (unsigned char *)&de, sizeof(struct dirent), offset);
        if(de.inum == 0)
            continue;
        if(strncmp(de.name, name, DIRSIZ) == 0){
            iget(de.inum, ip);
            if(doff){
                doff->offset = offset;
                doff->sector = bmapr(&pip, offset/BSIZE);
            }
            return 0;
        }
    }
    if(DEBUG)
        printf("nparent: Was unable to locate (%s) within parent (%d)\n", name, parent);
    return 1;

}
/** 
 * iparent.
 * Inspects a parent directory for an entry by inode number.
 * Updates name pointer and dirent_offset value on success.
 * Returns  1 on error.
 *          0 on success.
 * 
*/
int iparent(uint parent, uint query, char *name, struct inode *ip, struct dirent_offset * doff){
    
    struct inode pip;
    struct dirent de;

    struct superblock sb;

    uint offset = 0;

    memset(&pip, 0, sizeof pip);
    memset(&sb, 0, sizeof sb);

    readsb(&sb);

    if(!ip){
        if(DEBUG)
            printf("iparent: Invalid memory location for ip pointer\n");
        return 1;
    }

    if(parent < 1 || parent > sb.ninodes || query < 1 || query > sb.ninodes) {
        if(DEBUG)
            printf("iparent: inum # out of range: parent (%d) query (%d) expected (1 - %d)\n", parent, query, sb.ninodes);
        return 1;
    }

    if(!name) {
        if(DEBUG)
            printf("iparent: Name pointer must be valid pointer %d bytes long\n", DIRSIZ);
        return 1;
    }

    if(iget(parent, &pip)){
        if(DEBUG)
            printf("iparent: Parent (%d) could not be retrieved\n", parent);
        return 1;    
    }

    if(pip.type != T_DIR) {
        if(DEBUG)
            printf("iparent: Parent (%d) must be a directory\n", parent);
        return 1;
    }

    for(offset = 0; offset < pip.size; offset += sizeof(struct dirent)){
        memset(&de, 0, sizeof de);
        iread(&pip, (unsigned char *)&de, sizeof de, offset);
        if(de.inum == 0)
            continue;
        if(de.inum == query){
            iget(de.inum, ip);
            memset(name, 0, DIRSIZ);
            strncpy(name, de.name, DIRSIZ);
            if(doff){
                doff->offset = offset;
                doff->sector = bmapr(&pip, offset/BSIZE);
            }
            return 0;
        }
    }
    if(DEBUG)
        printf("iparent: Could not find inode (%d) within parent (%d)\n", query, parent);
    return 1;
}

/** *
 *  BMAPW.
 *  Returns the disk address block for a block number.
 *  If block number is zero, allocate a new block.
 *  Returns 0 on failure.
 *          Disk address block on success.
 */

uint bmapw(struct inode *ip, uint bn) {

    unsigned char buffer[BSIZE];
    uint *a;

    memset(buffer, 0, sizeof buffer);

    if(bn < NDIRECT){

        if(ip->addrs[bn] == 0){
            if(DEBUG)
                printf("bmapw: Allocating new block for %d\n", ip->inum);
            ip->addrs[bn] = blkalloc();
            
            if(ip->type == T_DIR)
                ip->size += BSIZE;
            
            iupdate(ip);
        }

        return ip->addrs[bn];
    }

    bn -= NDIRECT;

    if(bn > NINDIRECT)
        return 0;
    
    if(ip->addrs[NDIRECT] == 0) {
        printf("bmapw: Allocating indirect block for %d\n", ip->inum);
        ip->addrs[NDIRECT] = blkalloc();
        iupdate(ip);
    }

    rsec(ip->addrs[NDIRECT], buffer);

    a = (uint *)(buffer);
    
    if(a[bn] == 0) {
        printf("bmapw: Allocating indirect block pointer for %d\n", ip->inum);
        a[bn] = blkalloc();
        
        if(ip->type == T_DIR)
            ip->size += BSIZE;
        
        wsec(ip->addrs[NDIRECT], buffer);
    }

    return a[bn];
}

/** 
 * BMAPR.
 * Returns a disk address block for a block number.
 * Returns  0 on failure.
 *          Disk address block on success.
*/

uint bmapr(struct inode *ip, uint bn) {


    unsigned char buffer[BSIZE];
    uint *a;

    memset(buffer, 0, sizeof buffer);

    if(bn < NDIRECT){
        return ip->addrs[bn] == 0 ? 0: ip->addrs[bn];
    }

    bn -= NDIRECT;

    if(bn > NINDIRECT)
        return 0;
    
    if(rsec(ip->addrs[NDIRECT], buffer) < 512)
        return 0;
    
    a = (uint *)(buffer);
    
    if(a[bn] == 0)
        return 0;

    return a[bn];
}

/** 
 * iread.
 * Reads the n bytes of data from an inode at a offset.
 * Returns  0 on failure.
 *          Number of bytes read on success.
*/
int iread(struct inode *ip, unsigned char *buf, uint n, uint offset){

    uint total = 0;
    uint m = 0;
    uint off = 0;
    uint bn = 0;

    unsigned char buffer[BSIZE];

    if(!ip){
        if(DEBUG)
            printf("iread: Invalid inode pointer\n");
        return 0;
    }

    if(!buf){
        if(DEBUG)
            printf("iread: Invalid buffer pointer\n");
        return 0;
    }

    if(offset > ip->size){
        if(DEBUG)
            printf("iread: Offset(%d) is greater than ip->size(%d)\n", offset, ip->size);
        return 0;
    }


    if(n + offset > ip->size)
        n = ip->size - offset;
    
    memset(buf, 0, n);

    for(total = 0, m = 0, off = offset; total < n; total += m, off += m){
        bn = bmapr(ip, off/BSIZE);
        memset(buffer, 0, sizeof buffer);
        rsec(bn, buffer);
        m = MIN(n - total, BSIZE - off%BSIZE);
        memmove(buf + total, buffer + off % BSIZE, m);
    }

    return total;

}
/** 
 * iwrite.
 * Writes n bytes of data to an inode at a offset.
 * Returns  0 on failure.
 *          Number of bytes written on success.
*/

int iwrite(struct inode *ip, const unsigned char *buf, uint n, uint offset){

    uint total = 0;
    uint m = 0;
    uint off = 0;
    uint bn = 0;
    unsigned char buffer[BSIZE];


    if(!ip){
        if(DEBUG)
            printf("iwrite: Invalid inode pointer\n");
        return 0;
    }

    if(!buf){
        if(DEBUG)
            printf("iwrite: Invalid buffer pointer\n");
        return 0;
    }

    if(offset > ip->size){
        if(DEBUG)
            printf("iwrite: Offset(%d) is greater than ip->size(%d)\n", offset, ip->size);
        return 0;
    }

    for(total = 0, off = offset, m = 0; total < n; total += m, off += m){
        bn = bmapw(ip, off/BSIZE);
        memset(buffer, 0, sizeof buffer);
        rsec(bn, buffer);
        m = MIN(n - total, BSIZE - off%BSIZE);
        memmove(buffer + off%BSIZE, buf + total, m);
        wsec(bn, buffer);
    }

    if(off > ip->size && total > 0 && ip->type == T_FILE){
        ip->size = off;
        iupdate(ip);
    }

    return total;

}

int iunlink(struct inode *pip, struct inode *ip, struct dirent_offset * doff){

    
    struct dirent de;

    if(!pip){
        if(DEBUG)
            printf("iunlink: Pip must be a valid pointer to Parent Inode\n");
        return 1;
    }

    if(!ip){
        if(DEBUG)
            printf("iunlink: ip must be a valid pointer\n");
        return 1;
    }

    if(!doff){
        if(DEBUG)
            printf("iunlink: doff must be a pointer returned from either iparent or nparent\n");
        return 1;
    }

    memset(&de, 0, sizeof de);

    if(iread(pip, (unsigned char *)&de, sizeof de, doff->offset) != sizeof(struct dirent)){
        if(DEBUG)
            printf("iunlink: iread: Bytes read did not match size of dirent\n");
        return 1;
    }

    if(de.inum != ip->inum){
        if(DEBUG)
            printf("iunlink: dirent (%s)(%d) at offset (%d) does not match inode (%d) supplied\n", de.name, de.inum, doff->offset, ip->inum);
        return 1;
    }

    if(de.inum == 1 || strncmp(de.name, ".", DIRSIZ) == 0 || strncmp(de.name, "..", DIRSIZ) == 0){
        if(DEBUG)
            printf("iunlink: Can not remove . / .. or root directory\n");
        return 1;
    }

    memset(&de, 0, sizeof de);

    if(iwrite(pip, (unsigned char *)&de, sizeof de, doff->offset) != sizeof(struct dirent)){
        if(DEBUG)
            printf("iunlink: iwrite: Bytes written did not match sizeof dirent\n");
        return 1;
    }
    
    pip->nlink -= 1;
    iupdate(pip);

    return 0;

}

/** 
 * dirremove.
 * Recursively delete all objects within the directory
 * regardless of their links.
*/
static int dirempty(struct inode *pip) {

    uint offset = 0;
    struct dirent de;

    for(offset = (sizeof(struct dirent) * 2); offset < pip->size; offset += sizeof(struct dirent)){
        memset(&de, 0, sizeof de);
        iread(pip, (unsigned char *)&de, sizeof de, offset);
        
        if(de.inum != 0)
            return 0;
    }

    return 1;

}


static void _dir_remove(struct inode *ip){
    
    uint offset = 0;
    struct dirent de;
    struct inode rip;


    if(ip->type == T_FILE){
        iremove(ip);
        return;
    }

    if(dirempty(ip)){
        iremove(ip);
        return;
    }

    for(offset = (sizeof(struct dirent) * 2); offset < ip->size; offset += sizeof(struct dirent)){
        memset(&de, 0, sizeof de);
        if(iread(ip, (unsigned char *)&de, sizeof de, offset) != sizeof(struct dirent)){
            printf("iread: error\n");
            return;
        }
        
        if(de.inum == 0)
            continue;

        memset(&rip, 0, sizeof rip);
        iget(de.inum, &rip),
        _dir_remove(&rip);
    }
}

int dirremove(struct inode *pip){
    
    if(!pip)
        return 1;
    
    if(pip->inum == 1){
        return 1;
    }

    if(dirempty(pip)){
        return 0;
    }

    _dir_remove(pip);
    return 0;
}

int dirinit(uint parent, struct inode *dir){

    struct superblock sb;
    struct dirent e;
    uint offset;

    memset(&sb, 0, sizeof sb);
    readsb(&sb);

    if(!dir){
        if(DEBUG)
            printf("dirinit: Dir must be a valid inode pointer\n");
        return 1;
    }

    if(parent > sb.ninodes || parent < 1){
        if(DEBUG)
            printf("ilink: Parent inum # out of range: (%d) expected (1 - %d)\n", parent, sb.ninodes);
        return 1;
    }

    memset(&e, 0, sizeof e);

    e.inum = dir->inum;
    strncpy(e.name, ".", DIRSIZ);
    offset = 0;

    if(iwrite(dir, (unsigned char *)&e, sizeof e, offset) != sizeof(struct dirent)){
        if(DEBUG)
            printf("dirinit: iwrite: Write error of .\n");
        return 1;
    }

    memset(&e, 0, sizeof e);
    e.inum = parent;
    strncpy(e.name, "..", DIRSIZ);

    offset += sizeof(struct dirent);

    if(iwrite(dir, (unsigned char *)&e, sizeof e, offset) != sizeof(struct dirent)){
        if(DEBUG)
            printf("dirinit: iwrite: Write error of .\n");
        return 1;
    }

    return 0;
}