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
        printf("stat: Could not get inode %ld\n", ino);
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


static int reply_buf_limited(fuse_req_t req, const char *buf, size_t bufsize, off_t offset, size_t maxsize){

    
    
    if(offset < (long)(bufsize)) {
        return fuse_reply_buf(req, buf + offset, MIN(bufsize - offset, maxsize));
    } 

    return fuse_reply_buf(req, 0, 0);
}

static void xv6_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi) {
    
    struct dirbuf b;

    struct inode ip;
    struct dirent *d;
    uint index = 0;
    uint offset = 0;
    unsigned char buffer[BSIZE];
    unsigned char indirect[BSIZE];
    uint *a;
    
    (void) fi;

    memset(&b, 0, sizeof b);
    memset(&ip, 0, sizeof ip);
    memset(buffer, 0, sizeof buffer);

    if(iget(ino, &ip, f)){
        fuse_reply_buf(req, 0, 0);
        return;
    }

    for(index = 0;index < NDIRECT;index++){
        if(ip.addrs[index] == 0)
            continue;
        memset(buffer, 0, sizeof buffer);
        rsec(ip.addrs[index], buffer, f);

        for(offset = 0;offset < BSIZE; offset += sizeof(struct dirent)){
            d = (struct dirent *)(buffer + offset % BSIZE);
            if(d->inum == 0)
                continue;
            dirbuf_add(req, &b, d->name, d->inum);
        }
    }

    if(ip.addrs[NDIRECT]) {
        memset(indirect, 0, sizeof indirect);
        rsec(ip.addrs[NDIRECT], indirect, f);
        a = (uint *)indirect;
        for(index = 0; index < BSIZE; index++){
            if(a[index] == 0)
                continue;
            rsec(a[index], buffer, f);
            for(offset = 0; offset < BSIZE; offset += sizeof(struct dirent)){
                d = (struct dirent *)(buffer + offset % BSIZE);
                if(d->inum == 0)
                    continue;
                dirbuf_add(req, &b, d->name, d->inum);
            }
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

    memset(&e, 0, sizeof e);
    memset(&ip, 0, sizeof ip);
    
    if(nparent(parent, name, &ip, f, 0)){
        printf("lookup: Could not retrieve inode from parent %ld\n", parent);
        fuse_reply_err(req, ENOENT);
        return;
    }
    
    if(xv6_stat(ip.inum, &e.attr)){
        printf("lookup: Could not retrive stat inode %d\n", ip.inum);
        fuse_reply_err(req, ENOENT);
        return;
    }

    e.ino = ip.inum;
    e.attr_timeout = 100.0;
    e.entry_timeout = 100.0;

    fuse_reply_entry(req, &e);
    
}

static void xv6_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi){
    
    struct stat stbuf;

    memset(&stbuf, 0, sizeof stbuf);

    xv6_stat(ino, &stbuf);
    
    fi->fh = 0;

    if(fi->flags & O_APPEND){
        fi->fh = stbuf.st_size;
    }

    fuse_reply_open(req, fi);
    
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
        bn = bmapr(&ip, offset/BSIZE, f);
        rsec(bn, buffer, f);
        m = MIN(size - total, BSIZE - offset % BSIZE);
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

    printf("\nWRITE\nino\t:%ld\nsize\t:%ld\noffset\t:%ld\nfi->fh:\t%ld\n\n", ino, size, off, fi->fh);

    if(iget(ino, &ip, f)){
        fuse_reply_write(req, 0);
        return;
    }

    for(total = 0, offset = off; total < size; total += m, offset += m) {
        bn = bmapw(&ip, offset/BSIZE, f);
        rsec(bn, buffer, f);
        m = MIN(size - total, BSIZE - (offset % BSIZE));
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


static void xv6_forget(fuse_req_t req, fuse_ino_t ino, uint64_t nlookup){
    struct inode ip;

    memset(&ip, 0, sizeof ip);

    iget(ino, &ip, f);
    ip.nlink -= nlookup;
    iupdate(&ip, f);
   
    printf("forget:\ninum\t:%ld\nnlink\t:%d\n", ino, ip.nlink);

    if(ip.nlink > 0){
        printf("forget: Link amount %d \n", ip.nlink);
        fuse_reply_none(req);
        return;
    }

    if(ip.inum == 1){
        fuse_reply_none(req);
        return;
    }

    if(ip.type == T_DIR){
        printf("forget: Removing directory currently not implemented\n");
        fuse_reply_none(req);
        return;
    }

    if(iremove(&ip, f)){
        printf("forget: Error removing file using iremove\n");
        fuse_reply_none(req);
        return;
    }

}


static void xv6_unlink(fuse_req_t req, fuse_ino_t parent, const char *name){
    
    struct inode ip;
    struct inode pip;
    struct dirent_offset doff;

    unsigned char buffer[BSIZE];
    struct dirent *d;

    memset(&ip, 0, sizeof ip);
    memset(&pip, 0, sizeof pip);
    memset(&doff, 0, sizeof doff);
    memset(buffer, 0 ,sizeof buffer);

    if(iget(parent, &pip, f)){
        printf("unlink: Error getting parent inode\n");
        fuse_reply_err(req, ENOENT);
        return;
    }

    if(nparent(parent, name, &ip, f, &doff)){
        printf("unlink: Error getting %s inode through parent directory %ld\n", name, parent);
        fuse_reply_err(req, ENOENT);
        return;
    }

    if(ip.nlink > 1){
        printf("unlink: %s still has %d links\n", name, ip.nlink);
        fuse_reply_err(req, 0);
        return;
    }

    rsec(doff.sector, buffer, f);
    d = (struct dirent *)(buffer + doff.offset % BSIZE);

    if(d->inum != ip.inum){
        printf("unlink: inum %d from directory does not match nparent inum %d\n", d->inum, ip.inum);
        fuse_reply_err(req, 0);
        return;
    }

    memset(d, 0, sizeof(struct dirent));
    wsec(doff.sector, buffer, f);

    pip.nlink -= 1;
    pip.size -= sizeof(struct dirent);
    iupdate(&pip, f);

    fuse_reply_err(req, 0);
}



static const struct fuse_lowlevel_ops opers = {
    .getattr    = xv6_getattr,
    .readdir    = xv6_readdir,
    .lookup     = xv6_lookup,
    .open       = xv6_open,
    .read       = xv6_read,
    .write      = xv6_write,
    .create     = xv6_create,
    .forget     = xv6_forget,
    .unlink     = xv6_unlink,
};

int main(int argc, char **argv) {
    int result = 0;

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct fuse_session *se = 0;
    struct fuse_cmdline_opts opts;
    struct fuse_loop_config config;

    memset(&opts, 0, sizeof opts);
    memset(&config, 0, sizeof config);
    

    f = fopen("fs.img", "rb+");        

    if(!f) {
        printf("Could not open fs.img\n");
        return 1;
    }

    superblock_read(f);
    printf("Press any key to continue...\n");
    
    while(1) {
        if(getc(stdin))
            break;
    }
    

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
