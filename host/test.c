#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <unistd.h>
#include <libusb-1.0/libusb.h>

#include "ztex.h"
#include "inouttraffic.h"
#include "ztex_scan.h"
#include "pkt_comm/pkt_comm.h"
#include "device.h"

const int BUF_SIZE_MAX = 65536;


volatile int signal_received = 0;

void signal_handler(int signum)
{
	signal_received = 1;
}

void set_random()
{
	struct timeval tv0;
	gettimeofday(&tv0, NULL);
	srandom(tv0.tv_usec);
}

uint64_t buf_set(unsigned char *buf, int len, uint64_t data)
{
	int i;
	for (i = 0; i < len; i++) {
		buf[i] = data & 0xff; //buf[i+1] = (data >> 8) & 0xff;
		//buf[i+2] = (data >> 16) & 0xff; buf[i+3] = (data >> 24) & 0xff;
		//buf[i+4] = (data >> 32) & 0xff; buf[i+5] = (data >> 40) & 0xff;
		//buf[i+6] = (data >> 48) & 0xff; buf[i+7] = (data >> 56) & 0xff;
		data++;
	}
	return data;
}

int buf_check(unsigned char *buf, int len, uint64_t *data)
{
	int i;
	for (i = 0; i < len; i++) {
		//printf("%d ", data & 0xff);
		
		/*unsigned long tmp0 = buf[i] | (buf[i+1] << 8) | (buf[i+2] << 16) | (buf[i+3] << 24);
		unsigned long long tmp1 = buf[i+4] | (buf[i+5] << 8) | (buf[i+6] << 16) | (buf[i+7] << 24);
		tmp1 <<= 32;
		tmp1 |= tmp0 & 0xffffffff;
		*/
		(*data) &= 0xff;
		unsigned char tmp1 = buf[i];
		if (tmp1 != (*data)) {
			//fprintf(stderr, "len:%d i:%d Bad data: %016llx, must be: %016llx\n",
			//	len,i,tmp1, (unsigned long long)data);
			fprintf(stderr, "len:%d i:%d Bad data: %d, must be: %d\n",
				len,i,tmp1, (*data));
			int j;
			for (j = i-10; j < i+10; j++) {
				fprintf(stderr, "%d ", buf[j] & 0xff);
				if (i==j) fprintf(stderr, "- ");
			}
			fprintf(stderr, "\n");
			return -1;
		}
		(*data)++;
	}
	return 0;
}


///////////////////////////////////////////////////////////////////////////////////
//
// Traverse device list, perform FPGA Initialization
// specific for test.c
//
// For most cases, it would be enough to call device_init_scan() / device_timely_scan()
// that would call device_list_init() 
//
int device_unique_id = 0;

int device_list_init_fpgas_1(struct device_list *device_list)
{
	int ok_count = 0;
	struct device *device;
	for (device = device_list->device; device; device = device->next) {
		if (!device_valid(device))
			continue;

		int i;
		for (i = 0; i < device->num_of_fpgas; i++) {
			struct fpga *fpga = &device->fpga[i];

			int result = fpga_select(fpga);
			if (result < 0) {
				device_invalidate(device);
				return result;
			}

			// These fields are used only in test.c
			fpga->wr.buf = malloc(BUF_SIZE_MAX);
			fpga->rd.buf = malloc(BUF_SIZE_MAX);
			if (!fpga->wr.buf || !fpga->rd.buf) {
				fprintf(stderr, "device_list_init_fpgas: malloc\n");
				return -1;
			}
			
			// These fields are used only in test.c
			fpga->data_in = device_unique_id++ * 8 + (unsigned)fpga->num + 1;
			fpga->data_out = fpga->data_in;

			// Application mode 1 (test): FPGAs send exactly what received
			// using output_limit=1, bypassing pkt_comm
			result = fpga_set_app_mode(fpga, 1);
			if (result < 0) {
				printf("SN %s error %d initializing FPGAs.\n",
						device->ztex_device->snString, result);
				device_invalidate(device);
				return result;
			}
		}

		ok_count ++;
	}
	return ok_count;
}

///////////////////////////////////////////////////////////////////////////////////
//
// FPGA Read / Write
//
///////////////////////////////////////////////////////////////////////////////////

long long wr_byte_count = 0, rd_byte_count = 0;
int partial_read_count = 0;
// FPGA's input buffer size is 32K; input buffer's prog_full asserted at 16K - don't write more than 16K at once
// output buffer is 32K minus 1 word (32766 bytes)
//int min_len = 514, max_len = 514;//16384;
//int min_len = 8192, max_len = 8192;
int min_len = 8192, max_len = 16384;

int device_fpgas_rw(struct device *device)
{
	int result;
	int num;
	for (num = 0; num < device->num_of_fpgas; num++) {
		struct fpga *fpga = &device->fpga[num];
		//if (!fpga->valid) // currently if r/w error on some FPGA, the entire device invalidated
		//	continue;

		//fpga_select(fpga); // unlike select_fpga() from Ztex SDK, it waits for i/o timeout
		result = fpga_select_setup_io(fpga); // combines fpga_select(), fpga_get_io_state() and fpga_setup_output() in 1 USB request
		if (result < 0) {
			fprintf(stderr, "SN %s FPGA #%d fpga_select_setup_io() error: %d\n",
				device->ztex_device->snString, num, result);
			return result;
		}

		// write
		if (!fpga->wr.wr_count || fpga->wr.wr_done) {
			// FPGA-based application processes in 2-byte words, don't write unaligned
			int len = 2* (random() % (max_len+1 - min_len)/2) + min_len;
			fpga->wr.len = len;
			fpga->data_out = buf_set(fpga->wr.buf, fpga->wr.len, fpga->data_out);
		}
		result = fpga_write(fpga);
		if (result < 0) {
			fprintf(stderr, "SN %s FPGA #%d write error: %d (%s)\n",
				device->ztex_device->snString, num, result, libusb_strerror(result));
			fpga->valid = 0;
			return result;
		}
		if (result > 0) {
			wr_byte_count += fpga->wr.len;
			if ( wr_byte_count/1024/1024 != (wr_byte_count - fpga->wr.len)/1024/1024 ) {
				printf(".");
				fflush(stdout);
			}
		}

		// read
		result = fpga_read(fpga);
		if (result < 0) {
			fprintf(stderr, "SN %s FPGA #%d read error: %d (%s)\n",
				device->ztex_device->snString, num, result, libusb_strerror(result));
			fpga->valid = 0;
			return result;
		}
		if (result > 0) {
			rd_byte_count += fpga->rd.read_limit;
			if (buf_check(fpga->rd.buf, fpga->rd.len, &fpga->data_in) < 0) {
				return -1;
			}
			partial_read_count += fpga->rd.partial_read_count;
			fpga->rd.partial_read_count = 0;
		}

	} // for( ;num_of_fpgas ;)
	return 1;
}


/////////////////////////////////////////////////////////////////////////////////////

struct device_bitstream bitstream_test = {
	0x0001,					// type ID, hardcoded at vcr.v/BITSTREAM_TYPE
	"../fpga/inouttraffic.bit",
	{ 2, 16384, 32766 }		// struct pkt_comm_params
};

int main(int argc, char **argv)
{
	if (argc == 2)
		bitstream_test.path = argv[1];
	
	set_random();

	int result = libusb_init(NULL);
	if (result < 0) {
		printf("libusb_init(): %s\n", libusb_strerror(result));
		exit(EXIT_FAILURE);
	}


	///////////////////////////////////////////////////////////////
	//
	// 1. Find ZTEX devices, initialize
	//
	///////////////////////////////////////////////////////////////
//ZTEX_DEBUG=1;
//DEBUG = 1;

	struct device_list *device_list = device_init_scan(&bitstream_test);
	
	int device_count = device_list_count(device_list);
	printf("%d device(s) ZTEX 1.15y ready\n", device_count);
	
	if (device_count)
		ztex_dev_list_print(device_list->ztex_dev_list);
	//else
	//	exit(0);
	
	device_list_init_fpgas_1(device_list);
	
	///////////////////////////////////////////////////////////////
	//
	// 2. Perform I/O.
	//
	///////////////////////////////////////////////////////////////

	// Signals aren't checked at time of firmware and bitstream uploads
	signal(SIGHUP, signal_handler);
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);
	signal(SIGALRM, signal_handler);

	printf("Writing to each FPGA of each device and reading back, random length writes (%d-%d)\n",
			min_len, max_len);

	struct timeval tv0, tv1;
	gettimeofday(&tv0, NULL);

	struct timeval tv2, tv3;
	gettimeofday(&tv2, NULL);

	for ( ; ; ) {
		if (signal_received) {
			printf("Signal received.\n");
			break;
		}

		
		struct device_list *device_list_1 = device_timely_scan(device_list, &bitstream_test);
		int found_devices = device_list_count(device_list_1);
		if (found_devices) {
			printf("Found %d device(s) ZTEX 1.15y\n", found_devices);
			ztex_dev_list_print(device_list_1->ztex_dev_list);
			device_list_init_fpgas_1(device_list_1);
		}
		device_list_merge(device_list, device_list_1);


		int device_count = 0;
		struct device *device;
		for (device = device_list->device; device; device = device->next) {
			if (!device_valid(device))
				continue;

			result = device_fpgas_rw(device);
			if (result < 0) {
				printf("SN %s error %d doing r/w of FPGAs (%s)\n", device->ztex_device->snString,
						result, libusb_strerror(result) );
				device_invalidate(device);
			}
			device_count ++;
		}

		if (!device_count) {
			gettimeofday(&tv3, NULL);
			if (tv3.tv_sec - tv2.tv_sec == 1) {
				printf("x"); fflush(stdout);
			}
			tv2 = tv3;
			usleep(500 *1000);
		}

	} // for(;;)

	gettimeofday(&tv1, NULL);
	unsigned long long usec = (tv1.tv_sec - tv0.tv_sec)*1000000 + tv1.tv_usec - tv0.tv_usec;
	float kbyte_count = (wr_byte_count+rd_byte_count)/1024;
	unsigned long long cmd_count = 0;

	printf("%.2f MB write, %.2f MB read, rate %.2f MB/s, partial reads %d\n",
		(float)wr_byte_count/1024/1024, (float)rd_byte_count/1024/1024, kbyte_count *1000000/usec /1024,
		partial_read_count);
	
	libusb_exit(NULL);
}

