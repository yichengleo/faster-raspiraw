#ifndef RASPIRAW_H
#define RASPIRAW_H
#define VERSION_STRING "0.0.4"

#define _GNU_SOURCE
#include <ctype.h>
#include <fcntl.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <pthread.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "interface/mmal/mmal.h"
#include "interface/mmal/mmal_buffer.h"
#include "interface/mmal/mmal_logging.h"
#include "interface/mmal/util/mmal_default_components.h"
#include "interface/mmal/util/mmal_util.h"
#include "interface/mmal/util/mmal_util_params.h"
#include "interface/mmal/util/mmal_connection.h"
#include "interface/vcos/vcos.h"


#include "bcm_host.h"
#include "RaspiCLI.h"
#include "raw_header.h"



#define I2C_SLAVE_FORCE 	0x0706

#define DEFAULT_I2C_DEVICE 	0
#define FRAME_LOG		   	0
#define BUFFER_NUM_MANUAL	8	// 0 sets the recommended buffer num

#define I2C_DEVICE_NAME_LEN 13	// "/dev/i2c-XXX"+NULL
static char i2c_device_name[I2C_DEVICE_NAME_LEN];

enum bayer_order {
	//Carefully ordered so that an hflip is ^1,
	//and a vflip is ^2.
	BAYER_ORDER_BGGR,
	BAYER_ORDER_GBRG,
	BAYER_ORDER_GRBG,
	BAYER_ORDER_RGGB
};

struct sensor_regs {
	uint16_t reg;
	uint16_t data;
};

struct mode_def
{
	struct sensor_regs *regs;
	int num_regs;
	int width;
	int height;
	MMAL_FOURCC_T encoding;
	enum bayer_order order;
	int native_bit_depth;
	uint8_t image_id;
	uint8_t data_lanes;
	unsigned int min_vts;
	int line_time_ns;
	uint32_t timing1;
	uint32_t timing2;
	uint32_t timing3;
	uint32_t timing4;
	uint32_t timing5;
	uint32_t term1;
	uint32_t term2;
	int black_level;
};

struct sensor_def
{
	char *name;
	struct mode_def *modes;
	int num_modes;
	struct sensor_regs *stop;
	int num_stop_regs;

	uint8_t i2c_addr;	// Device I2C slave address
	int i2c_addressing; // Length of register address values
	int i2c_data_size;	// Length of register data to write

	//  Detecting the device
	int i2c_ident_length;	  // Length of I2C ID register
	uint16_t i2c_ident_reg;	  // ID register address
	uint16_t i2c_ident_value; // ID register value

	// Flip configuration
	uint16_t vflip_reg;				   // Register for VFlip
	int vflip_reg_bit;				   // Bit in that register for VFlip
	uint16_t hflip_reg;				   // Register for HFlip
	int hflip_reg_bit;				   // Bit in that register for HFlip
	int flips_dont_change_bayer_order; // Some sensors do not change the
									   // Bayer order by adjusting X/Y starts
									   // to compensate.

	uint16_t exposure_reg;
	int exposure_reg_num_bits;

	uint16_t vts_reg;
	int vts_reg_num_bits;

	uint16_t gain_reg;
	int gain_reg_num_bits;

	uint16_t xos_reg;
	int xos_reg_num_bits;

	uint16_t yos_reg;
	int yos_reg_num_bits;
};

#define NUM_ELEMENTS(a)  (sizeof(a) / sizeof(a[0]))


enum {
	CommandHelp,
	CommandMode,
	CommandHFlip,
	CommandVFlip,
	CommandExposure,
	CommandGain,
	CommandOutput,
	CommandWriteHeader,
	CommandTimeout,
	CommandSaveRate,
	CommandBitDepth,
	CommandCameraNum,
	CommandExposureus,
	CommandI2cBus,
	CommandAwbGains,
	CommandRegs,
	CommandHinc,
	CommandVinc,
	CommandVoinc,
	CommandHoinc,
	CommandBin44,
	CommandFps,
	CommandWidth,
	CommandHeight,
	CommandLeft,
	CommandTop,
	CommandVts,
	CommandLine,
	CommandWriteHeader0,
	CommandWriteHeaderG,
	CommandWriteTimestamps,
	CommandWriteEmpty,
};


typedef struct __attribute__((aligned(16))) pts_node {
	uint32_t idx;
	uint64_t pts;
	struct pts_node *nxt;
} *PTS_NODE_T;

typedef struct
{
	int 	mode;
	int 	hflip;
	int 	vflip;
	int 	exposure;
	int 	gain;
	char 	*output;
	int 	capture;
	int 	write_header;
	int 	timeout;
	int 	saverate;
	int 	bit_depth;
	int 	camera_num;
	int 	exposure_us;
	int 	i2c_bus;
	double 	awb_gains_r;
	double 	awb_gains_b;
	char 	*regs;
	int 	hinc;
	int 	vinc;
	int 	voinc;
	int 	hoinc;
	int 	bin44;
	double 	fps;
	int 	width;
	int 	height;
	int 	left;
	int 	top;
	char 	*write_header0;
	char 	*write_headerg;
	char 	*write_timestamps;
	int 	write_empty;
	PTS_NODE_T ptsa;
	PTS_NODE_T ptso;
} RASPIRAW_PARAMS_T;


struct DEPTH {
    const uint32_t depth8[4];
    const uint32_t depth10[4];
    const uint32_t depth12[4];
    const uint32_t depth16[4];
};

extern const struct DEPTH DEPTH_T;

//The process first loads the cleaned up dump of the registers
//than updates the known registers to the proper values
//based on: http://www.seeedstudio.com/wiki/images/3/3c/Ov5647_full.pdf
enum operation {
       EQUAL,  //Set bit to value
       SET,    //Set bit
       CLEAR,  //Clear bit
       XOR     //Xor bit
};

static int parse_cmdline(int argc, char **argv, RASPIRAW_PARAMS_T *cfg);

#define MAX_THREADS 4

typedef struct file_copy_task{
    char *src;  // Source file path
    char *dst;  // Destination file path
	struct file_copy_task* next;
} file_copy_task_t;

void *worker(void* args);

void enqueue_task(char *const, char *const);
file_copy_task_t* dequeue_task();

void init_thread_pool(size_t);
void dstr_thread_pool(size_t);

#endif  // #ifndef
