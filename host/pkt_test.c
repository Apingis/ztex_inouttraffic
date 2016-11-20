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
#include "pkt_comm/word_list.h"
#include "pkt_comm/word_gen.h"
#include "device.h"

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


struct device_bitstream bitstream_test = {
	0x0001,					// type ID, hardcoded at vcr.v/BITSTREAM_TYPE
	"../fpga/inouttraffic.bit",
	{	2,	// 2 is constant for the board; reflects the fact of 16-bit I/O
		16384, // FPGA's input buffer can accept this many bytes when IO_STATE_INPUT_PROG_FULL deasserted
		32766 // Size of FPGA's output buffer; can receive at most this many bytes in 1 read
	}		// struct pkt_comm_params
};


// Range bbb00000 - bbb99999
// Start from bbb00500, generate 100
struct word_gen word_gen_100k = {
	8,
	{ 
		{ 1, 0, 98 },
		{ 1, 0, 98 },
		{ 1, 0, 98 },
		{ 10, 0, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57 },
		{ 10, 0, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57 },
		{ 10, 5, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57 }, // start from 00500
		{ 10, 0, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57 },
		{ 10, 0, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57 }
	},
	0, {},
	100		// generate this many per word
};

// Range [0-9]{insert_word}[0-9][0-9] (1000 per word)
struct word_gen word_gen_word1k = {
	3,
	{
		{ 10, 0, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57 },
		{ 10, 0, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57 },
		{ 10, 0, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57 }
	},
	1, 1,	// insert word at position 1
	3		// generate 3 per word
};

// list of 16 words
char *words[] = {
	"aaaaa", "bbb", "cc", "dddddd", "e", "f", "g", "hh",
	"iii", "jjjj", "kkkkk", "llll", "mmm", "nn", "p", "q",
	NULL };

// This configuration generates 1 word
struct word_gen word_gen_test_input_bandwith = {
	8,
	{
		{ 1, 0, 48 }, { 1, 0, 49 }, { 1, 0, 50 }, { 1, 0, 51 },
		{ 1, 0, 52 }, { 1, 0, 53 }, { 1, 0, 54 }, { 1, 0, 55 }
	},
	0
};

//////////////////////////////////////////////////////////////

int main(int argc, char **argv)
{
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
	fprintf(stderr, "%d device(s) ZTEX 1.15y ready\n", device_count);
	
	if (device_count)
		ztex_dev_list_print(device_list->ztex_dev_list);
	//else
	//	exit(0);
	

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


	int pkt_id = 0;
	int pkt_count = 0;

	struct timeval tv0, tv1;
	gettimeofday(&tv0, NULL);

	for ( ; ; ) {
		// timely scan for new devices
		struct device_list *device_list_1 = device_timely_scan(device_list, &bitstream_test);
		int found_devices = device_list_count(device_list_1);
		if (found_devices) {
			fprintf(stderr, "Found %d device(s) ZTEX 1.15y\n", found_devices);
			ztex_dev_list_print(device_list_1->ztex_dev_list);
		}
		device_list_merge(device_list, device_list_1);


		int device_count = 0;
		struct device *device;
		for (device = device_list->device; device; device = device->next) {
			if (!device_valid(device))
				continue;

			if (signal_received)
				break;

			result = device_pkt_rw(device);
			if (result < 0) {
				fprintf(stderr, "SN %s device_pkt_rw(): %d (%s)\n",
					device->ztex_device->snString, result, libusb_strerror(result) );
				device_invalidate(device);
				continue;
			}
			device_count ++;


			struct pkt *inpkt;
			// Using FPGA #0 of each device for tests
			while ( (inpkt = pkt_queue_fetch(device->fpga[0].comm->input_queue) ) ) {
				/*
				printf("%s pkt 0x%02x len %d - id: %d w: %d cand: %d - %.8s\n",
					device->ztex_device->snString,
					inpkt->type,inpkt->data_len,
					inpkt->id,
					inpkt->data[8] + inpkt->data[9] * 256,
					inpkt->data[10] + inpkt->data[11] * 256 + inpkt->data[12] * 65536,
					inpkt->data);
				*/
				
				if (!(++pkt_count % 64000)) {
					fprintf(stderr,".");
					fflush(stderr);
				}
				
				pkt_delete(inpkt);
			}
			//printf("\n");
			//printf("pkt_count: %d\n", get_pkt_count());
		
			struct pkt *outpkt;
			struct pkt *outpkt2;
			int i;
			
			/*
			for (i=0; i<1; i++) {
				if (pkt_queue_full(device->fpga[0].comm->output_queue, 1))
					break;
				outpkt = pkt_word_gen_new(&word_gen_test_input_bandwith);
				outpkt->id = pkt_id++;
				pkt_queue_push(device->fpga[0].comm->output_queue, outpkt);
			}
			*/
			
			for (i=0; i<1; i++) {
				if (pkt_queue_full(device->fpga[0].comm->output_queue, 1))
					break;
				outpkt = pkt_word_gen_new(&word_gen_100k);
				outpkt->id = pkt_id++;
				pkt_queue_push(device->fpga[0].comm->output_queue, outpkt);
			}
			
			if (pkt_queue_full(device->fpga[0].comm->output_queue, 2))
				break;
			outpkt = pkt_word_gen_new(&word_gen_word1k);
			outpkt->id = pkt_id++;
			pkt_queue_push(device->fpga[0].comm->output_queue, outpkt);
		
			outpkt = pkt_word_list_new(words);
			pkt_queue_push(device->fpga[0].comm->output_queue, outpkt);

		} // for (device_list)

		if (signal_received) {
			fprintf(stderr, "Signal received.\n");
			break;
		}
	} // for(;;)


	gettimeofday(&tv1, NULL);
	unsigned long usec = (tv1.tv_sec - tv0.tv_sec)*1000000 + tv1.tv_usec - tv0.tv_usec;

	libusb_exit(NULL);
}

