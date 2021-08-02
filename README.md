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
- [ ] Create directories.
- [ ] Remove directories.
- [x] Mount fs.img.
- [ ] Mount USB mass storage device.
- [ ] Output log messages to a text file.
- [ ] Unit tests.
- [ ] Implement FLUSH / fsync function. 
- [x] Documentation.
 
## Issues:
* The code in `xv6_readdir`smells bad because this is repeated code in `nparent` and `iparent`.

## Change list:
* Added functions which should remove a lot of the repeated code issue I was having.
* Removed global variables which will allow me to better make unit tests.