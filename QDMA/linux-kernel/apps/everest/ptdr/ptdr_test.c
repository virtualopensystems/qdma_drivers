/*
 * Copyright (C) 2023 - Virtual Open Systems SAS - All rights reserved.
 * Reproduction and communication of this document is strictly prohibited
 * unless specifically authorized in writing by Virtual Open Systems.
 * ****************************************************************************
 * File Name   : ptdr_test.c
 * Author	   : STEFANO CIRICI <s.cirici@virtualopensystems.com>
 * Description :
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#include "dmautils.h"
#include "qdma_queues.h"
#include "ptdr_dev.h"
#include "version.h"

#ifndef SAMPLES_COUNT
#define SAMPLES_COUNT 10
#endif

#ifndef ITERATIONS_COUNT
#define ITERATIONS_COUNT 1
#endif

#define KERN_PCI_BUS		(0x0083)
#define KERN_PCI_VF_BUS		(0x0007)
#define KERN_PCI_DEV		(0x00)
#define KERN_FUN_ID			(0x00)
#define KERN_IS_VF			(0x00)
#define KERN_Q_START		(0)
#define VF_NUM_MAX			(252) // Max num of VF allowed by QDMA

/* ptdrXHBM.bit */
#define MEM_IN_BASE_ADDR	(0x0000000000000000ULL) // input @ 0
#ifdef HBM16GB //up to 16 GB HBM memory on u55c
#pragma message "HBM set to 16 GB"
#define MEM_OUT_BASE_ADDR	(0x0000000200000000ULL) // output @ 8GB offset
#else
#define MEM_OUT_BASE_ADDR	(0x0000000100000000ULL) // output @ 4GB offset
#endif
#define KERN_BASE_ADDR		(0x0000000400000000ULL) // kernels starts after 16 GB of HBM
#define KERN_VF_INCR		(0x0000000000010000ULL) // kernels offset

#define ROUND_UP(num, pow)	( (num + (pow-1)) & (~(pow-1)) )
#define MEM_IN_SIZE			( 6889080 )
//#define MEM_IN_SIZE			( (121+1331+1331)*(sizeof(double)) )
//#define MEM_OUT_SIZE		( (1331)*(sizeof(double)) )

#define TIMEOUT_COUNT_MS	(300*1000) //5 min

#ifndef DEBUG
#define ptdr_reg_dump(x) (x)
#define ptdr_ctrl_dump(x) (x)
#endif

#define info_print(fmt, ...) \
	do { \
		if (!quiet_flag) { \
			printf(fmt, ##__VA_ARGS__); \
		} \
	} while (0)

#define ERR_CHECK(err) \
	do { \
		if (err < 0) { \
			fprintf(stderr, "Error %d\n", err); \
			ptdr_dev_destroy(kern); \
			exit(-err); \
		} \
	} while (0)

static void * kern;
static int quiet_flag = 0;

static uint64_t kern_addr		= KERN_BASE_ADDR;
static uint64_t hbm_addr		= MEM_IN_BASE_ADDR;
//static uint64_t mem_out_addr	= MEM_OUT_BASE_ADDR;
static int kern_pci_bus			= KERN_PCI_BUS;
static int kern_pci_dev			= KERN_PCI_DEV;
static int kern_pci_id			= KERN_FUN_ID;
static int is_vf				= KERN_IS_VF;


void intHandler(int sig) {
	char c;
	int ret;

	signal(sig, SIG_IGN);

	printf("\nDo you really want to quit? [y/n] ");
	c = getchar();
	if (c == 'y' || c == 'Y') {
		if (kern != NULL) {
			info_print("\nDestroying kernel\n");
			ret = ptdr_dev_destroy(kern);
			ERR_CHECK(ret);
		}
		exit(0);
	}
	signal(sig, intHandler);
}


static int mem_read_to_buffer(uint64_t addr, uint64_t size, char** buffer)
{
	int ret;
	struct queue_info *q_info;
	struct queue_conf q_conf;

	q_conf.pci_bus = kern_pci_bus;
	q_conf.pci_dev = kern_pci_dev;
	q_conf.fun_id = kern_pci_id;
	q_conf.is_vf = is_vf;
	q_conf.q_start = KERN_Q_START + 1; //use a different queue id

	ret = queue_setup(&q_info, &q_conf);
	if (ret < 0) {
		return ret;
	}

	*buffer = (char*) calloc(1, sizeof(char) * size);
	if (*buffer == NULL) {
		fprintf(stderr, "Error allocating %ld bytes\n", size);
		*buffer = NULL;
		queue_destroy(q_info);
		return -ENOMEM;
	}

	info_print("Reading 0x%02lx (%ld) bytes @ 0x%016lx\n", size, size, addr);
	size_t rsize = queue_read(q_info, *buffer, size, addr);

	if (rsize != size){
		fprintf(stderr, "Error: read %ld bytes instead of %ld\n", rsize, size);
		free(*buffer);
		queue_destroy(q_info);
		return -EIO;
	}

	ret = queue_destroy(q_info);
	return ret;
}

static int mem_write_from_buffer(uint64_t addr, char* buffer, size_t size)
{
	int ret;
	struct queue_info *q_info;
	struct queue_conf q_conf;

	q_conf.pci_bus = kern_pci_bus;
	q_conf.pci_dev = kern_pci_dev;
	q_conf.fun_id = kern_pci_id;
	q_conf.is_vf = is_vf;
	q_conf.q_start = KERN_Q_START + 1; //use a different queue id

	ret = queue_setup(&q_info, &q_conf);
	if (ret < 0) {
		return ret;
	}

	info_print("Writing 0x%02lx (%ld) bytes @ 0x%016lx\n", size, size, addr);
	size_t wsize = queue_write(q_info, buffer, size, addr);

	if (wsize != size) {
		fprintf(stderr, "Error: written %ld bytes instead of %ld\n", wsize, size);
		queue_destroy(q_info);
		return -EIO;
	}

	ret = queue_destroy(q_info);
	return ret;
}


int write_buffer_into_file(const char* filename, const char* buffer, size_t buffer_size)
{
	FILE* file = fopen(filename, "wb");

	if (file == NULL) {
		fprintf(stderr, "ERR %d: Failed opening file \"%s\"\n", errno, filename);
		return -errno;
	}

	info_print("Writing 0x%02lx (%ld) bytes to \"%s\"\n", buffer_size, buffer_size, filename);
	size_t wsize = fwrite(buffer, 1, buffer_size, file);

	if (wsize != buffer_size) {
		fprintf(stderr, "Error: written %ld bytes instead of %ld\n", wsize, buffer_size);
		fclose(file);
		return -EIO;
	}

	fclose(file);
	return 0;
}

int read_file_into_buffer(const char* filename, char** buffer, size_t* buffer_size)
{
	FILE* file = fopen(filename, "rb");
	size_t size = 0;
	char* data = NULL;

	if (file == NULL) {
		fprintf(stderr, "ERR %d: Failed opening file \"%s\"\n", errno, filename);
		return -errno;
		return -ENOENT;
	}

	if (fseek(file, 0L, SEEK_END) < 0) {
		fprintf(stderr, "ERR %d: Failed fseek on file \"%s\"\n", errno, filename);
		return -errno;
	}
	size = ftell(file);
	if (size < 0) {
		fprintf(stderr, "ERR %d: Failed ftell on file \"%s\"\n", errno, filename);
		return -errno;
	}
	if (fseek(file, 0L, SEEK_SET) < 0) {
		fprintf(stderr, "ERR %d: Failed fseek on file \"%s\"\n", errno, filename);
		return -errno;
	}

	data = (char*) malloc(size);
	if (data == NULL) {
		fprintf(stderr, "Error allocating %ld bytes\n", size);
		fclose(file);
		return -ENOMEM;
	}

	info_print("Reading 0x%02lx (%ld) bytes from \"%s\"\n", size, size, filename);
	size_t rsize = fread(data, 1, size, file);

	if (rsize != size) {
		fprintf(stderr, "Error: read %ld bytes instead of %ld\n", rsize, size);
		fclose(file);
		free(data);
		return -EIO;
	}

	fclose(file);

	*buffer_size = size;
	*buffer = data;

	return 0;
}

static void print_usage(char*argv[])
{
	printf("EVEREST PTDR kernel test\n");
	printf("Usage: %s [OPTION]...\n", argv[0]);
	printf("  -i FILE        specify input FILE\n");
	printf("  -o FILE        specify output FILE\n");
	printf("  -v vf_num      specify VF number (-1 to use PF, default is -1)\n");
	printf("  -d device_id   specify device BDF (defaults to %04x:%02x.%01x for PF and %04x:%02x.%01x for VF)\n",
			KERN_PCI_BUS, KERN_PCI_DEV, KERN_FUN_ID, KERN_PCI_VF_BUS, KERN_PCI_DEV, KERN_FUN_ID);
	printf("  -q             quiet output\n");
	printf("  -h             display this help and exit\n");
}

int main(int argc, char *argv[])
{
	int ret, opt;
	int count;
	struct timespec ts = {0, 1000*1000}; //1msec
	char *input_filename = NULL;
	char *output_filename = NULL;
	int vf_num = -1;
	uint64_t bdf = 0xFFFFFFFF;

	signal(SIGINT, intHandler); // Register interrupt handler on CTRL-c

	// Parse command line
	while ((opt = getopt(argc, argv, "hi:o:v:d:q")) != -1) {
		switch (opt) {
			case 'h':
				print_usage(argv);
				exit(EXIT_SUCCESS);
			case 'i':
				input_filename = optarg;
				break;
			case 'o':
				output_filename = optarg;
				break;
			case 'v':
				vf_num = strtol(optarg, NULL, 0);
				break;
			case 'd':
				bdf = strtol(optarg, NULL, 16);
				break;
			case 'q':
				quiet_flag = 1;
				break;
			case '?':
				/* Error message printed by getopt */
				exit(EXIT_FAILURE);
			default:
				/* Should never get here */
				abort();
		}
	}


	if (input_filename == NULL || output_filename == NULL) {
		printf("Invalid input or output file names!\n");
		exit(EXIT_FAILURE);
	}

	// Parse VF option
	if (vf_num == -1) {
		info_print("PF mode:\n");
	}
	else if (vf_num < 0 || vf_num > VF_NUM_MAX) {
		printf("Invalid vf_num %d (max is %d)\n", vf_num, VF_NUM_MAX);
		exit(EXIT_FAILURE);
	} else {
		is_vf = 1; //Activate VF mode
		// Addresses depends on VF num
		kern_addr = KERN_BASE_ADDR + KERN_VF_INCR * vf_num;
		hbm_addr = MEM_IN_BASE_ADDR + ROUND_UP(MEM_IN_SIZE,4096) * vf_num;
		//mem_out_addr = MEM_OUT_BASE_ADDR + ROUND_UP(MEM_OUT_SIZE,4096) * vf_num;
		kern_pci_bus = KERN_PCI_VF_BUS;
		kern_pci_dev = KERN_PCI_DEV;
		kern_pci_id = KERN_FUN_ID;
		info_print("VF mode: VF num %d\n", vf_num);
	}

	// Parse BDF option
	if (bdf < 0x0FFFFFFF) {
		kern_pci_bus = (bdf >> 12) & 0x0FFFF;
		kern_pci_dev = (bdf >> 4) & 0x0FF;
		kern_pci_id = bdf & 0x0F;
	}

	info_print("    MEM IN   0x%016lx - 0x%016lx\n", hbm_addr, hbm_addr+MEM_IN_SIZE);
	//info_print("    MEM OUT  0x%016lx - 0x%016lx\n", mem_out_addr, mem_out_addr+MEM_OUT_SIZE);
	info_print("    Kern PCI %04x:%02x.%01x\n", kern_pci_bus, kern_pci_dev, kern_pci_id);
	info_print("    Samples %d, iterations %d\n", SAMPLES_COUNT, ITERATIONS_COUNT);


	info_print("\nInitializing kernel @ 0x%016lx\n", kern_addr);


	kern = ptdr_dev_init(kern_addr, kern_pci_bus, kern_pci_dev,
				kern_pci_id, is_vf, KERN_Q_START);

	if (kern == NULL) {
		printf("Error during init!\n");
		exit(EXIT_FAILURE);
	}
	info_print("Kernel initialized correctly!\n");


    //std::vector<DurationSamples_t> dur_profiles( ITERATIONS_COUNT, DurationSamples_t(SAMPLES_COUNT, 0));
	ptdr_duration_t dur_profiles[ITERATIONS_COUNT][SAMPLES_COUNT];
    //ptdr::route::Route route = ptdr::route::ReadFromFile(input_file_path);
	ptdr_route_t route;
    ptdr_routepos_t start_pos = {0, 0.0};
    ptdr_datetime_t departure_time = 1623823200ULL * 1000; // "2021-06-16 08:00:00"
    ptdr_seed_t seed = 0xABCDE23456789;

    size_t dur_size = sizeof(dur_profiles[0]);
    size_t route_size; //sizeof(route);
    size_t pos_size = sizeof(start_pos);
    size_t dep_size = sizeof(departure_time);
    size_t seed_size = sizeof(seed);


	{
		char *buff;
		size_t buff_size = 0;

		ret = read_file_into_buffer(input_filename, (void*) &buff, &buff_size);
		ERR_CHECK(ret);

		info_print("HERE\n");

		ret = ptdr_route_parse(buff, buff_size, &route, &route_size);
		ERR_CHECK(ret);

		free (buff);
	}

    size_t total_size = dur_size + route_size + pos_size + dep_size + seed_size;

    info_print("\ndur_size %ld\n", dur_size);
    info_print("route_size %ld\n", route_size);
    info_print("pos_size %ld\n", pos_size);
    info_print("dep_size %ld\n", dep_size);
    info_print("seed_size %ld\n", seed_size);
    info_print("total_size %ld\n\n", total_size);


    unsigned int durations_ptr = 0;
    unsigned int route_ptr = durations_ptr + dur_size;
    unsigned int position_ptr = route_ptr + route_size;
    unsigned int departure_ptr = position_ptr + pos_size;
    unsigned int seed_ptr = departure_ptr + dep_size;


    // Allocate space for the total amount
    char* mem_dat = (char*)malloc(total_size);
	if (mem_dat == NULL) {
		fprintf(stderr, "Error allocating %ld bytes\n", total_size);
		return -ENOMEM;
	}

    // Copy data into this unified space
    memcpy(&mem_dat[durations_ptr], (const char*) (&dur_profiles[0]),    dur_size);
    memcpy(&mem_dat[route_ptr],     (const char*) (&route),              route_size);
    memcpy(&mem_dat[position_ptr],  (const char*) (&start_pos),          pos_size);
    memcpy(&mem_dat[departure_ptr], (const char*) (&departure_time),     dep_size);
    memcpy(&mem_dat[seed_ptr],      (const char*) (&seed),               seed_size);


	//Write inputs from input file into FPGA memory
	info_print("Write inputs to FPGA HBM mem @0x%016lx size %ld\n", hbm_addr, total_size);
	ret = mem_write_from_buffer(hbm_addr, mem_dat, total_size);
	ERR_CHECK(ret);


	info_print("Setting num times to 1\n");
	ret = ptdr_set_numtimes(kern, 1);
	ERR_CHECK(ret);

	info_print("Setting duration ptr to %u\n", durations_ptr);
	ret = ptdr_set_durations(kern, durations_ptr);
	ERR_CHECK(ret);

	info_print("Setting route ptr to %u\n", route_ptr);
	ret = ptdr_set_route(kern, route_ptr);
	ERR_CHECK(ret);

	info_print("Setting position ptr to %u\n", position_ptr);
	ret = ptdr_set_position(kern, position_ptr);
	ERR_CHECK(ret);

	info_print("Setting departure ptr to %u\n", departure_ptr);
	ret = ptdr_set_departure(kern, departure_ptr);
	ERR_CHECK(ret);

	info_print("Setting seed ptr to %u\n", seed_ptr);
	ret = ptdr_set_seed(kern, seed_ptr);
	ERR_CHECK(ret);

	info_print("Setting base to 0x%016lx\n", hbm_addr);
	ret = ptdr_set_base(kern, hbm_addr);
	ERR_CHECK(ret);

	info_print("Setting autorestart to 0\n");
	ret = ptdr_autorestart(kern, 0);
	ERR_CHECK(ret);

	info_print("Setting interruptglobal to 0\n");
	ret = ptdr_interruptglobal(kern, 0);
	ERR_CHECK(ret);

	info_print("Kernel is ready %d\n", ptdr_isready(kern));
	info_print("Kernel is idle %d\n", ptdr_isidle(kern));

	(void) ptdr_reg_dump(kern);


	info_print("\nWaiting for kernel to be ready\n");
	count = TIMEOUT_COUNT_MS;
	while ((ptdr_isready(kern) == 0) && (--count != 0)) {
		nanosleep(&ts, NULL); // sleep 1ms
		if ((count % 1000) == 0) { // Print "." every sec
			info_print(" ."); fflush(stdout);
		}
	}
	if (count == 0) {
		info_print("\nTIMEOUT reached\n\n");
		ERR_CHECK(-EAGAIN);
	}
	(void) ptdr_ctrl_dump(kern);


	info_print("Starting kernel operations\n");
	ret = ptdr_start(kern);
	if (ptdr_isdone(kern)) {
		// If this is not the first operation, the done bit will remain high.
		// To start again the procedure, we must also set the continue bit
		ptdr_continue(kern);
	}
	ERR_CHECK(ret);
	//(void) ptdr_ctrl_dump(kern); //commented to avoid altering clear on read registers


	info_print("Waiting for kernel to finish\n");
	count = TIMEOUT_COUNT_MS;
	while ( !(ptdr_isdone(kern) || ptdr_isidle(kern)) && (--count != 0)) {
		nanosleep(&ts, NULL); // sleep 1ms
		if ((count % 1000) == 0) { // Print "." every sec
			info_print(" ."); fflush(stdout);
		}
	}

	(void) ptdr_ctrl_dump(kern);

	if (count == 0) {
		info_print("\nTIMEOUT reached\n\n");
		ERR_CHECK(-EAGAIN);
	} else {
		info_print("FINISHED!\n\n");
	}


	// Read FPGA out mem into buffer and write the buffer into	output file
	{
		char *buff;
		//std::memcpy(reinterpret_cast<char *>(&dur_profiles.at(0)), &mem_dat[durations_ptr], dur_size);
		ret = mem_read_to_buffer(hbm_addr+durations_ptr, dur_size, &buff);
		//ret = mem_read_to_buffer(hbm_addr, total_size, &buff);
		ERR_CHECK(ret);
		ret = write_buffer_into_file(output_filename, buff, dur_size);
		//ret = write_buffer_into_file(output_filename, buff, total_size);
		ERR_CHECK(ret);
		free(buff);
	}

	info_print("\nDestroying kernel\n");
	ret = ptdr_dev_destroy(kern);
	ERR_CHECK(ret);

	exit(EXIT_SUCCESS);
}

