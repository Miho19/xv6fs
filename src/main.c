#define FUSE_USE_VERSION 34 

#include <fuse_lowlevel.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include <unistd.h>
#include "fs.h"

#define PATH_TO_FS "/home/josh/Desktop/fuse_test/fs.img"
FILE *f = 0;


static int test_stat(fuse_ino_t ino, struct stat *stbuf) {
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

static void test_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi){
    
    struct stat stbuf;
    int result = 0;

    (void) fi;
    memset(&stbuf, 0, sizeof stbuf);

    result = test_stat(ino, &stbuf);

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

static void test_readdir(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi) {
    
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

static void test_lookup(fuse_req_t req, fuse_ino_t parent, const char *name) {
    
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

    if(test_stat(query, &e.attr)){
        printf("Query: %ld. Could not retrive stat\n", query);
        fuse_reply_err(req, ENOENT);
        return;
    }

    e.ino = query;
    e.attr_timeout = 100.0;
    e.entry_timeout = 100.0;

    fuse_reply_entry(req, &e);
    
}

static void test_open(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi){
    
    struct stat stbuf;
    
    printf("\n\t%ld: Opening file\n", ino);

    memset(&stbuf, 0, sizeof stbuf);

    test_stat(ino, &stbuf);
    
    fi->fh = 0;

    if(fi->flags & O_APPEND){
        printf("\n\tAPPEND MODE: %ld\n", stbuf.st_size);
        fi->fh = stbuf.st_size;
    }

    fuse_reply_open(req, fi);
    
}

static uint bmap(struct inode *ip, uint bn) {

    unsigned char buffer[BSIZE];
    uint *a;

    memset(buffer, 0, sizeof buffer);

    if(bn < NDIRECT){
        if(ip->addrs[bn] == 0)
            return 0;
        return ip->addrs[bn];
    }

    bn -= NDIRECT;

    if(bn > NINDIRECT)
        return 0;
    
    if(rsec(ip->addrs[NDIRECT], buffer, f) < 512)
        return 0;
    
    a = (uint *)(buffer);
    
    if(a[bn] == 0)
        return 0;
    
    return a[bn];

}

static void test_read(fuse_req_t req, fuse_ino_t ino, size_t size, off_t off, struct fuse_file_info *fi) {
    
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
        bn = bmap(&ip, offset/BSIZE);
        rsec(bn, buffer, f);
        m = min(size - total, BSIZE - offset % BSIZE);
        b.size += m;
        b.p = realloc(b.p, b.size);
        memmove(b.p + total, buffer + (offset % BSIZE), m);
    }
    
    
    reply_buf_limited(req, b.p, b.size, off, size);
    free(b.p);


}
/** offset: 1929 */

static void test_write(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, struct fuse_file_info *fi) {
    
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
        bn = bmap(&ip, offset/BSIZE);
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


static const struct fuse_lowlevel_ops opers = {
    .getattr    = test_getattr,
    .readdir    = test_readdir,
    .lookup     = test_lookup,
    .open       = test_open,
    .read       = test_read,
    .write      = test_write,
};

int main(int argc, char **argv) {
    int result = 0;

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct fuse_session *se = 0;
    struct fuse_cmdline_opts opts;
    struct fuse_loop_config config;

    memset(&opts, 0, sizeof opts);
    memset(&config, 0, sizeof config);
   


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
    


    f = fopen(PATH_TO_FS, "rb+");        // Open the file for reading/writing -> returns null if cant find it

    if(!f) {
        printf("Could not open fs.img\n");
        goto err_4;
    }

    superblock_init(f);

    printf("Press any key to continue...\n");
    
    while(1) {
        if(getc(stdin))
            break;
    }

    if(opts.singlethread) {
        result = fuse_session_loop(se);
    } else {
        config.clone_fd = opts.clone_fd;
        config.max_idle_threads = opts.max_idle_threads;
        result = fuse_session_loop_mt(se, &config);
    }

    fclose(f);

    err_4:
        fuse_session_unmount(se);
    err_3:
        fuse_remove_signal_handlers(se);
    err_2:
        fuse_session_destroy(se);
    err_1:
        free(opts.mountpoint);
        fuse_opt_free_args(&args);

    printf("\nEXIT CODE: %d\n", result);
    return result ? 1 : 0;
}
