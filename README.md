# xv6FS
***
Project designed to maniuplate the [xv6 File system](http://pekopeko11.sakura.ne.jp/unix_v6/xv6-book/en/File_system.html) using a [FUSE interface](https://en.wikipedia.org/wiki/Filesystem_in_Userspace).
***
## Example Usage:
```
cd xv6fs
make
mkdir mnt
cp ~xv6/source/fs.img ./fs.img
./xv6fs -d -s -f mnt
vim mnt/README
```

## Task List:
- [x] Read and write files.
- [x] Create files.
- [x] Remove files.
- [x] Create directories.
- [x] Remove directories.
- [x] Mount fs.img.
- [ ] Mount USB mass storage device.
- [ ] Output log messages to a text file.
- [x] Unit tests.
- [ ] Implement FLUSH / fsync function. 
- [x] Documentation.
 
## Issues:

## Change list:
* Removed the `FILE *f` pointer in place for global structure containing the read/write sector functions.
* Moved FUSE interface code into a seperate file to allow the merging of libusb code.
* More unit tests added. Tests for libfuse will likely require scripting.
* Added functions which should remove a lot of the repeated code issue I was having.
* Removed global variables which will allow me to better make unit tests.