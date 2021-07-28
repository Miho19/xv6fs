#define FUSE_USE_VERSION 34 

#include <fuse_lowlevel.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>



#include "fs.h"

FILE *f = 0;


static int xv6_stat(fuse_ino_t ino, struct stat *stbuf) {
    stbuf->st_ino = ino;
    struct inode ip;

    memset(&ip, 0, sizeof ip);

    if(iget(ino, &ip, f)) {
        printf("Inode error\n");
        return 1;
    }


    if(ip.type == T_DIR) {
        stbuf->st_mode = S_IFDIR | 00775;
    } else {
        stbuf->st_mode = S_IFREG | 00775;
    }

    stbuf->st_nlink     = ip.nlink;
    stbuf->st_size      = ip.size;
    stbuf->st_ino       = ip.inum;
        
    return 0;
    
}

static void xv6_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi){
    
    struct stat stbuf;
    int result = 0;

    (void) fi;
    memset(&stbuf, 0, sizeof stbuf);

    result = xv6_stat(ino, &stbuf);

    if(result) {
        fuse_reply_err(req, ENOENT);
    } else {
        fuse_reply_attr(req, &stbuf, 1.0);
    }

}

struct dirbuf {
    char *p;
    size_t size;
};



static void dirbuf_add(fuse_req_t req, struct dirbuf *b, const char *name, fuse_ino_t ino) {
    struct stat stbuf;
    size_t old_size = 0;

    memset(&stbuf, 0, sizeof stbuf);
    stbuf.st_ino = ino;


    old_size = b->size;
    b->size += fuse_add_direntry(req, 0, 0, name, 0, 0);
    b->p = realloc(b->p, b->size);

    fuse_add_direntry(req, b->p + old_size, b->size - old_size, name, &stbuf, b->size);

}

#define min(x, y) ( (x) < (y) ? (x) : (y))
static int reply_buf_limited(fuse_req_t req, const char *buf, size_t bufsize, off_t offset, size_t maxsize){

    
    
    if(offset < (long)(bufsize)) {
        return fuse_reply_buf(req, buf + offset, min(bufsize - offset, maxsize));
    } 

    return fuse_reply_buf(req, 0, 0);
}

static void xv6_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi) {
    
    struct dirbuf b;

    struct inode ip;
    struct dirent *d;
    int index;
    int offset;
    unsigned char buffer[BSIZE];
    
    (void) fi;

    memset(&b, 0, sizeof b);
    memset(&ip, 0, sizeof ip);
    memset(buffer, 0, sizeof buffer);

    if(iget(ino, &ip, f)){
        fuse_reply_buf(req, 0, 0);
        return;
    }

    for(index = 0; ip.addrs[index] != 0 && index < NDIRECT;index++) {
        offset = 0;
        rsec(ip.addrs[index], buffer, f);
        d = (struct dirent *)(buffer);
        while(1) {
            if(d->inum == 0)
                break;
            dirbuf_add(req, &b, d->name, d->inum);
            offset += sizeof(struct dirent);
            d = (struct dirent *)(buffer + offset);
        }

    }

    reply_buf_limited(req, b.p, b.size, off, size);
    free(b.p);
    
}

/** 
 *  Wants stat attributes in e.attrs;
 * 
 * 
*/

static void xv6_lookup(fuse_req_t req, fuse_ino_t parent, const char *name) {
    
    struct fuse_entry_param e;
    struct inode ip;

    fuse_ino_t query = 0;

    struct dirent *d;
    int index;
    int offset;
    unsigned char buffer[BSIZE];

    memset(&e, 0, sizeof e);
    memset(&ip, 0, sizeof ip);
    


    if(iget(parent, &ip, f)){
        printf("Parent: %ld is not a valid inode\n", parent);
        fuse_reply_err(req, ENOENT);
        return;
    }

    if(ip.type != T_DIR){
        printf("Parent: %ld is not a directory\n", parent);
        fuse_reply_err(req, ENOENT);
        return;
    }

    
    for(index = 0; ip.addrs[index] != 0 && index < NDIRECT;index++){
        offset = 0;
        memset(buffer, 0, sizeof buffer);
        rsec(ip.addrs[index], buffer, f);
        d = (struct dirent *)(buffer);

        while(1) {
            if(!d->inum)
                break;
            if(strcmp(d->name, name) == 0){
                query = d->inum;
                break;
            }
            offset += sizeof(struct dirent);
            d = (struct dirent*)(buffer + offset);
        }

        if(query)
            break;
    }

    if(!query){
        printf("Could not locate %s within parent: %ld\n", name, parent);
        fuse_reply_err(req, ENOENT);
        return;
    }

    if(xv6_stat(query, &e.attr)){
        printf("Query: %ld. Could not retrive stat\n", query);
        fuse_reply_err(req, ENOENT);
        return;
    }

    e.ino = query;
    e.attr_timeout = 100.0;
    e.entry_timeout = 100.0;

    fuse_reply_entry(req, &e);
    
}

static void xv6_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi){
    
    struct stat stbuf;
    
    printf("\n\t%ld: Opening file\n", ino);

    memset(&stbuf, 0, sizeof stbuf);

    xv6_stat(ino, &stbuf);
    
    fi->fh = 0;

    if(fi->flags & O_APPEND){
        printf("\n\tAPPEND MODE: %ld\n", stbuf.st_size);
        fi->fh = stbuf.st_size;
    }

    fuse_reply_open(req, fi);
    
}

static uint bmap(struct inode *ip, uint bn, int mode) {

    unsigned char buffer[BSIZE];
    uint *a;
    uint new_block = 0;

    memset(buffer, 0, sizeof buffer);

    if(bn < NDIRECT){
        if(ip->addrs[bn] == 0){
            if(mode == READ_MODE)
                return 0;

            new_block = blkalloc(f);
            if(!new_block){
                printf("Error creating new block for writing\n");
                return 0;
            }
            ip->addrs[bn] = new_block;
            iupdate(ip, f);
        }
        return ip->addrs[bn];
    }

    bn -= NDIRECT;

    if(bn > NINDIRECT)
        return 0;
    
    if(rsec(ip->addrs[NDIRECT], buffer, f) < 512)
        return 0;
    
    a = (uint *)(buffer);
    
    if(a[bn] == 0) {
        if(mode == READ_MODE)
            return 0;
        new_block = blkalloc(f);

        if(!new_block){
            printf("Error allocating new block within indirect block for writing\n");
            return 0;
        }

        a[bn] = new_block;
        wsec(ip->addrs[NDIRECT], buffer, f);
    }
    
    return a[bn];

}

static void xv6_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi) {
    
    struct inode ip;
    struct dirbuf b;

    uint bn = 0;
    uint m = 0;
    uint total = 0;
    uint offset = 0;

    
    unsigned char buffer[BSIZE];
    
    (void) fi;
    memset(buffer, 0, sizeof buffer);
    memset(&ip, 0, sizeof ip);
    memset(&b, 0, sizeof b);


    if(iget(ino, &ip, f)) {
        printf("Error obtaining inode: %ld in read\n", ino);
        fuse_reply_err(req, EACCES);
        return;
    }


    for(total = 0, offset = off; total < size; total += m, offset += m) {
        memset(buffer, 0, sizeof buffer);
        bn = bmap(&ip, offset/BSIZE, READ_MODE);
        if(!bn){
            printf("Ending read early\n");
            break;
        }
        rsec(bn, buffer, f);
        m = min(size - total, BSIZE - offset % BSIZE);
        b.size += m;
        b.p = realloc(b.p, b.size);
        memmove(b.p + total, buffer + (offset % BSIZE), m);
    }
    
    
    reply_buf_limited(req, b.p, b.size, off, size);
    free(b.p);


}

static void xv6_write(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, struct fuse_file_info *fi) {
    
    size_t offset = off;
    size_t total = 0;
    uint bn = 0;
    uint m = 0;

    struct inode ip;

    unsigned char buffer[BSIZE];

    memset(&ip, 0, sizeof ip);

    printf("\nWRITE\nino:\t%ld\nsize:\t%ld\noffset:\t%ld\nfi->fh:\t%ld\n", ino, size, off, fi->fh);

    if(iget(ino, &ip, f)){
        fuse_reply_write(req, 0);
        return;
    }

    for(total = 0, offset = off; total < size; total += m, offset += m) {
        bn = bmap(&ip, offset/BSIZE, WRITE_MODE);
        if(!bn){
            printf("Writing error\n");
            break;
        }
        rsec(bn, buffer, f);
        m = min(size - total, BSIZE - (offset % BSIZE));
        memmove(buffer + (offset % BSIZE), buf + total, m);
        wsec(bn, buffer, f);
    }

    if(total > 0 && offset > ip.size) {
        ip.size = offset;
        iupdate(&ip, f);
    }



    fuse_reply_write(req, total);
    
}

static void xv6_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi){
    
    struct fuse_entry_param e;
    struct inode ip;
    short type = T_FILE;

    

    memset(&ip, 0, sizeof ip);
    
    if((mode & S_IFMT) == S_IFDIR) {
        type = T_DIR;
    }

    printf("Creating\t%s\nParent\t%ld\ntype\t%s\n\n", name, parent, type == T_FILE ? "File" : "Directory");
    

    if(ialloc(&ip, type, f)){
        printf("ialloc: Error creating file %s\n", name);
        fuse_reply_err(req, ENOENT);
        return;
    }

    

    if(ilink(parent, name, &ip, f)) {
        printf("ilink: Error adding file to parent %ld\n", parent);
        iremove(&ip, f);
        fuse_reply_err(req, ENOENT);
        return;
    }

    

    memset(&e, 0, sizeof e);
    e.attr_timeout      = 100.0;
    e.entry_timeout     = 100.0;
    e.ino               = ip.inum;
    xv6_stat(e.ino, &e.attr);

    fuse_reply_create(req, &e, fi);

}



static const struct fuse_lowlevel_ops opers = {
    .getattr    = xv6_getattr,
    .readdir    = xv6_readdir,
    .lookup     = xv6_lookup,
    .open       = xv6_open,
    .read       = xv6_read,
    .write      = xv6_write,
    .create     = xv6_create,
};

int main(int argc, char **argv) {
    int result = 0;

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct fuse_session *se = 0;
    struct fuse_cmdline_opts opts;
    struct fuse_loop_config config;

    struct inode ip;

    memset(&opts, 0, sizeof opts);
    memset(&config, 0, sizeof config);

    f = fopen("fs.img", "rb+");        

    if(!f) {
        printf("Could not open fs.img\n");
        return 1;
    }

    superblock_init(f);
    memset(&ip, 0, sizeof ip);
    iget(1, &ip, f);
    ip.type = 1;
    iupdate(&ip, f);

    if(fuse_parse_cmdline(&args, &opts) != 0)
        return 1;
    
    if(opts.show_help) {
        printf("Help message\n");
        fuse_cmdline_help();
        fuse_lowlevel_help();
        result = 0;
        goto err_1;
    }

    if(opts.show_version) {
        printf("Version %s\n", fuse_pkgversion());
        fuse_lowlevel_version();
        result = 0;
        goto err_1;
    }

    if(!opts.mountpoint) {
        printf("No mount point\n");
        result = 1;
        goto err_1;
    }

    se = fuse_session_new(&args, &opers, sizeof opers, NULL);

    if(!se) {
        goto err_1;
    }

    if(fuse_set_signal_handlers(se)) {
        printf("Faile to setup signal handlers\n");
        goto err_2;
    }

    if(fuse_session_mount(se, opts.mountpoint)) {
        goto err_3;
    }
    
    fuse_daemonize(opts.foreground);


    if(opts.singlethread) {
        result = fuse_session_loop(se);
    } else {
        config.clone_fd = opts.clone_fd;
        config.max_idle_threads = opts.max_idle_threads;
        result = fuse_session_loop_mt(se, &config);
    }

    
    fuse_session_unmount(se);
    err_3:
        fuse_remove_signal_handlers(se);
    err_2:
        fuse_session_destroy(se);
    err_1:
        free(opts.mountpoint);
        fuse_opt_free_args(&args);
        fclose(f);

    printf("\nEXIT CODE: %d\n", result);
    return result ? 1 : 0;
}
