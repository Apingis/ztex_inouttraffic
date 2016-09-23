//
// Top Level Hardware Operation - Data Structures & Functions.
//
struct device_bitstream {
	unsigned short type;
	char *path;
	struct pkt_comm_params pkt_comm_params;
};

// device_list_init() takes list of devices with uploaded firmware
// 1. upload specified bitstreams
// 2. initialize FPGAs
void device_list_init(struct device_list *device_list, struct device_bitstream *bitstream);

// - Scan for devices at program initialization
// - Upload specified bitstream
// - Initialize devices
// - Return list of newly found and initialized devices.
// The function waits until device initialization and it takes some time.
struct device_list *device_init_scan(struct device_bitstream *bitstream);

// - Scan for new devices when program is running
// - Upload specified bitstream
// - *device_list argument points at devices already operated (to be skipped)
// - Invoke timely, actual scan occurs as often as defined in ztex_scan.h
// - Initialize devices
// - Return list of newly found and initialized devices.
// The function returns ASAP to continue initialization sequences
// at next invocations.
struct device_list *device_timely_scan(struct device_list *device_list, struct device_bitstream *bitstream);

// Perform r/w operation on device
// using high-speed packet communication interface (pkt_comm)
// Return values:
// < 0 - error
// >= 0 - OK, including the case when no data was actually transmitted
int device_fpgas_pkt_rw(struct device *device);

