# xv6FS

Allows a user to manipulate the xv6 file system [xv6 File system](http://pekopeko11.sakura.ne.jp/unix_v6/xv6-book/en/File_system.html) on a linux based system
using the [FUSE interface](https://en.wikipedia.org/wiki/Filesystem_in_Userspace).

### Usage:

```
cd xv6fs
make run
```

### Requirements

- [fuse3](https://packages.ubuntu.com/focal/fuse3)
- [libusb-1-0](https://libusb.info/)
- [check](https://libcheck.github.io/check/web/install.html)
- USB flash drive formatted with the xv6 file system. _will default to using the fs.img if no USB device is present_

### Supported Operations

- [x] Read and write files.
- [x] Create files.
- [x] Remove files.
- [x] Create directories.
- [x] Remove directories.
- [x] Mount fs.img.
- [x] Mount USB mass storage device.

###
