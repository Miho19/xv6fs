#include "xv6fuse.h"



static int xv6_stat(fuse_ino_t ino, struct stat *stbuf) {
    stbuf->st_ino = ino;
    struct inode ip;

    memset(&ip, 0, sizeof ip);

    if(iget(ino, &ip)) {
        printf("stat: Could not get inode %lld\n", ino);
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
    struct dirent de;
    
    uint offset = 0;
    
    
    (void) fi;

    memset(&b, 0, sizeof b);
    memset(&ip, 0, sizeof ip);
    

    if(iget(ino, &ip)){
        fuse_reply_buf(req, 0, 0);
        return;
    }

    if(ip.type != T_DIR){
        fuse_reply_buf(req, 0, 0);
        return;
    }

    for(offset = 0; offset < ip.size; offset += sizeof(struct dirent)){
        memset(&de, 0, sizeof de);
        iread(&ip, (unsigned char *)&de, sizeof de, offset);
        if(de.inum == 0)
            continue;
        dirbuf_add(req, &b, de.name, de.inum);
    }    

    reply_buf_limited(req, b.p, b.size, off, size);
    free(b.p);
    
}


static void xv6_lookup(fuse_req_t req, fuse_ino_t parent, const char *name) {
    
    struct fuse_entry_param e;
    struct inode ip;

    memset(&e, 0, sizeof e);
    memset(&ip, 0, sizeof ip);
    
    if(nparent(parent, name, &ip, 0)){
        printf("lookup: Could not retrieve inode from parent %lld\n", parent);
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
    int result = 0;
    
    (void) fi;
   
    memset(&ip, 0, sizeof ip);
    memset(&b, 0, sizeof b);


    if(iget(ino, &ip)) {
        printf("read: iget: Error obtaining inode(%lld)\n", ino);
        fuse_reply_err(req, EACCES);
        return;
    }

    b.p = malloc(size);
    

    result = iread(&ip, (unsigned char *)b.p, size, off);
    b.size = result;

    if(result != (int)size){
        printf("read: Bytes read (%d) does not match size (%d)\n", result , size);
    }
    
    reply_buf_limited(req, b.p, b.size, off, size);
    free(b.p);


}

static void xv6_write(fuse_req_t req, fuse_ino_t ino, const char *buf, size_t size, off_t off, struct fuse_file_info *fi) {
    
    
    int result = 0;

    struct inode ip;
    (void)fi;
    memset(&ip, 0, sizeof ip);

    if(iget(ino, &ip)){
        printf("write: iget: Could not retrieve inode (%lld)\n", ino);
        fuse_reply_write(req, 0);
        return;
    }

    result = iwrite(&ip, (unsigned char *)buf, size, off);

    if(result != (int)size){
        printf("write: Bytes written (%d) does not match size (%d)\n", result, size);
    }

    fuse_reply_write(req, result);
    
}


static void xv6_rmdir(fuse_req_t req, fuse_ino_t parent, const char *name) {
    struct inode pip;
    struct inode ip;
    struct dirent_offset doff;
    
    memset(&ip, 0, sizeof ip);
    memset(&pip, 0, sizeof pip);
    memset(&doff, 0, sizeof doff);

    if(nparent(parent, name, &ip, &doff)){
        printf("rmdir: Could not get (%s) within parent(%lld)\n", name, parent);
        fuse_reply_err(req, ENONET);
        return;
    }

    if(iget(parent, &pip)){
        printf("rmdir: Could not get parent (%lld) inode\n", parent);
        fuse_reply_err(req, ENONET);
        return;
    }

    if(ip.nlink > 1){
        printf("rmdir: (%s) still has %d links\n", name, ip.nlink);
        fuse_reply_err(req, EACCES);
        return;
    }


    if(iunlink(&pip, &ip, &doff)){
        printf("rmdir: Could not remove inode(%s)(%d) from parent(%lld)\n", name, ip.inum, parent);
        fuse_reply_err(req, ENONET);
        return;
    }

    fuse_reply_err(req, 0);
}


static void xv6_mkdir(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode) {
    struct fuse_entry_param e;
    struct inode ip;


    (void) mode;

    memset(&ip, 0, sizeof ip);


    if(ialloc(&ip, T_DIR)){
        printf("ialloc: Error making directory(%s)\n", name);
        fuse_reply_err(req, EACCES);
        return;
    }
    
    if(ilink(parent, name, &ip)){
        printf("iparent: Error adding (%s) to parent (%lld)\n", name, parent);
        iremove(&ip);
        fuse_reply_err(req, ENOENT);
        return;
    }

    if(dirinit(parent, &ip)){
        printf("dirinit: Error initializing new directory (%s)\n", name);
        fuse_reply_err(req, ENOENT);
        return;
    }


    memset(&e, 0, sizeof e);
    e.ino           = ip.inum;
    e.attr_timeout  = 100.0;
    e.entry_timeout = 100.0;
    xv6_stat(ip.inum, &e.attr);
    fuse_reply_entry(req, &e);

}

static void xv6_create(fuse_req_t req, fuse_ino_t parent, const char *name, mode_t mode, struct fuse_file_info *fi){
    
    struct fuse_entry_param e;
    struct inode ip;

    memset(&ip, 0, sizeof ip);
    
    if((mode & S_IFMT) != S_IFREG) {
        fuse_reply_err(req, EACCES);
        return;
    }    

    if(ialloc(&ip, T_FILE)){
        printf("ialloc: Error creating file %s\n", name);
        fuse_reply_err(req, ENOENT);
        return;
    }

    if(ilink(parent, name, &ip)) {
        printf("ilink: Error adding file to parent %lld\n", parent);
        iremove(&ip);
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

    iget(ino, &ip);
    ip.nlink -= nlookup;
    iupdate(&ip);
   
    printf("forget:\ninum\t:%lld\nnlink\t:%d\n", ino, ip.nlink);

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
        dirremove(&ip);
    }

    if(iremove(&ip)){
        printf("forget: Error removing file using iremove\n");
        fuse_reply_none(req);
        return;
    }

}


static void xv6_unlink(fuse_req_t req, fuse_ino_t parent, const char *name){
    
    struct inode ip;
    struct inode pip;
    struct dirent_offset doff;

    memset(&ip, 0, sizeof ip);
    memset(&pip, 0, sizeof pip);
    memset(&doff, 0, sizeof doff);
   

    if(iget(parent, &pip)){
        printf("unlink: Error getting parent inode\n");
        fuse_reply_err(req, ENOENT);
        return;
    }

    if(nparent(parent, name, &ip, &doff)){
        printf("unlink: Error getting %s inode through parent directory %lld\n", name, parent);
        fuse_reply_err(req, ENOENT);
        return;
    }

    if(ip.nlink > 1){
        printf("unlink: %s still has %d links\n", name, ip.nlink);
        fuse_reply_err(req, 0);
        return;
    }

    if(iunlink(&pip, &ip, &doff)){
        printf("unlink: Error removing (%s) from parent (%lld)\n", name, parent);
        fuse_reply_err(req, EACCES);
        return;
    }

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
    .mkdir      = xv6_mkdir,
    .rmdir      = xv6_rmdir,
};

int xv6_fuse_run(int argc, char **argv){
    
    int result = 0;

    struct fuse_args args = FUSE_ARGS_INIT(argc, argv);
    struct fuse_session *se = 0;
    struct fuse_cmdline_opts opts;
    struct fuse_loop_config config;

    memset(&opts, 0, sizeof opts);
    memset(&config, 0, sizeof config);
    

    if(fuse_parse_cmdline(&args, &opts) != 0) {
        return 1;
    }

    if(opts.show_help) {
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
        printf("Failed to setup signal handlers\n");
        goto err_2;
    }

    if(fuse_session_mount(se, opts.mountpoint)) {
        printf("Failed to mount\n");
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

    return result ? 1 : 0;

}