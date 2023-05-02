/*
 * Copyright (C) 2023 - Virtual Open Systems SAS - All rights reserved.
 * Reproduction and communication of this document is strictly prohibited
 * unless specifically authorized in writing by Virtual Open Systems.
 * ****************************************************************************
 * File Name   : helm_api.c
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

#include "dmautils.h"
#include "qdma_queues.h"
#include "helm_dev.h"
#include "version.h"


#define KERN_PCI_BUS	(0x0083)
#define KERN_PCI_VF_BUS	(0x0007)
#define KERN_PCI_DEV	(0x00)
#define KERN_FUN_ID		(0x00)
#define KERN_IS_VF		(0x00)
#define KERN_Q_START	(0)
#define VF_NUM_MAX		(252) // Max num of VF allowed by QDMA

/* helm4.bit */
#define KERN_BASE_ADDR		(0x10000000)
#define KERN_VF_INCR		(0x00010000)
#define MEM_IN_BASE_ADDR	(0x00000000) //4MB total
#define MEM_OUT_BASE_ADDR	(0x002A0000) //1/3 only for output

#define ROUND_UP(num, pow)  ( (num + (pow-1)) & (~(pow-1)) )
#define MEM_IN_SIZE		((121+1331+1331)*(sizeof(double)))
#define MEM_OUT_SIZE	((1331)*(sizeof(double)))


#ifndef DEBUG
#define helm_reg_dump(x) (x)
#define helm_ctrl_dump(x) (x)
#endif

static uint64_t kern_addr		= KERN_BASE_ADDR;
static uint64_t mem_in_addr		= MEM_IN_BASE_ADDR;
static uint64_t mem_out_addr	= MEM_OUT_BASE_ADDR;
static int kern_pci_bus			= KERN_PCI_BUS;
static int kern_pci_dev			= KERN_PCI_DEV;
static int kern_pci_id			= KERN_FUN_ID;
static int is_vf				= KERN_IS_VF;

#define ERR_CHECK(err) do { \
		if (err < 0) { \
			fprintf(stderr, "Error %d\n", err); \
			goto error; \
		} \
	} while (0)

int mem_read_to_buffer(uint64_t addr, uint64_t size, char** buffer)
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
		fprintf(stderr, "Error allocating %d bytes\n", size);
		*buffer = NULL;
		queue_destroy(q_info);
		return -ENOMEM;
	}

	printf("Reading 0x%02x (%d) bytes @ 0x%08x\n", size, size, addr);
	int rsize = queue_read(q_info, *buffer, size, addr);

	if (rsize != size){
		fprintf(stderr, "Error: read %d bytes instead of %d\n", rsize, size);
		free(*buffer);
		queue_destroy(q_info);
		return -EIO;
	}

	ret = queue_destroy(q_info);
	return ret;
}

int mem_write_from_buffer(uint64_t addr, char* buffer, size_t size)
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

	printf("Writing 0x%02x (%d) bytes @ 0x%08x\n", size, size, addr);
	int wsize = queue_write(q_info, buffer, size, addr);

	if (wsize != size) {
		fprintf(stderr, "Error: written %d bytes instead of %d\n", wsize, size);
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
		fprintf(stderr, "Failed opening file \"%s\"\n", filename);
		return -ENOENT;
	}

	printf("Writing 0x%02x (%d) bytes to \"%s\"\n", buffer_size, buffer_size, filename);
	size_t wsize = fwrite(buffer, 1, buffer_size, file);

	if (wsize != buffer_size) {
		fprintf(stderr, "Error: written %d bytes instead of %d\n", wsize, buffer_size);
		fclose(file);
		return -EIO;
	}

	fclose(file);
	return 0;
}

int read_file_into_buffer(const char* filename, char** buffer, size_t* buffer_size)
{
	FILE* file = fopen(filename, "rb");

	if (file == NULL) {
		fprintf(stderr, "Failed opening file \"%s\"\n", filename);
		*buffer = NULL;
		*buffer_size = 0;
		return -ENOENT;
	}

	fseek(file, 0L, SEEK_END);
	*buffer_size = ftell(file);
	fseek(file, 0L, SEEK_SET);

	*buffer = (char*) malloc(*buffer_size);
	if (*buffer == NULL) {
		fprintf(stderr, "Error allocating %d bytes\n", *buffer_size);
		fclose(file);
		*buffer = NULL;
		*buffer_size = 0;
		return -ENOMEM;
	}

	printf("Reading 0x%02x (%d) bytes from \"%s\"\n", *buffer_size, *buffer_size, filename);
	size_t rsize = fread(*buffer, 1, *buffer_size, file);

	if (rsize != *buffer_size) {
		fprintf(stderr, "Error: read %d bytes instead of %d\n", rsize, *buffer_size);
		fclose(file);
		free(*buffer);
		*buffer = NULL;
		*buffer_size = 0;
		return -EIO;
	}

	fclose(file);
	return 0;
}

int main(int argc, char *argv[])
{
	int ret;
	struct timespec ts = {0, 1000*1000}; //1msec
	int count;

	if (argc < 3 || argc > 5) {
		printf("Usage: %s <infile> <outfile> [vf_num] [PCI BUS]\n", argv[0]);
		return -1;
	}

	char *infile_name = argv[1];
	char *outfile_name = argv[2];


	// Parse VF number
	if (argc >= 4) {
		int vf_num = strtol(argv[3], NULL, 0);
		if (vf_num < 0 || vf_num > VF_NUM_MAX) {
			printf("Invalid vf_num %d (max is %d)\n", vf_num, VF_NUM_MAX);
			return -1;
		}
		// Addresses depends on VF num
		kern_addr = KERN_BASE_ADDR + KERN_VF_INCR * vf_num;
		mem_in_addr = MEM_IN_BASE_ADDR + ROUND_UP(MEM_IN_SIZE,4096) * vf_num;
		mem_out_addr = MEM_OUT_BASE_ADDR + ROUND_UP(MEM_OUT_SIZE,4096) * vf_num;
		kern_pci_bus = KERN_PCI_VF_BUS;
		kern_pci_dev = KERN_PCI_DEV;
		kern_pci_id = KERN_FUN_ID;
		is_vf = 1;
		printf("VF mode: VF num %d\n", vf_num);
	} else {
		printf("PF mode:\n");
	}

	if (argc == 5) {
		uint64_t bdf = strtol(argv[4], NULL, 16);
		kern_pci_bus = (bdf >> 12) & 0x0FFFF;
		kern_pci_dev = (bdf >> 4) & 0x0FF;
		kern_pci_id = bdf & 0x0F;
	}

	printf("         MEM IN   0x%08x - 0x%08x\n", mem_in_addr, mem_in_addr+MEM_IN_SIZE);
	printf("         MEM OUT  0x%08x - 0x%08x\n", mem_out_addr, mem_out_addr+MEM_OUT_SIZE);
	printf("         Kern PCI %04x:%02x.%01x\n\n", kern_pci_bus, kern_pci_dev, kern_pci_id);


	printf("Initializing kernel @ 0x%08x\n", kern_addr);
	return 0;

	void * kern;
	kern = helm_dev_init(kern_addr, kern_pci_bus, kern_pci_dev,
				kern_pci_id, is_vf, KERN_Q_START);

	if (kern == NULL) {
		fprintf(stderr, "Error during init!\n");
		return -1;
	}
	printf("Kernel initialized correctly!\n");

	printf("Setting memory in addr  @ 0x%08x\n", mem_in_addr);
	ret = helm_set_in(kern, mem_in_addr);
	ERR_CHECK(ret);

	printf("Setting memory out addr @ 0x%08x\n", mem_out_addr);
	ret = helm_set_out(kern, mem_out_addr);
	ERR_CHECK(ret);

	printf("Setting num times to 1\n");
	ret = helm_set_numtimes(kern, 1);
	ERR_CHECK(ret);

	printf("Setting autorestart to 0\n");
	ret = helm_autorestart(kern, 0);
	ERR_CHECK(ret);

	printf("Setting interruptglobal to 0\n");
	ret = helm_interruptglobal(kern, 0);
	ERR_CHECK(ret);

	printf("Kernel is ready %d\n", helm_isready(kern));
	printf("Kernel is idle %d\n", helm_isidle(kern));


	(void) helm_reg_dump(kern);


	//Write inputs from input file into FPGA memory
	printf("\nWrite inputs to FPGA IN mem\n");
	char *inbuff;
	size_t inbuff_size;
	ret = read_file_into_buffer(infile_name, &inbuff, &inbuff_size);
	ERR_CHECK(ret);

	if (inbuff_size != MEM_IN_SIZE) {
		printf("Infile size (%d) != mem size (%d)\n", inbuff_size, MEM_IN_SIZE);
		ERR_CHECK(-1);
	}

	ret = mem_write_from_buffer(mem_in_addr, inbuff, inbuff_size);
	ERR_CHECK(ret);


	//Clean FPGA out memory location
	printf("\nClean FPGA OUT mem\n");
	char* tmpbuff = calloc(1, MEM_OUT_SIZE);
	if (tmpbuff == NULL) {
		fprintf(stderr, "Error allocating %d bytes\n", MEM_OUT_SIZE);
		goto error;
	}
	ret = mem_write_from_buffer(mem_out_addr, tmpbuff, MEM_OUT_SIZE);
	free(tmpbuff);
	ERR_CHECK(ret);


	printf("\nWaiting for kernel to be ready\n");
	count = 20*1000; //20 sec
	while ((helm_isready(kern) == 0) && (--count != 0)) {
		nanosleep(&ts, NULL); // sleep 1ms
		if ((count % 1000) == 0) { // Print "." every sec
			printf(" ."); fflush(stdout);
		}
	}
	if (count == 0) {
		printf("\nTIMEOUT reached\n\n");
		goto error;
	}
	(void) helm_ctrl_dump(kern);


	printf("Starting kernel operations\n");
	ret = helm_start(kern);
	if (helm_isdone(kern)) {
		// If this is not the first operation, the done bit will remain high.
		// To start again the procedure, we must also set the continue bit
		helm_continue(kern);
	}
	ERR_CHECK(ret);
	//(void) helm_ctrl_dump(kern); //commented to avoid altering clear on read registers


	printf("\nWaiting for kernel to finish\n");
	count = 20*1000; //20 sec
	while ( !(helm_isdone(kern) || helm_isidle(kern)) && (--count != 0)) {
		nanosleep(&ts, NULL); // sleep 1ms
		if ((count % 1000) == 0) { // Print "." every sec
			printf(" ."); fflush(stdout);
		}
	}
	if (count == 0) {
		printf("\nTIMEOUT reached\n\n");
		goto error;
	} else {
		printf("\nFINISHED!\n");
	}
	(void) helm_ctrl_dump(kern);


	// Read FPGA out mem into buffer and write the buffer into  output file
	char *outbuff;
	ret = mem_read_to_buffer(mem_out_addr, MEM_OUT_SIZE, &outbuff);
	ERR_CHECK(ret);
	ret = write_buffer_into_file(outfile_name, outbuff, MEM_OUT_SIZE);
	ERR_CHECK(ret);


	printf("\nDestroying kernel\n");
error:
	ret = helm_dev_destroy(kern);
	if (ret < 0) {
		fprintf(stderr, "Error %d\n");
		return ret;
	}

	return 0;
}

