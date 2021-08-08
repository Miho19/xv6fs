#include "xv6usb.h"

#define V_ID 0x058f
#define P_ID 0x6387

#define IN ( 0x82) 
#define OUT (0x01)

#define GET_MAX_LUN 0xFE
#define TIMEOUT 4000

#define DEBUG 0

#define be_to_int32(buf) ( ( (buf)[0] << 24) |  ((buf)[1] << 16)  | ( (buf)[2] << 8) | (buf)[3])

static void perr(char const *format, ...) {

	va_list args;
	va_start (args, format);
	vfprintf(stderr, format, args);
	va_end(args);
};


#define ERR_EXIT(errcode) do {perr("	%s\n", libusb_strerror((enum libusb_error)errcode)); return -1;}while(0)

#define CALL_CHECK(fcall) do {int _r =fcall; if(_r < 0) ERR_EXIT(_r); } while(0)


static int MAX_LUN = 0;

libusb_device_handle *_dev_handle = 0;
libusb_context *_context = NULL;

void send_command(libusb_device_handle *handle, uint8_t endpoint, uint8_t lun, uint8_t *cdb, uint8_t direction, int data_length, uint32_t *ret_tag); 

/** CBW */
struct command_block_wrapper {
	uint8_t dCBWSignature[4];
	uint32_t dCBWTag;
	uint32_t dCBWDataTransferLength;
	uint8_t bmCBWFlags;
	uint8_t bCBWLUN;
	uint8_t bCBWCBLength;
	uint8_t CBWCB[16];

};

/** CSW */
struct command_status_wrapper {
	uint8_t dCSWSignature[4];
	uint32_t dCSWTag;
	uint32_t dCSWDataResidue;
	uint8_t bCSWStatus;
};


/** Each command has a specific length */

static const uint8_t cdb_length[256] = {
//	 0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
	06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,  //  0
	06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,06,  //  1
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  2
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  3
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  4
	10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,  //  5
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  6
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  7
	16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,  //  8
	16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,  //  9
	12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,  //  A
	12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,  //  B
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  C
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  D
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  E
	00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,00,  //  F
};


/** 
	1. Get MAX LUN so we hve destination for CBW.
		LIBUSB_ENDPOINT_IN | CLASS REQUEST | RECIPIENT INTERFACE Using interface 0
	2. Get inquiry data -> 36 bytes
		1. Send inquiry CBW
		2. Get inquiry data from endpoint in
*/


/** Returns the status of the storage device */

static int storage_status(libusb_device_handle *handle, uint8_t endpoint, uint32_t expected_tag) {
	int n = 0;
	int size = 0;
	int result = 0;

	struct command_status_wrapper csw;

	memset(&csw, 0, sizeof csw);

	do {
		result = libusb_bulk_transfer(handle, endpoint, (unsigned char *)&csw, 13, &size, TIMEOUT);
		if(result == LIBUSB_ERROR_PIPE) {
			libusb_clear_halt(handle, endpoint);
		}
		n++;
	}while((result == LIBUSB_ERROR_PIPE) && n < 5);
	
	if(result != LIBUSB_SUCCESS) {
		printf("storage status failed: %d\n", result);
		return -1;
	}

	if(size != 13) {
		printf("storage status recieved %d bytes instead of 13\n", size);
		return -1;
	}

	if(csw.dCSWTag != expected_tag) {
		printf("storage status: mismatch tags expected: %08X recieved: %08X\n", expected_tag, csw.dCSWTag);
		return -1;
	}

	// printf("Storage status: %02X: (%s)\n\tData Residue: %d\n", csw.bCSWStatus, csw.bCSWStatus? "Failure" : "Success", csw.dCSWDataResidue);
	

	if(csw.bCSWStatus) {
		if(csw.bCSWStatus == 1)
			return -2;
		else
			return -3;
	}

	

	return 0;
}


/** 
	Device to host sense data. Error reported by device and host must get sense data.

*/
static void get_sense(libusb_device_handle *handle, uint8_t endpoint_in, uint8_t endpoint_out) {
	uint8_t cdb[16];

	uint8_t sense[18];

	uint32_t expected_tag = 0;
	int size = 0;
	int result = 0;

	printf("Request Sense\n");
	memset(sense, 0, sizeof sense);
	memset(cdb, 0, sizeof cdb);
	cdb[0] = 0x03; /** sig for request sense */
	cdb[4] = 0x12;

	send_command(handle, endpoint_out, 0, cdb, LIBUSB_ENDPOINT_IN, 0x12, &expected_tag);

	result = libusb_bulk_transfer(handle, endpoint_in, (unsigned char*)&sense, 0x12, &size, TIMEOUT);

	if(result < 0) {
		printf("Error getting sense: %d\n", result);
		return;
	}

	printf("Sense size: %d\n", size);

	if((sense[0] != 0x70) && (sense[0] != 0x71)) {
		printf("Not valid sense data\n");
	} else {
		printf("Sense: %02X %02X %02X\n", sense[2]&0x0F, sense[12], sense[13]);
	}

	storage_status(handle, endpoint_in, expected_tag);
}



void send_command(libusb_device_handle *handle, uint8_t endpoint, uint8_t lun, uint8_t *cdb, uint8_t direction, int data_length, uint32_t *ret_tag) {
	static uint32_t tag = 1;

	uint8_t cdb_len = 0;

	struct command_block_wrapper cbw;

	int n = 0;
	int size = 0;
	int result = 0;

	
	if(!cdb)
		return;
	if(endpoint & LIBUSB_ENDPOINT_IN) {
		printf("Can not send command on IN endpoint\n");
		return;
	}

	cdb_len = cdb_length[cdb[0]];
	
	if((cdb_len == 0) || cdb_len > sizeof cbw.CBWCB) {
		printf("Command length too long: %d | %d \n", cdb[0], cdb_len);
		return;
	}

	memset(&cbw, 0, sizeof cbw);
	
	cbw.dCBWSignature[0] = 'U';		/** 0x43425355*/
	cbw.dCBWSignature[1] = 'S';
	cbw.dCBWSignature[2] = 'B';
	cbw.dCBWSignature[3] = 'C';

	*ret_tag = tag;

	cbw.dCBWTag = tag++; /** Need this for CSW */
	cbw.dCBWDataTransferLength = data_length;
	cbw.bmCBWFlags = direction;
	cbw.bCBWLUN = lun;

	cbw.bCBWCBLength = cdb_len;

	memcpy(cbw.CBWCB, cdb, cdb_len);

	do {

		result = libusb_bulk_transfer(handle, endpoint, (unsigned char*)&cbw, 31, &size, TIMEOUT);
		
		if(result == LIBUSB_ERROR_PIPE) {
			libusb_clear_halt(handle, endpoint);
		}
		n++;

	}while((result == LIBUSB_ERROR_PIPE) && n < 5);
	
	if(result != 0) {
		printf("Error in sending CBW: %d\n", result);
		return;
	}

	/** printf("Sent %d CDB Bytes\n", cdb_len);	*/
}


int storage_init(libusb_device_handle *handle) {

	int result = 0;
	uint8_t lun;

	uint8_t cdb[16];  /* Command descriptor block used to cbw to lun*/

	
	uint8_t buffer[64];

	uint32_t expected_tag = 0;
	int size = 0;
	int i;

	char vid[9];
	char pid[9];
	char rev[5];


	uint32_t max_block;
	uint32_t block_size;
	double device_size;
	

	result = libusb_control_transfer(handle, LIBUSB_ENDPOINT_IN | LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE, GET_MAX_LUN, 0, 0,&lun,1, TIMEOUT);

	if(result == 0) {
		lun = 0;
	} else if ( result < 0) {
		printf("Error getting MAX LUN value: %d\n", result);
		return -1;
	}
	
	printf("Max LUN: %d\n", lun);

	MAX_LUN = lun;
	_dev_handle = handle;
	
	memset(buffer, 0, sizeof buffer);
	memset(cdb, 0, sizeof cdb);

	cdb[0] = 0x12;		/** Send inquiry of device */
	cdb[4] = 0x24;		/** Length of inquiry, 36 bytes */

	send_command(handle, OUT, lun, cdb, LIBUSB_ENDPOINT_IN, 0x24, &expected_tag);

	CALL_CHECK(libusb_bulk_transfer(handle, 0x82, (unsigned char *)&buffer, 0x24, &size, TIMEOUT));
	printf("Inquiry: %d bytes\n", size);

	memset(vid, 0, sizeof vid);
	memset(pid, 0, sizeof pid);
	memset(rev, 0, sizeof rev);

	
	for(i=0;i<8;i++) {
		vid[i] = buffer[8+i];
		pid[i] = buffer[16+i];
		rev[i/2] = buffer[32 + i/2];
	}

	vid[8] = 0;
	pid[8] = 0;
	rev[4] = 0;


	printf("\tVID: %8s\n\tPID: %8s\n\tREV: %8s\n", vid, pid, rev);

	if(storage_status(handle, IN, expected_tag) == -2) {
		get_sense(handle, IN, OUT);
	}

	

	memset(buffer, 0, sizeof buffer);
	memset(cdb, 0, sizeof cdb);

	cdb[0] = 0x25;	/** Read capacity */
	expected_tag = 0;
	size = 0;

	send_command(handle, OUT, lun, cdb, LIBUSB_ENDPOINT_IN, 0x08, &expected_tag); 
	

	CALL_CHECK(libusb_bulk_transfer(handle, IN, (unsigned char*)&buffer, 0x08, &size, TIMEOUT));


	max_block = 0;
	block_size = 0;
	device_size = 0.0; /** GB = (bytes) /(1024 * 1024 * 1024) */

	max_block = be_to_int32(&buffer[0]);	
	block_size = be_to_int32(&buffer[4]);
	device_size = ((double)(max_block + 1)) * block_size / (1024*1024*1024); 

	printf("Last block: 0x%08X\nBlock Size: %d Bytes\nSize: %.2f GB\n",max_block, block_size, device_size);

	if(storage_status(handle, IN, expected_tag) == -2) {
		get_sense(handle, IN, OUT);
	}
	
	return 0;
}

int read_sector(int sec, void *buf) {
	int size = 0;
	uint32_t expected_tag = 0;
	uint8_t cdb[16];

	memset(buf, 0, 512);
	memset(cdb, 0, sizeof cdb);
	
	cdb[0] = 0x28; // Read(10)
	cdb[5] = sec; // Address 
	cdb[8] = 0x01;

	send_command(_dev_handle, OUT, MAX_LUN, cdb, LIBUSB_ENDPOINT_IN, 512, &expected_tag);
	
	CALL_CHECK(libusb_bulk_transfer(_dev_handle, IN, buf, 512, &size, TIMEOUT));

	if(storage_status(_dev_handle, IN, expected_tag) == -2) {
		get_sense(_dev_handle, IN, OUT);
	}
	
	if(DEBUG)
		printf("Receieved %d bytes back from read sector\n", size);

	return size;
}


int write_sector(int sec, void *buf) {
	int size = 0;
	uint32_t expected_tag = 0;
	uint8_t cdb[16];

	memset(cdb, 0, sizeof cdb);

	cdb[0] = 0x2A; //Write(10)

	cdb[5] = sec; // Address

	cdb[8] = 0x01; // 1 sector write

	send_command(_dev_handle, OUT, MAX_LUN, cdb, LIBUSB_ENDPOINT_OUT, 512, &expected_tag);

	CALL_CHECK(libusb_bulk_transfer(_dev_handle, OUT, buf, 512, &size, TIMEOUT));

	
	if(storage_status(_dev_handle, IN, expected_tag) == -2) {
		get_sense(_dev_handle, IN, OUT);
	}

	if(DEBUG)
		printf("Written %d bytes to the device\n", size);

	return size;
		

}




void print_device(libusb_device *dev) {
	struct libusb_device_descriptor desc;
		
	struct libusb_config_descriptor *config;
	const struct libusb_interface *inter;
	
	const struct libusb_interface_descriptor *interdesc;
	const struct libusb_endpoint_descriptor *endpointdesc;

	int ret = 0;
	int i = 0;
	int j = 0;
	int k = 0;


	memset(&desc, 0, sizeof desc);

	ret = libusb_get_device_descriptor(dev, &desc);
	if(ret < 0) {
		fprintf(stderr, "error in getting device descriptor\n");
		return;
	}

	
	printf("Number of posibile configs: %d\n", desc.bNumConfigurations);
	printf("Device Class: %d\n", desc.idVendor);
	printf("Product ID: %d\n", desc.idProduct);
	libusb_get_config_descriptor(dev, 0, &config);
	printf("Interfaces: %d\n", config->bNumInterfaces);	
	
	for(i=0;i<config->bNumInterfaces;i++){
		inter = &config->interface[i];
		printf("Interface: %d number of alt settings: %d\n",i,  inter->num_altsetting);
		for(j=0;j<inter->num_altsetting;j++) {
			interdesc = &inter->altsetting[j];
			printf("Interface number: %d, ", interdesc->bInterfaceNumber);
			printf("Num of endpoints: %d\n", interdesc->bNumEndpoints);
			printf("Alternating setting number = %d\n", interdesc->bAlternateSetting);
			for(k=0;k< interdesc->bNumEndpoints;k++) {
				endpointdesc = &interdesc->endpoint[k];
				printf("\tDesc Type: %d\n ", endpointdesc->bDescriptorType);
				printf("\tENDPOINT Address: %d\n", endpointdesc->bEndpointAddress);
				printf("\tMax Packet Size: %d\n", endpointdesc->wMaxPacketSize);
				printf("\tPolling Rate: %d\n", endpointdesc->bInterval);
				printf("\tType: %d\n", endpointdesc->bmAttributes);	
			}
		}
	}

	printf("\n\n");
	libusb_free_config_descriptor(config);
}

int endian_test() {
	unsigned int i = 1;

	char *c = (char *)&i;

	if(*c){
		return 0;
	} else {
		return -1;
	}
}

int usb_init() {
	int result = 0;
			
	libusb_context *context = NULL;
	libusb_device_handle *dev_handle;

	if(endian_test() < 0) {
		printf("Libusb only supports little endian CPUS\n");
		exit(1);
	} 

	result = libusb_init(&context);
	
	if(result < 0) {
		perror("libusb_init");
		return 1;
	}

	_context = context;
		

	dev_handle = libusb_open_device_with_vid_pid(context, V_ID, P_ID);
	
	if(!dev_handle) {
		perror("Could not open device\n");
		usb_close();
		return 1;
	}

	_dev_handle = dev_handle;


	if(libusb_kernel_driver_active(dev_handle, 0) == 1) {
		if(libusb_detach_kernel_driver(dev_handle, 0) == 0) {
		} else {
			usb_close();
			return 1;
		}
		
	}

	result = libusb_claim_interface(dev_handle, 0);
	if(result < 0) {
		printf("Error claiming interface: %d\n", result);
		usb_close();
		return 1;
	}

	result = storage_init(_dev_handle);

	if(result != 0) {
		printf("Error: Could not establish XV6 file system on USB\n");
		usb_close();
		return 1;
	}

	return 0;
}


int usb_close() {
	
	int result = 0;
	
	if(!_dev_handle)
		return 1;

	result = libusb_release_interface(_dev_handle, 0);

	if(result != LIBUSB_SUCCESS) {
		printf("USB CLOSE: %s\n", libusb_strerror((enum libusb_error)result));
	}
	

	result = libusb_attach_kernel_driver(_dev_handle, 0);
	
	if(result != LIBUSB_SUCCESS) {
		printf("USB CLOSE: %s\n", libusb_strerror((enum libusb_error)result));
	}


	libusb_close(_dev_handle);
	libusb_exit(_context);
	
	return 0;	 	
}


