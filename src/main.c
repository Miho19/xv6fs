#define FUSE_USE_VERSION 34 

#include <fuse_lowlevel.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>


#include "fs.h"

FILE *f = 0;

/** 
    TIMEOUTS ARE DOUBLES IN SECONDS
    0755 default dir permissions
    0444 default file permissions
*/

static int test_stat(fuse_ino_t ino, struct stat *stbuf) {
    stbuf->st_ino = ino;
    struct inode ip;

    memset(&ip, 0, sizeof ip);

    iget(ino, &ip, f);
    
    if(stbuf->st_ino == 1) {
        stbuf->st_mode = S_IFDIR| 0755;
        stbuf->st_nlink = 2;
        return 0;
    }




    stbuf->st_mode = S_IFREG | 0444;
    stbuf->st_nlink = 1;
    return 0;
    
}

static void test_getattr(fuse_req_t req, fuse_ino_t ino, struct fuse_file_info *fi){
    
    struct stat stbuf;
    int result = 0;
    
    printf("\n\tAttempting to GETATTR\n");
    
    (void) fi;
    memset(&stbuf, 0, sizeof stbuf);

    result = test_stat(ino, &stbuf);

    if(result) {
        fuse_reply_err(req, ENOENT);
    } else {
        fuse_reply_attr(req, &stbuf, 1.0);
    }

}



/*

.lookup		= hello_ll_lookup,
	.getattr	= hello_ll_getattr,
	.readdir	= hello_ll_readdir,
	.open		= hello_ll_open,
	.read		= hello_ll_read,

*/


static const struct fuse_lowlevel_ops opers = {
    .getattr    = test_getattr, 
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


    f = fopen("/home/josh/Desktop/fuse-test/fs.img", "rb+");        // Open the file for reading/writing -> returns null if cant find it

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