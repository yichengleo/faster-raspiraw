/**
 * Modified by https://github.com/yichengleo/
 */
/*
Copyright (c) 2015, Raspberry Pi Foundation
Copyright (c) 2015, Dave Stevenson
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "raspiraw.h"
#include "operations.h"

const struct DEPTH DEPTH_T = {
    { MMAL_ENCODING_BAYER_SBGGR8, MMAL_ENCODING_BAYER_SGBRG8, MMAL_ENCODING_BAYER_SGRBG8, MMAL_ENCODING_BAYER_SRGGB8 },
    { MMAL_ENCODING_BAYER_SBGGR10P, MMAL_ENCODING_BAYER_SGBRG10P, MMAL_ENCODING_BAYER_SGRBG10P, MMAL_ENCODING_BAYER_SRGGB10P },
    { MMAL_ENCODING_BAYER_SBGGR12P, MMAL_ENCODING_BAYER_SGBRG12P, MMAL_ENCODING_BAYER_SGRBG12P, MMAL_ENCODING_BAYER_SRGGB12P },
    { MMAL_ENCODING_BAYER_SBGGR16, MMAL_ENCODING_BAYER_SGBRG16, MMAL_ENCODING_BAYER_SGRBG16, MMAL_ENCODING_BAYER_SRGGB16 }
};

const static COMMAND_LIST cmdline_commands[] =
{
	{ CommandHelp,			"-help",		"?",  	"This help information", 0 },
	{ CommandMode,			"-mode",		"md", 	"Set sensor mode <mode>", 1 },
	{ CommandHFlip,			"-hflip",		"hf", 	"Set horizontal flip", 0},
	{ CommandVFlip,			"-vflip",		"vf", 	"Set vertical flip", 0},
	{ CommandExposure,		"-ss",			"e",  	"Set the sensor exposure time (not calibrated units)", 0 },
	{ CommandGain,			"-gain",		"g",  	"Set the sensor gain code (not calibrated units)", 0 },
	{ CommandOutput,		"-output",		"o",  	"Set the output filename", 0 },
	{ CommandWriteHeader,	"-header",		"hd", 	"Write the BRCM header to the output file", 1 },
	{ CommandTimeout,		"-timeout",		"t",  	"Time (in ms) before shutting down (if not specified, set to 5s)", 1 },
	{ CommandSaveRate, 		"-saverate",	"sr", 	"Save every Nth frame", 1 },
	{ CommandBitDepth, 		"-bitdepth",	"b",  	"Set output raw bit depth (8, 10, 12 or 16, if not specified, set to sensor native)", 1 },
	{ CommandCameraNum, 	"-cameranum",	"c",  	"Set camera number to use (0=CAM0, 1=CAM1).", 1 },
	{ CommandExposureus, 	"-expus",		"eus",  "Set the sensor exposure time in micro seconds.", -1 },
	{ CommandI2cBus, 		"-i2c",	        "y",  	"Set the I2C bus to use.", -1 },
	{ CommandAwbGains, 		"-awbgains",	"awbg", "Set the AWB gains to use.", 1 },
	{ CommandRegs,	 		"-regs",		"r",  	"Change (current mode) regs", 0 },
	{ CommandHinc,			"-hinc",		"hi", 	"Set horizontal odd/even inc reg", -1},
	{ CommandVinc,			"-vinc",		"vi", 	"Set vertical odd/even inc reg", -1},   // ov5647
	{ CommandVoinc,			"-voinc",		"voi",	"Set vertical odd inc reg", -1},        // imx219
	{ CommandHoinc,			"-hoinc",		"hoi",	"Set horizontal odd inc reg", -1},      // imx219
	{ CommandBin44,			"-bin44",		"b44",	"Set 4x4 binning", -1},      			// imx219
	{ CommandFps,			"-fps",			"f",  	"Set framerate regs", -1},
	{ CommandWidth,			"-width",		"w",  	"Set current mode width", -1},
	{ CommandHeight,		"-height",		"h",  	"Set current mode height", -1},
	{ CommandLeft,			"-left",		"lt", 	"Set current mode left", -1},
	{ CommandTop,			"-top",			"tp", 	"Set current mode top", -1},
	{ CommandWriteHeader0,	"-header0",		"hd0",	"Sets filename to write the BRCM header to", 0 },
	{ CommandWriteHeaderG,	"-headerg",		"hdg",	"Sets filename to write the .pgm header to", 0 },
	{ CommandWriteTimestamps,"-tstamps",	"ts", 	"Sets filename to write timestamps to", 0 },
	{ CommandWriteEmpty,	"-empty",		"emp",	"Write empty output files", 0 },
};

struct brcm_raw_header *brcm_header = NULL;

const static int cmdline_commands_size = sizeof(cmdline_commands) / sizeof(cmdline_commands[0]);

static char* mem_dir = "/dev/shm";
static char* des_dir = NULL;
static char* appended_path = NULL;
volatile bool enableCopy = true;

pthread_t threads[MAX_THREADS];  									// Working threads
pthread_mutex_t task_enqueue_mutex = PTHREAD_MUTEX_INITIALIZER;  	// Mutex for protecting task queue
pthread_mutex_t task_dequeue_mutex = PTHREAD_MUTEX_INITIALIZER;  	// Mutex for protecting task queue
sem_t produced_sem;

file_copy_task_t* task_queue_head = NULL;
file_copy_task_t* task_queue_tail = NULL;

void init_thread_pool(size_t num_threads) {
    pthread_mutex_init(&task_enqueue_mutex, NULL);
	pthread_mutex_init(&task_dequeue_mutex, NULL);
    sem_init(&produced_sem, 0, 0);
    // sem_init(&producer_stop_sem, 0, 0);


    pthread_t threads[num_threads];
    for (int i = 0; i < num_threads; ++i) {
        pthread_create(&threads[i], NULL, worker, NULL);
    }
}

void dstr_thread_pool(size_t num_threads){
	// sem_wait(&producer_stop_sem);
	for (int i = 0; i < num_threads; ++i) {
        pthread_join(threads[i], NULL);
    }
	pthread_mutex_destroy(&task_dequeue_mutex);
	pthread_mutex_destroy(&task_enqueue_mutex);
    sem_destroy(&produced_sem);
	// sem_destroy(&producer_stop_sem);
}

void enqueue_task(char *const src, char *const dst) {

    pthread_mutex_lock(&task_enqueue_mutex);

	file_copy_task_t *new_task = malloc(sizeof(file_copy_task_t));

    if (task_queue_tail) {
        task_queue_tail->next = new_task;
    } else {
        task_queue_head = new_task;
    }
    task_queue_tail = new_task;
	task_queue_tail->src = src;
    task_queue_tail->dst = dst;

    pthread_mutex_unlock(&task_enqueue_mutex);
	sem_post(&produced_sem);  // Signal a new task
}

file_copy_task_t* dequeue_task(){

	pthread_mutex_lock(&task_dequeue_mutex);
	if (task_queue_head == NULL) {
        pthread_mutex_unlock(&task_dequeue_mutex);
        return NULL;
    }

	file_copy_task_t* task = malloc(sizeof(task_queue_head));
	// Deep copy the src and dst strings
	task->src = strdup(task_queue_head->src);
	task->dst = strdup(task_queue_head->dst);
	task->next = NULL;

	task_queue_head = task_queue_head->next;

	if (task_queue_head == NULL) {
		task_queue_tail = NULL;
	}

	pthread_mutex_unlock(&task_dequeue_mutex);
	return task;
}

void* worker(void *args){
	while(1){
		sem_wait(&produced_sem);
		file_copy_task_t* task = dequeue_task();
		if(task){
            int src_fd = shm_open(strrchr(task->src, '/'), O_RDONLY, 0644);
            // int src_fd = open(task->src, O_RDONLY);
            if (src_fd < 0) {
                perror("Error opening source file");
                goto cleanup;
            }

            struct stat st;
            if (fstat(src_fd, &st) < 0) {
                perror("Error getting source file size");
                goto cleanup;
            }

            size_t file_sz = st.st_size;
            void *src_map = mmap(NULL, file_sz, PROT_READ, MAP_SHARED, src_fd, 0);
            if (src_map == MAP_FAILED) {
                perror("Error mmap'ing source file");
                goto cleanup;
            }

            int dst_fd = open(task->dst, O_RDWR | O_CREAT | O_TRUNC, 0666);
            if (dst_fd < 0) {
                perror("Error opening destination file");
                goto cleanup;
            }

            if (ftruncate(dst_fd, file_sz) < 0) {
                perror("Error setting destination file size");
                goto cleanup;
            }

            void *dst_map = mmap(NULL, file_sz, PROT_WRITE, MAP_SHARED, dst_fd, 0);
            if (dst_map == MAP_FAILED) {
                perror("Error mmap'ing destination file");
                goto cleanup;
            }

            memcpy(dst_map, src_map, file_sz);  // Perform the copy

            munmap(src_map, file_sz);
            munmap(dst_map, file_sz);
            close(src_fd);
            close(dst_fd);
			
			if (unlink(task->src) != 0) {
				perror("Error deleting source file after copy");
				goto cleanup;
			}
			goto cleanrest;

		cleanup:
            // Clean up task memory
			if(task->src){
				free(task->src);
				task->src = NULL;
			}
		cleanrest:
			if(task->dst){
				free(task->dst);
				task->src= NULL;
			}
			if(task){
				free(task);
				task->src = NULL;
			}
		}
		else{
			continue;
		}
	}
	
}


int i2c_rd(int fd, uint8_t i2c_addr, uint16_t reg, uint8_t *values, uint32_t n, const struct sensor_def *sensor)
{
	int err;
	uint8_t buf[2] = { reg >> 8, reg & 0xff };
	struct i2c_rdwr_ioctl_data msgset;
	struct i2c_msg msgs[2] = {
		{
			 .addr = i2c_addr,
			 .flags = 0,
			 .len = 2,
			 .buf = buf,
		},
		{
			.addr = i2c_addr,
			.flags = I2C_M_RD,
			.len = n,
			.buf = values,
		},
	};

	if (sensor->i2c_addressing == 1)
	{
		msgs[0].len = 1;
	}
	msgset.msgs = msgs;
	msgset.nmsgs = 2;

	err = ioctl(fd, I2C_RDWR, &msgset);
	// vcos_log_error("Read i2c addr %02X, reg %04X (len %d), value %02X, err %d", i2c_addr, msgs[0].buf[0], msgs[0].len, values[0], err);
	if (err != msgset.nmsgs)
		return -1;

	return 0;
}

void start_camera_streaming(const struct sensor_def *sensor, struct mode_def *mode)
{
	int fd;
	fd = open(i2c_device_name, O_RDWR);
	if (!fd)
	{
		vcos_log_error("Couldn't open I2C device");
		return;
	}
	if (ioctl(fd, I2C_SLAVE_FORCE, sensor->i2c_addr) < 0)
	{
		vcos_log_error("Failed to set I2C address");
		return;
	}
	send_regs(fd, sensor, mode->regs, mode->num_regs);
	close(fd);
	vcos_log_error("Now streaming...");
}

void stop_camera_streaming(const struct sensor_def *sensor)
{
	int fd;
	fd = open(i2c_device_name, O_RDWR);
	if (!fd)
	{
		vcos_log_error("Couldn't open I2C device");
		return;
	}
	if (ioctl(fd, I2C_SLAVE_FORCE, sensor->i2c_addr) < 0)
	{
		vcos_log_error("Failed to set I2C address");
		return;
	}
	send_regs(fd, sensor, sensor->stop, sensor->num_stop_regs);
	close(fd);
}


#include "ov5647_modes.h"
#include "imx219_modes.h"
#include "adv7282m_modes.h"

const struct sensor_def *sensors[] = {
	&ov5647,
	&imx219,
	&adv7282,
	NULL
};

const struct sensor_def* probe_sensor(void)
{
	int fd;
	const struct sensor_def **sensor_list = &sensors[0];
	const struct sensor_def *sensor = NULL;

	fd = open(i2c_device_name, O_RDWR);
	if (!fd)
	{
		vcos_log_error("Couldn't open I2C device");
		return NULL;
	}

	while(*sensor_list != NULL)
	{
		uint16_t reg = 0;
		sensor = *sensor_list;
		vcos_log_error("Probing sensor %s on addr %02X", sensor->name, sensor->i2c_addr);
		if (sensor->i2c_ident_length <= 2)
		{
			if (!i2c_rd(fd, sensor->i2c_addr, sensor->i2c_ident_reg, (uint8_t*)&reg, sensor->i2c_ident_length, sensor))
			{
				if (reg == sensor->i2c_ident_value)
				{
					vcos_log_error("Found sensor %s at address %02X", sensor->name, sensor->i2c_addr);
					break;
				}
			}
		}
		sensor_list++;
		sensor = NULL;
	}
	return sensor;
}

/**
 * Allocates and generates a filename based on the
 * user-supplied pattern and the frame number.
 * On successful return, finalName and tempName point to malloc()ed strings
 * which must be freed externally.  (On failure, returns nulls that
 * don't need free()ing.)
 *
 * @param finalName pointer receives an
 * @param pattern sprintf pattern with %d to be replaced by frame
 * @param frame for timelapse, the frame number
 * @return Returns a MMAL_STATUS_T giving result of operation
*/

MMAL_STATUS_T create_filenames(char** finalName, char * pattern, int frame)
{
	*finalName = NULL;
	if (0 > asprintf(finalName, pattern, frame))
	{
		return MMAL_ENOMEM;    // It may be some other error, but it is not worth getting it right
	}
	return MMAL_SUCCESS;
}

int running = 0;
static void callback(MMAL_PORT_T *port, MMAL_BUFFER_HEADER_T *buffer)
{
	volatile static u_int32_t count = 0;
#if FRAME_LOG
		vcos_log_error("Buffer %p returned, filled %d, timestamp %llu, flags %04X", buffer, buffer->length, buffer->pts, buffer->flags);
#endif
	if (running)
	{
		RASPIRAW_PARAMS_T *cfg = (RASPIRAW_PARAMS_T *)port->userdata;

		if (!(buffer->flags & MMAL_BUFFER_HEADER_FLAG_CODECSIDEINFO) &&
			(((count++) % cfg->saverate) == 0))
		{
			// FIXME
			// Save every Nth frame
			// SD card access is too slow to do much more.

			char *filename = NULL;
			char *des_filename = NULL;
			// printf("Filename: %s\n", mem_dir);
			if (asprintf(&filename, mem_dir, count) >= 0 &&
				(asprintf(&des_filename, des_dir, count) >= 0))
			{
				// printf("\nfilename: %s, des_filename%s\n", filename, des_filename);
				int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
				if (fd >= 0)
				{
					// Calculate the size needed for the file
					size_t file_size = buffer->length;
					if (cfg->write_header)
						file_size += BRCM_RAW_HEADER_LENGTH;

					// Set the file size
					ftruncate(fd, file_size);

					// Memory-map the file
					void *mapped_mem = mmap(NULL, file_size, PROT_WRITE, MAP_SHARED, fd, 0);
					if (mapped_mem != MAP_FAILED)
					{
						size_t offset = 0;

						if (cfg->ptso) // make sure previous malloc() was successful
						{
							cfg->ptso->idx = count;
							cfg->ptso->pts = buffer->pts;
							cfg->ptso->nxt = malloc(sizeof(*cfg->ptso->nxt));
							cfg->ptso = cfg->ptso->nxt;
						}

						if (!cfg->write_empty)
						{
							if (cfg->write_header)
							{
								memcpy(mapped_mem, brcm_header, BRCM_RAW_HEADER_LENGTH);
								offset += BRCM_RAW_HEADER_LENGTH;
							}
							memcpy(mapped_mem + offset, buffer->data, buffer->length);
						}
						// Unmap the file
						munmap(mapped_mem, file_size);
					}
					else
					{
						// Handle mmap failure
						perror("mmap");
					}
					close(fd);

					// vcos_log_error("Now enqueueing task success...");
				}
				else
				{
					// Handle open file failure
					perror("open");
				}

				// FIXME
				// signal to copy the file
				// vcos_log_error("Now enqueueing task...");
				if (filename && des_filename)
				{
					// printf("%s, %s\n", filename, des_filename);
					if (enableCopy){
						char* src = strdup(filename);
						char* dst = strdup(des_filename);
						enqueue_task(src, dst);

					}
				}

				if (filename){
					free(filename);
					filename = NULL;
				}
				if (des_filename){
					free(des_filename);
					filename = NULL;
				}
			}
		}
		buffer->length = 0;
		mmal_port_send_buffer(port, buffer);
	}
	else
		mmal_buffer_header_release(buffer);
}

uint32_t order_and_bit_depth_to_encoding(enum bayer_order order, int bit_depth)
{
	//BAYER_ORDER_BGGR,
	//BAYER_ORDER_GBRG,
	//BAYER_ORDER_GRBG,
	//BAYER_ORDER_RGGB
	if (order < 0 || order > 3)
	{
		vcos_log_error("order out of range - %d", order);
		return 0;
	}

	switch(bit_depth)
	{
		case 8:
			return DEPTH_T.depth8[order];
		case 10:
			return DEPTH_T.depth10[order];
		case 12:
			return DEPTH_T.depth12[order];
		case 16:
			return DEPTH_T.depth16[order];
	}
	vcos_log_error("%d not one of the handled bit depths", bit_depth);
	return 0;
}

/**
 * Parse the incoming command line and put resulting parameters in to the state
 *
 * @param argc Number of arguments in command line
 * @param argv Array of pointers to strings from command line
 * @param state Pointer to state structure to assign any discovered parameters to
 * @return non-0 if failed for some reason, 0 otherwise
 */
static int parse_cmdline(int argc, char **argv, RASPIRAW_PARAMS_T *cfg)
{
	// Parse the command line arguments.
	// We are looking for --<something> or -<abbreviation of something>

	int valid = 1;
	int i;

	for (i = 1; i < argc && valid; i++)
	{
		int command_id, num_parameters, len;

		if (!argv[i])
			continue;

		if (argv[i][0] != '-')
		{
			valid = 0;
			continue;
		}

		// Assume parameter is valid until proven otherwise
		valid = 1;

		command_id = raspicli_get_command_id(cmdline_commands, cmdline_commands_size, &argv[i][1], &num_parameters);

		// If we found a command but are missing a parameter, continue (and we will drop out of the loop)
		if (command_id != -1 && num_parameters > 0 && (i + 1 >= argc) )
			continue;

		//  We are now dealing with a command line option
		switch (command_id)
		{
			case CommandHelp:
				raspicli_display_help(cmdline_commands, cmdline_commands_size);
				// exit straight away if help requested
				return -1;

			case CommandMode:
				if (sscanf(argv[i + 1], "%d", &cfg->mode) != 1)
					valid = 0;
				else
					i++;
				break;

			case CommandHFlip:
				cfg->hflip = 1;
				break;

			case CommandVFlip:
				cfg->vflip = 1;
				break;

			case CommandExposure:
				if (sscanf(argv[i + 1], "%d", &cfg->exposure) != 1)
					valid = 0;
				else
					i++;
				break;

			case CommandGain:
				if (sscanf(argv[i + 1], "%d", &cfg->gain) != 1)
					valid = 0;
				else
					i++;
				break;

			case CommandOutput:  // output filename
			{
				len = strlen(argv[i + 1]);
				if (len)
				{
					//We use sprintf to append the frame number for timelapse mode
					//Ensure that any %<char> is either %% or %d.
					const char *percent = argv[i+1];
					while(valid && *percent && (percent=strchr(percent, '%')) != NULL)
					{
					int digits=0;
					percent++;
					while(isdigit(*percent))
					{
						percent++;
						digits++;
					}
					if (!((*percent == '%' && !digits) || *percent == 'd'))
					{
						valid = 0;
						fprintf(stderr, "Filename contains %% characters, but not %%d or %%%% - sorry, will fail\n");
					}
					percent++;
				}
				cfg->output = malloc(len + 10); // leave enough space for any timelapse generated changes to filename
				des_dir = malloc(len + 10);
				vcos_assert(cfg->output);
				if (cfg->output){
					strncpy(cfg->output, argv[i + 1], len+1);
					strncpy(des_dir, argv[i + 1], len+1);
					const char *last_slash = strrchr(des_dir, '/');
					vcos_log_error("Now setting enableCopy...");
					if (last_slash != NULL)
					{
						appended_path = malloc(strlen(mem_dir) + strlen(last_slash) + 1);
						strcpy(appended_path, mem_dir);	   
						strcat(appended_path, last_slash); 						
						size_t prefix_len = last_slash - des_dir;
						if (strncmp(appended_path, des_dir, prefix_len) != 0)
						{
							enableCopy = true;
							mem_dir = appended_path;
							// printf("\nINmem_dir: %s\n", mem_dir);
						}
						else{
							enableCopy = false;
							mem_dir = des_dir;
							// printf("\nOUTmem_dir: %s\n", mem_dir);
						}
					}

					i++;
					cfg->capture = 1;
					}
				}
				else
					valid = 0;
				break;
			}

			case CommandWriteHeader:
				cfg->write_header = 1;
				break;

			case CommandTimeout: // Time to run for in milliseconds
				if (sscanf(argv[i + 1], "%u", &cfg->timeout) == 1)
				{
					i++;
				}
				else
					valid = 0;
				break;

			case CommandSaveRate:
				if (sscanf(argv[i + 1], "%u", &cfg->saverate) == 1)
				{
					i++;
				}
				else
					valid = 0;
				break;

			case CommandBitDepth:
				if (sscanf(argv[i + 1], "%u", &cfg->bit_depth) == 1)
				{
					i++;
				}
				else
					valid = 0;
				break;

			case CommandCameraNum:
				if (sscanf(argv[i + 1], "%u", &cfg->camera_num) == 1)
				{
					i++;
					if (cfg->camera_num !=0 && cfg->camera_num != 1)
					{
						fprintf(stderr, "Invalid camera number specified (%d)."
							" It should be 0 or 1.\n", cfg->camera_num);
						valid = 0;
					}
				}
				else
					valid = 0;
				break;

			case CommandExposureus:
				if (sscanf(argv[i + 1], "%d", &cfg->exposure_us) != 1)
					valid = 0;
				else
					i++;
				break;

			case CommandI2cBus:
				if (sscanf(argv[i + 1], "%d", &cfg->i2c_bus) != 1)
					valid = 0;
				else
					i++;
				break;

			case CommandAwbGains:
			{
				double r,b;
				int args;

				args = sscanf(argv[i + 1], "%lf,%lf", &r,&b);

				if (args != 2 || r > 8.0 || b > 8.0)
					valid = 0;

				cfg->awb_gains_r = r;
				cfg->awb_gains_b = b;

				i++;
				break;
			}

			case CommandRegs:  // register changes
			{
				len = strlen(argv[i + 1]);
				cfg->regs = malloc(len+1);
				vcos_assert(cfg->regs);
				strncpy(cfg->regs, argv[i + 1], len+1);
				i++;
				break;
			}

			case CommandHinc:
				if (strlen(argv[i+1]) != 2 ||
                                    sscanf(argv[i + 1], "%x", &cfg->hinc) != 1)
					valid = 0;
				else
					i++;
				break;

			case CommandVinc:
				if (strlen(argv[i+1]) != 2 ||
                                    sscanf(argv[i + 1], "%x", &cfg->vinc) != 1)
					valid = 0;
				else
					i++;
				break;

			case CommandVoinc:
				if (strlen(argv[i+1]) != 2 ||
                                    sscanf(argv[i + 1], "%x", &cfg->voinc) != 1)
					valid = 0;
				else
					i++;
				break;

			case CommandHoinc:
				if (strlen(argv[i+1]) != 2 ||
                                    sscanf(argv[i + 1], "%x", &cfg->hoinc) != 1)
					valid = 0;
				else
					i++;
				break;

			case CommandBin44:
				cfg->bin44 = 1;
				break;

			case CommandFps:
				if (sscanf(argv[i + 1], "%lf", &cfg->fps) != 1)
					valid = 0;
				else
					i++;
				break;

			case CommandWidth:
				if (sscanf(argv[i + 1], "%d", &cfg->width) != 1)
					valid = 0;
				else
					i++;
				break;

			case CommandHeight:
				if (sscanf(argv[i + 1], "%d", &cfg->height) != 1)
					valid = 0;
				else
					i++;
				break;

			case CommandLeft:
				if (sscanf(argv[i + 1], "%d", &cfg->left) != 1)
					valid = 0;
				else
					i++;
				break;

			case CommandTop:
				if (sscanf(argv[i + 1], "%d", &cfg->top) != 1)
					valid = 0;
				else
					i++;
				break;

			case CommandWriteHeader0:
				len = strlen(argv[i + 1]);
				cfg->write_header0 = malloc(len + 1);
				vcos_assert(cfg->write_header0);
				strncpy(cfg->write_header0, argv[i + 1], len+1);
				i++;
				break;

			case CommandWriteHeaderG:
				len = strlen(argv[i + 1]);
				cfg->write_headerg = malloc(len + 1);
				vcos_assert(cfg->write_headerg);
				strncpy(cfg->write_headerg, argv[i + 1], len+1);
				i++;
				break;

			case CommandWriteTimestamps:
				len = strlen(argv[i + 1]);
				cfg->write_timestamps = malloc(len + 1);
				vcos_assert(cfg->write_timestamps);
				strncpy(cfg->write_timestamps, argv[i + 1], len+1);
				i++;
				cfg->ptsa = malloc(sizeof(*cfg->ptsa));
				cfg->ptso = cfg->ptsa;
				break;

			case CommandWriteEmpty:
				cfg->write_empty = 1;
				break;
				
			default:
				valid = 0;
				break;
		}
	}

	if (!valid)
	{
		fprintf(stderr, "Invalid command line option (%s)\n", argv[i-1]);
		return 1;
	}

	return 0;
}


int main(int argc, char** argv) {
	RASPIRAW_PARAMS_T cfg = {
		.mode = 0,
		.hflip = 0,
		.vflip = 0,
		.exposure = -1,
		.gain = -1,
		.output = NULL,
		.capture = 0,
		.write_header = 1,
		.timeout = 5000,
		.saverate = 20,
		.bit_depth = -1,
		.camera_num = -1,
		.exposure_us = -1,
		.i2c_bus = DEFAULT_I2C_DEVICE,
		.regs = NULL,
		.hinc = -1,
		.vinc = -1,
		.voinc = -1,
		.hoinc = -1,
		.bin44 = 0,
		.fps = -1,
		.width = -1,
		.height = -1,
		.left = -1,
		.top = -1,
		.write_header0 = NULL,
		.write_headerg = NULL,
		.write_timestamps = NULL,
		.write_empty = 0,
		.ptsa = NULL,
		.ptso = NULL,
	};
	uint32_t encoding;
	const struct sensor_def *sensor;
	struct mode_def *sensor_mode = NULL;

	bcm_host_init();
	vcos_log_register("RaspiRaw", VCOS_LOG_CATEGORY);

	if (argc == 1)
	{
		fprintf(stdout, "\n%s Camera App %s\n\n", basename(argv[0]), VERSION_STRING);

		raspicli_display_help(cmdline_commands, cmdline_commands_size);
		exit(-1);
	}

	// Parse the command line and put options in to our status structure
	if (parse_cmdline(argc, argv, &cfg))
	{
		exit(-1);
	}

	snprintf(i2c_device_name, sizeof(i2c_device_name), "/dev/i2c-%d", cfg.i2c_bus);
	printf("Using i2C device %s\n", i2c_device_name);

	sensor = probe_sensor();
	if (!sensor)
	{
		vcos_log_error("No sensor found. Aborting");
		return -1;
	}

	if (cfg.mode >= 0 && cfg.mode < sensor->num_modes)
	{
		sensor_mode = &sensor->modes[cfg.mode];
	}

	if (!sensor_mode)
	{
		vcos_log_error("Invalid mode %d - aborting", cfg.mode);
		return -2;
	}


	if (cfg.regs)
	{
		int r,b;
		char *p,*q;

		p=strtok(cfg.regs, ";");
		while (p)
		{
			vcos_assert(strlen(p)>6);
			vcos_assert(p[4]==',');
			vcos_assert(strlen(p)%2);
			p[4]='\0'; q=p+5;
			sscanf(p,"%4x",&r);
			while(*q)
			{
				vcos_assert(isxdigit(q[0]));
				vcos_assert(isxdigit(q[1]));

				sscanf(q,"%2x",&b);
				vcos_log_error("%04x: %02x",r,b);

				modReg(sensor_mode, r, 0, 7, b, EQUAL);

				++r;
				q+=2;
			}
			p=strtok(NULL,";");
		}
	}

	if (cfg.hinc >= 0)
	{
                if (!strcmp(sensor->name, "ov5647"))
		        modReg(sensor_mode, 0x3814, 0, 7, cfg.hinc, EQUAL);
	}

	if (cfg.vinc >= 0)
	{
                if (!strcmp(sensor->name, "ov5647"))
		        modReg(sensor_mode, 0x3815, 0, 7, cfg.vinc, EQUAL);
	}

	if (cfg.voinc >= 0)
	{
                if (!strcmp(sensor->name, "imx219"))
		        modReg(sensor_mode, 0x0171, 0, 2, cfg.voinc, EQUAL);
	}

	if (cfg.hoinc >= 0)
	{
                if (!strcmp(sensor->name, "imx219"))
		        modReg(sensor_mode, 0x0170, 0, 2, cfg.hoinc, EQUAL);
	}

	if (cfg.fps > 0)
	{
		int n = 1000000000 / (sensor_mode->line_time_ns * cfg.fps);
		modReg(sensor_mode, sensor->vts_reg+0, 0, 7, n>>8, EQUAL);
		modReg(sensor_mode, sensor->vts_reg+1, 0, 7, n&0xFF, EQUAL);
	}

	if (cfg.width > 0)
	{
		sensor_mode->width = cfg.width;
		modReg(sensor_mode, sensor->xos_reg + 0, 0, 3, cfg.width >> 8, EQUAL);
		modReg(sensor_mode, sensor->xos_reg + 1, 0, 7, cfg.width & 0xFF, EQUAL);
	}

	if (cfg.height > 0)
	{
		sensor_mode->height = cfg.height;
		modReg(sensor_mode, sensor->yos_reg+0, 0, 3, cfg.height >>8, EQUAL);
		modReg(sensor_mode, sensor->yos_reg+1, 0, 7, cfg.height &0xFF, EQUAL);
	}

	if (cfg.left > 0)
	{
		if (!strcmp(sensor->name, "ov5647"))
		{
			int val = cfg.left * (cfg.mode < 2 ? 1 : 1 << (cfg.mode / 2 - 1));
			modReg(sensor_mode, 0x3800, 0, 3, val >> 8, EQUAL);
			modReg(sensor_mode, 0x3801, 0, 7, val & 0xFF, EQUAL);
		}
	}

	if (cfg.top > 0)
	{
		if (!strcmp(sensor->name, "ov5647"))
		{
			int val = cfg.top * (cfg.mode < 2 ? 1 : 1 << (cfg.mode / 2 - 1));
			modReg(sensor_mode, 0x3802, 0, 3, val >> 8, EQUAL);
			modReg(sensor_mode, 0x3803, 0, 7, val & 0xFF, EQUAL);
		}
	}

	if (cfg.bin44 == 1)
	{
		if (!strcmp(sensor->name, "imx219"))
		{
			fprintf(stderr, "start\n");
			unsigned nwidth, nheight, border, end;
			// enabled 4x4 binning
			//		modReg(sensor_mode, 0x0174, 0, 7, 2, EQUAL);
			//		modReg(sensor_mode, 0x0175, 0, 7, 2, EQUAL);

			// calculate native fov x borders
			//		nwidth = cfg.width*4 * ((cfg.hoinc == 3) ? 2 : 1);
			nwidth = cfg.width * 2 * ((cfg.hoinc == 3) ? 2 : 1);
			border = (3280 - nwidth) / 2;
			end = 3280 - border - 1;

			// set x params
			modReg(sensor_mode, 0x0164, 0, 7, border >> 8, EQUAL);
			modReg(sensor_mode, 0x0165, 0, 7, border & 0xff, EQUAL);
			modReg(sensor_mode, 0x0166, 0, 7, end >> 8, EQUAL);
			modReg(sensor_mode, 0x0167, 0, 7, end & 0xff, EQUAL);

			// calculate native fov y borders
			// nheight = cfg.height*4 * ((cfg.voinc == 3) ? 2 : 1);
			nheight = cfg.height * 2 * ((cfg.voinc == 3) ? 2 : 1);
			border = (2464 - nheight) / 2;
			end = 2464 - border - 1;

			// set y params
			modReg(sensor_mode, 0x0168, 0, 7, border >> 8, EQUAL);
			modReg(sensor_mode, 0x0169, 0, 7, border & 0xff, EQUAL);
			modReg(sensor_mode, 0x016a, 0, 7, end >> 8, EQUAL);
			modReg(sensor_mode, 0x016b, 0, 7, end & 0xff, EQUAL);
			fprintf(stderr, "end\n");
		}
	}

	if (cfg.bit_depth == -1)
	{
		cfg.bit_depth = sensor_mode->native_bit_depth;
	}


	if (cfg.write_headerg && (cfg.bit_depth != sensor_mode->native_bit_depth))
	{
		// needs change after fix for https://github.com/6by9/raspiraw/issues/2
		vcos_log_error("--headerG supported for native bit depth only");
		exit(-1);
	}

	if (cfg.exposure_us != -1)
	{
		cfg.exposure = ((int64_t)cfg.exposure_us * 1000) / sensor_mode->line_time_ns;
		vcos_log_error("Setting exposure to %d from time %dus", cfg.exposure, cfg.exposure_us);
	}

	update_regs(sensor, sensor_mode, cfg.hflip, cfg.vflip, cfg.exposure, cfg.gain);
	if (sensor_mode->encoding == 0)
		encoding = order_and_bit_depth_to_encoding(sensor_mode->order, cfg.bit_depth);
	else
		encoding = sensor_mode->encoding;
	if (!encoding)
	{
		vcos_log_error("Failed to map bitdepth %d and order %d into encoding\n", cfg.bit_depth, sensor_mode->order);
		return -3;
	}
	vcos_log_error("Encoding %08X", encoding);

	MMAL_COMPONENT_T *rawcam=NULL, *isp=NULL, *render=NULL;
	MMAL_STATUS_T status;
	MMAL_PORT_T *output = NULL;
	MMAL_POOL_T *pool = NULL;
	MMAL_CONNECTION_T *rawcam_isp = NULL;
	MMAL_CONNECTION_T *isp_render = NULL;
	MMAL_PARAMETER_CAMERA_RX_CONFIG_T rx_cfg = {{MMAL_PARAMETER_CAMERA_RX_CONFIG, sizeof(rx_cfg)}};
	MMAL_PARAMETER_CAMERA_RX_TIMING_T rx_timing = {{MMAL_PARAMETER_CAMERA_RX_TIMING, sizeof(rx_timing)}};
	int i;

	bcm_host_init();
	vcos_log_register("FastRaspiRaw", VCOS_LOG_CATEGORY);
	
	// vcos_log_error("Now start thread pool...");
	if(enableCopy)
		init_thread_pool(MAX_THREADS);
	// vcos_log_error("Now start thread pool successful...");
	

	status = mmal_component_create("vc.ril.rawcam", &rawcam);
	if (status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to create rawcam");
		return -1;
	}

	status = mmal_component_create("vc.ril.isp", &isp);
	if (status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to create isp");
		goto component_destroy;
	}

	status = mmal_component_create(MMAL_COMPONENT_DEFAULT_VIDEO_RENDERER, &render);
	if (status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to create render");
		goto component_destroy;
	}

	output = rawcam->output[0];
	status = mmal_port_parameter_get(output, &rx_cfg.hdr);
	if (status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to get cfg");
		goto component_destroy;
	}
	if (sensor_mode->encoding || cfg.bit_depth == sensor_mode->native_bit_depth)
	{
		rx_cfg.unpack = MMAL_CAMERA_RX_CONFIG_UNPACK_NONE;
		rx_cfg.pack = MMAL_CAMERA_RX_CONFIG_PACK_NONE;
	}
	else
	{
		switch (sensor_mode->native_bit_depth)
		{
		case 8:
			rx_cfg.unpack = MMAL_CAMERA_RX_CONFIG_UNPACK_8;
			break;
		case 10:
			rx_cfg.unpack = MMAL_CAMERA_RX_CONFIG_UNPACK_10;
			break;
		case 12:
			rx_cfg.unpack = MMAL_CAMERA_RX_CONFIG_UNPACK_12;
			break;
		case 14:
			rx_cfg.unpack = MMAL_CAMERA_RX_CONFIG_UNPACK_16;
			break;
		case 16:
			rx_cfg.unpack = MMAL_CAMERA_RX_CONFIG_UNPACK_16;
			break;
		default:
			vcos_log_error("Unknown native bit depth %d", sensor_mode->native_bit_depth);
			rx_cfg.unpack = MMAL_CAMERA_RX_CONFIG_UNPACK_NONE;
			break;
		}
		switch (cfg.bit_depth)
		{
		case 8:
			rx_cfg.pack = MMAL_CAMERA_RX_CONFIG_PACK_8;
			break;
		case 10:
			rx_cfg.pack = MMAL_CAMERA_RX_CONFIG_PACK_RAW10;
			break;
		case 12:
			rx_cfg.pack = MMAL_CAMERA_RX_CONFIG_PACK_RAW12;
			break;
		case 14:
			rx_cfg.pack = MMAL_CAMERA_RX_CONFIG_PACK_14;
			break;
		case 16:
			rx_cfg.pack = MMAL_CAMERA_RX_CONFIG_PACK_16;
			break;
		default:
			vcos_log_error("Unknown output bit depth %d", cfg.bit_depth);
			rx_cfg.pack = MMAL_CAMERA_RX_CONFIG_UNPACK_NONE;
			break;
		}
	}
	vcos_log_error("Set pack to %d, unpack to %d", rx_cfg.unpack, rx_cfg.pack);
	if (sensor_mode->data_lanes)
		rx_cfg.data_lanes = sensor_mode->data_lanes;
	if (sensor_mode->image_id)
		rx_cfg.image_id = sensor_mode->image_id;
	status = mmal_port_parameter_set(output, &rx_cfg.hdr);
	if (status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to set cfg");
		goto component_destroy;
	}
	status = mmal_port_parameter_get(output, &rx_timing.hdr);
	if (status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to get timing");
		goto component_destroy;
	}
	if (sensor_mode->timing1)
		rx_timing.timing1 = sensor_mode->timing1;
	if (sensor_mode->timing2)
		rx_timing.timing2 = sensor_mode->timing2;
	if (sensor_mode->timing3)
		rx_timing.timing3 = sensor_mode->timing3;
	if (sensor_mode->timing4)
		rx_timing.timing4 = sensor_mode->timing4;
	if (sensor_mode->timing5)
		rx_timing.timing5 = sensor_mode->timing5;
	if (sensor_mode->term1)
		rx_timing.term1 = sensor_mode->term1;
	if (sensor_mode->term2)
		rx_timing.term2 = sensor_mode->term2;
	vcos_log_error("Timing %u/%u, %u/%u/%u, %u/%u",
		rx_timing.timing1, rx_timing.timing2,
		rx_timing.timing3, rx_timing.timing4, rx_timing.timing5,
		rx_timing.term1,  rx_timing.term2);
	status = mmal_port_parameter_set(output, &rx_timing.hdr);
	if (status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to set timing");
		goto component_destroy;
	}

	if (cfg.camera_num != -1) {
		vcos_log_error("Set camera_num to %d", cfg.camera_num);
		status = mmal_port_parameter_set_int32(output, MMAL_PARAMETER_CAMERA_NUM, cfg.camera_num);
		if (status != MMAL_SUCCESS)
		{
			vcos_log_error("Failed to set camera_num");
			goto component_destroy;
		}
	}

	status = mmal_component_enable(rawcam);
	if (status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to enable rawcam");
		goto component_destroy;
	}
	status = mmal_component_enable(isp);
	if (status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to enable isp");
		goto component_destroy;
	}
	status = mmal_component_enable(render);
	if (status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to enable render");
		goto component_destroy;
	}

	output->format->es->video.crop.width = sensor_mode->width;
	output->format->es->video.crop.height = sensor_mode->height;
	output->format->es->video.width = VCOS_ALIGN_UP(sensor_mode->width, 16);
	output->format->es->video.height = VCOS_ALIGN_UP(sensor_mode->height, 16);
	output->format->encoding = encoding;

	status = mmal_port_format_commit(output);
	if (status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed port_format_commit");
		goto component_disable;
	}

	output->buffer_size = output->buffer_size_recommended;
	output->buffer_num = BUFFER_NUM_MANUAL ? BUFFER_NUM_MANUAL : output->buffer_num_recommended;
	// output->buffer_num = output->buffer_num_recommended;

	if (cfg.capture)
	{
		if (cfg.write_header || cfg.write_header0)
		{
			brcm_header = (struct brcm_raw_header*)malloc(BRCM_RAW_HEADER_LENGTH);
			if (brcm_header)
			{
				memset(brcm_header, 0, BRCM_RAW_HEADER_LENGTH);
				brcm_header->id = BRCM_ID_SIG;
				brcm_header->version = HEADER_VERSION;
				brcm_header->mode.width = sensor_mode->width;
				brcm_header->mode.height = sensor_mode->height;
				// FIXME: Ought to check that the sensor is producing
				// Bayer rather than just assuming.
				brcm_header->mode.format = VC_IMAGE_BAYER;
				switch(sensor_mode->order)
				{
					case BAYER_ORDER_BGGR:
						brcm_header->mode.bayer_order = VC_IMAGE_BAYER_BGGR;
						break;
					case BAYER_ORDER_GBRG:
						brcm_header->mode.bayer_order = VC_IMAGE_BAYER_GBRG;
						break;
					case BAYER_ORDER_GRBG:
						brcm_header->mode.bayer_order = VC_IMAGE_BAYER_GRBG;
						break;
					case BAYER_ORDER_RGGB:
						brcm_header->mode.bayer_order = VC_IMAGE_BAYER_RGGB;
						break;
				}
				switch(cfg.bit_depth)
				{
					case 8:
						brcm_header->mode.bayer_format = VC_IMAGE_BAYER_RAW8;
						break;
					case 10:
						brcm_header->mode.bayer_format = VC_IMAGE_BAYER_RAW10;
						break;
					case 12:
						brcm_header->mode.bayer_format = VC_IMAGE_BAYER_RAW12;
						break;
					case 14:
						brcm_header->mode.bayer_format = VC_IMAGE_BAYER_RAW14;
						break;
					case 16:
						brcm_header->mode.bayer_format = VC_IMAGE_BAYER_RAW16;
						break;
				}
				if (cfg.write_header0)
				{
					// Save bcrm_header into one file only
					FILE *file;
					file = fopen(cfg.write_header0, "wb");
					if (file)
					{
						fwrite(brcm_header, BRCM_RAW_HEADER_LENGTH, 1, file);
						fclose(file);
					}
				}
			}
		}
		else if (cfg.write_headerg)
		{
			// Save pgm_header into one file only
			FILE *file;
			file = fopen(cfg.write_headerg, "wb");
			if (file)
			{
				fprintf(file, "P5\n%d %d\n255\n", sensor_mode->width, sensor_mode->height);
				fclose(file);
			}
		}

		status = mmal_port_parameter_set_boolean(output, MMAL_PARAMETER_ZERO_COPY, MMAL_TRUE);
		if (status != MMAL_SUCCESS)
		{
			vcos_log_error("Failed to set zero copy");
			goto component_disable;
		}

		vcos_log_error("Create pool of %d buffers of size %d", output->buffer_num, output->buffer_size);
		pool = mmal_port_pool_create(output, output->buffer_num, output->buffer_size);
		if (!pool)
		{
			vcos_log_error("Failed to create pool");
			goto component_disable;
		}

		output->userdata = (struct MMAL_PORT_USERDATA_T *)&cfg;
		status = mmal_port_enable(output, callback);
		if (status != MMAL_SUCCESS)
		{
			vcos_log_error("Failed to enable port");
			goto pool_destroy;
		}
		running = 1;
		for(i = 0; i<output->buffer_num; i++)
		{
			MMAL_BUFFER_HEADER_T *buffer = mmal_queue_get(pool->queue);

			if (!buffer)
			{
				vcos_log_error("Where'd my buffer go?!");
				goto port_disable;
			}
			status = mmal_port_send_buffer(output, buffer);
			if (status != MMAL_SUCCESS)
			{
				vcos_log_error("mmal_port_send_buffer failed on buffer %p, status %d", buffer, status);
				goto port_disable;
			}
			vcos_log_error("Sent buffer %p", buffer);
		}
	}
	else
	{
		status = mmal_connection_create(&rawcam_isp, output, isp->input[0], MMAL_CONNECTION_FLAG_TUNNELLING);
		if (status != MMAL_SUCCESS)
		{
			vcos_log_error("Failed to create rawcam->isp connection");
			goto pool_destroy;
		}

		MMAL_PORT_T *port = isp->output[0];
		port->format->es->video.crop.width = sensor_mode->width;
		port->format->es->video.crop.height = sensor_mode->height;
		if (port->format->es->video.crop.width > 1920)
		{
			// Display can only go up to a certain resolution before underflowing
			port->format->es->video.crop.width /= 2;
			port->format->es->video.crop.height /= 2;
		}
		port->format->es->video.width = VCOS_ALIGN_UP(port->format->es->video.crop.width, 32);
		port->format->es->video.height = VCOS_ALIGN_UP(port->format->es->video.crop.height, 16);
		port->format->encoding = MMAL_ENCODING_I420;
		status = mmal_port_format_commit(port);
		if (status != MMAL_SUCCESS)
		{
			vcos_log_error("Failed to commit port format on isp output");
			goto pool_destroy;
		}

		if (sensor_mode->black_level)
		{
			status = mmal_port_parameter_set_uint32(isp->input[0], MMAL_PARAMETER_BLACK_LEVEL, sensor_mode->black_level);
			if (status != MMAL_SUCCESS)
			{
				vcos_log_error("Failed to set black level - try updating firmware");
			}
		}

		if (cfg.awb_gains_r && cfg.awb_gains_b)
		{
			MMAL_PARAMETER_AWB_GAINS_T param = {{MMAL_PARAMETER_CUSTOM_AWB_GAINS, sizeof(param)}, {0, 0}, {0, 0}};

			param.r_gain.num = (unsigned int)(cfg.awb_gains_r * 65536);
			param.b_gain.num = (unsigned int)(cfg.awb_gains_b * 65536);
			param.r_gain.den = param.b_gain.den = 65536;
			status = mmal_port_parameter_set(isp->input[0], &param.hdr);
			if (status != MMAL_SUCCESS)
			{
				vcos_log_error("Failed to set white balance");
			}
		}

		status = mmal_connection_create(&isp_render, isp->output[0], render->input[0], MMAL_CONNECTION_FLAG_TUNNELLING);
		if (status != MMAL_SUCCESS)
		{
			vcos_log_error("Failed to create isp->render connection");
			goto pool_destroy;
		}

		status = mmal_connection_enable(rawcam_isp);
		if (status != MMAL_SUCCESS)
		{
			vcos_log_error("Failed to enable rawcam->isp connection");
			goto pool_destroy;
		}
		status = mmal_connection_enable(isp_render);
		if (status != MMAL_SUCCESS)
		{
			vcos_log_error("Failed to enable isp->render connection");
			goto pool_destroy;
		}
	}

	start_camera_streaming(sensor, sensor_mode);

	vcos_sleep(cfg.timeout);
	running = 0;

	stop_camera_streaming(sensor);

port_disable:
	if (cfg.capture)
	{
		status = mmal_port_disable(output);
		if (status != MMAL_SUCCESS)
		{
			vcos_log_error("Failed to disable port");
			return -1;
		}
	}
pool_destroy:
	if (pool)
		mmal_port_pool_destroy(output, pool);
	if (isp_render)
	{
		mmal_connection_disable(isp_render);
		mmal_connection_destroy(isp_render);
	}
	if (rawcam_isp)
	{
		mmal_connection_disable(rawcam_isp);
		mmal_connection_destroy(rawcam_isp);
	}
component_disable:
	if (brcm_header)
		free(brcm_header);
	status = mmal_component_disable(render);
	if (status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to disable render");
	}
	status = mmal_component_disable(isp);
	if (status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to disable isp");
	}
	status = mmal_component_disable(rawcam);
	if (status != MMAL_SUCCESS)
	{
		vcos_log_error("Failed to disable rawcam");
	}
component_destroy:
	if (rawcam)
		mmal_component_destroy(rawcam);
	if (isp)
		mmal_component_destroy(isp);
	if (render)
		mmal_component_destroy(render);

	if (cfg.write_timestamps)
	{
		// FIXME
		// Save timestamps


		PTS_NODE_T aux;
		// Rough estimation of the line size
		size_t file_sz = (cfg.timeout / cfg.saverate) << 4;

		int fd = open(cfg.write_timestamps, O_RDWR | O_CREAT | O_TRUNC, 0644);
		if(fd == -1){
			perror("Error opening file");
			return -1;
		}
		if(ftruncate(fd, file_sz) == -1){
			perror("Error setting file size");
			close(fd);
			return -1;
		}
		void* mapped_mem = mmap(NULL, file_sz, PROT_WRITE, MAP_SHARED, fd, 0);
		if (mapped_mem == MAP_FAILED){
			perror("Error mapping file");
			close(fd);
			return -1;
		}

		uint64_t old = 0;
		char *write_ptr = (char *)mapped_mem;
		for(aux = cfg.ptsa; aux != cfg.ptso; aux = aux->nxt)
		{
			if (aux == cfg.ptsa)
				write_ptr += sprintf(write_ptr, ",%d,%lld\n", aux->idx, aux->pts);
			else
				write_ptr += sprintf(write_ptr, "%lld,%d,%lld\n", aux->pts-old, aux->idx, aux->pts);
			old = aux->pts;
		}

		// // Clean up memory-mapped region and file
		// if (msync(mapped_mem, file_sz, MS_SYNC) == -1) {
		// 	perror("Error syncing file");
		// }

		munmap(mapped_mem, file_sz);
		close(fd);

		while (cfg.ptsa != cfg.ptso)
		{
			aux = cfg.ptsa->nxt;
			free(cfg.ptsa);
			cfg.ptsa = aux;
		}
		free(cfg.ptso);
	}

	vcos_log_error("Now stop thread pool...");
	if(enableCopy){
		// sem_post(&producer_stop_sem);
		dstr_thread_pool(MAX_THREADS);
	}
	vcos_log_error("Now stop thread pool successful...");

	return 0;
}
