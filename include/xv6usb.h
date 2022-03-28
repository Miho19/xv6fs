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
#include <stdint.h>

/** 
 * Replace these before running.
 * 
*/
#define V_ID 0x058f // vendor ID
#define P_ID 0x6387 // product ID

int read_sector(uint32_t sec, void *buf);

int write_sector(uint32_t sec, void *buf);

int usb_init();

int usb_close();


#endif
