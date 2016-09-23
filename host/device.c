#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libusb-1.0/libusb.h>

#include "ztex.h"
#include "inouttraffic.h"
#include "ztex_scan.h"
#include "pkt_comm/pkt_comm.h"
#include "device.h"


int device_init_fpgas(struct device *device, struct pkt_comm_params *params)
{
	int i;
	for (i = 0; i < device->num_of_fpgas; i++) {
		struct fpga *fpga = &device->fpga[i];
		
		int result = fpga_select(fpga);
		if (result < 0) {
			device_invalidate(device);
			return result;
		}

		// Resets FPGA application with Global Set Reset (GSR)
		result = fpga_reset(device->handle);
		if (result < 0) {
			printf("SN %s #%d: device_fpga_reset: %d (%s)\n",
				device->ztex_device->snString,
				i, result, libusb_strerror(result));
			device_invalidate(device);
			return result;
		}
		
		fpga->comm = pkt_comm_new(params);
		
	} // for
	return 0;
}

int device_list_init_fpgas(struct device_list *device_list, struct pkt_comm_params *params)
{
	int ok_count = 0;
	struct device *device;
	for (device = device_list->device; device; device = device->next) {
		if (!device_valid(device))
			continue;

		int result = device_init_fpgas(device, params);
		if (result < 0) {
			fprintf(stderr, "SN %s error %d initializing FPGAs.\n",
					device->ztex_device->snString, result);
			device_invalidate(device);
		}
		else
			ok_count ++;
	}
	return ok_count;
}


///////////////////////////////////////////////////////////////////
//
// Hardware Handling
//
// device_list_init() takes list of devices with uploaded firmware
// 1. upload bitstreams
// 2. initialize FPGAs
//
///////////////////////////////////////////////////////////////////

void device_list_init(struct device_list *device_list, struct device_bitstream *bitstream)
{
	// bitstream->type is hardcoded into bitstream (vcr.v/BITSTREAM_TYPE)
	if (!bitstream || !bitstream->type || !bitstream->path) {
		fprintf(stderr, "device_list_init(): invalid bitstream information\n");
		exit(-1);
	}

	int result = device_list_check_bitstreams(device_list, bitstream->type, bitstream->path);
	if (result < 0) {
		// fatal error
		exit(-1);
	}
	if (result > 0) {
		//usleep(3000);
		result = device_list_check_bitstreams(device_list, bitstream->type, NULL);
		if (result < 0) {
			exit(-1);
		}
	}

	device_list_init_fpgas(device_list, &bitstream->pkt_comm_params);

	// Application mode 2: use high-speed packet communication (pkt_comm)
	// that's the primary mode of operation as opposed to test modes 0 & 1.
	device_list_set_app_mode(device_list, 2);
}


///////////////////////////////////////////////////////////////////
//
// Top Level Hardware Initialization Function.
//
// device_timely_scan() takes the list of devices currently in use
//
// 1. Performs ztex_timely_scan()
// 2. Initialize devices
// 3. Returns list of newly found and initialized devices.
//
///////////////////////////////////////////////////////////////////

struct device_list *device_timely_scan(struct device_list *device_list, struct device_bitstream *bitstream)
{
	struct ztex_dev_list *ztex_dev_list_1 = ztex_dev_list_new();
	ztex_timely_scan(ztex_dev_list_1, device_list->ztex_dev_list);

	struct device_list *device_list_1 = device_list_new(ztex_dev_list_1);
	device_list_init(device_list_1, bitstream);

	return device_list_1;
}

struct device_list *device_init_scan(struct device_bitstream *bitstream)
{
	struct ztex_dev_list *ztex_dev_list = ztex_dev_list_new();
	ztex_init_scan(ztex_dev_list);
	
	struct device_list *device_list = device_list_new(ztex_dev_list);
	device_list_init(device_list, bitstream);
	
	return device_list;
}


/////////////////////////////////////////////////////////////////////////////////////

//unsigned long long wr_byte_count = 0, rd_byte_count = 0;

int device_fpgas_pkt_rw(struct device *device)
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

		if (fpga->wr.io_state.pkt_comm_status) {
			fprintf(stderr, "SN %s FPGA #%d error: pkt_comm_status=0x%02x\n",
				device->ztex_device->snString, num, fpga->wr.io_state.pkt_comm_status);
			return -1;
		}

		if (fpga->wr.io_state.app_status) {
			fprintf(stderr, "SN %s FPGA #%d error: app_status=0x%02x\n",
				device->ztex_device->snString, num, fpga->wr.io_state.app_status);
			return -1;
		}

		result = fpga_pkt_write(fpga);
		if (result < 0) {
			fprintf(stderr, "SN %s FPGA #%d write error: %d (%s)\n",
				device->ztex_device->snString, num, result, libusb_strerror(result));
			return result; // on such a result, device will be invalidated
		}
		//if (result > 0) {
			//wr_byte_count += result;
			//if ( wr_byte_count/1024/1024 != (wr_byte_count - result)/1024/1024 ) {
				//printf(".");
				//fflush(stdout);
		//	}
		//}

		// read
		result = fpga_pkt_read(fpga);
		if (result < 0) {
			fprintf(stderr, "SN %s FPGA #%d read error: %d (%s)\n",
				device->ztex_device->snString, num, result, libusb_strerror(result));
			return result; // on such a result, device will be invalidated
		}
		//if (result > 0)
		//	rd_byte_count += result;

	} // for( ;num_of_fpgas ;)
	return 1;
}


