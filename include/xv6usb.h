#ifndef XV6_USB_H_
#define XV6_USB_H_

#include <libusb-1.0/libusb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <stddef.h>
#include <errno.h>


int read_sector(uint32_t sec, void *buf);

int write_sector(uint32_t sec, void *buf);

int usb_init();

int usb_close();


#endif
