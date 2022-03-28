# xv6FS

Allows a user to manipulate the xv6 file system [xv6 File system](http://pekopeko11.sakura.ne.jp/unix_v6/xv6-book/en/File_system.html) on a linux based system
using the [FUSE interface](https://en.wikipedia.org/wiki/Filesystem_in_Userspace).

### Before Using

You need to edit `xv6fs/include/xv6usb.h` to use your mass storage devices vendor and product ID.

```bash
usb-devices
T:...
D:...
P: Vendor=058f ProID=6387 ...
S: Product=Mass Storage
...
```

Copy the vendor and ProID and edit `xv6fs/include/xv6usb.h`
Replace your information for the following fields.

```
#define V_ID 0x058f // vendor ID
#define P_ID 0x6387 // product ID
```

### Usage

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
