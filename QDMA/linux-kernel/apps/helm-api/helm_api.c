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
#define KERN_PCI_DEV	(0x00)
#define KERN_FUN_ID		(0x00)
#define KERN_IS_VF		(0x00)
#define KERN_Q_START	(0)

#define KERN_ADDR 		(0x00000000)
#define MEM_IN_ADDR 	(0xC0000000)
#define MEM_OUT_ADDR 	(0xC0100000)

int helm_reg_dump(void *dev);


/* Additional debug prints  */
# if 1
# define debug_print(format, ...)       printf(format, ## __VA_ARGS__)
#else
# define debug_print(format, ...)       do { } while (0)
#endif


static void hexdump(const unsigned char *data, size_t size, uint64_t off)
{
	#define LINE_WIDTH 16

	for (int j = 0; j < size; j += LINE_WIDTH) {
		char hex[LINE_WIDTH * 3 + 1] = {0};
		char ascii[LINE_WIDTH + 1] = {0};
		for (int i = 0; i < LINE_WIDTH && i+j < size; i++) {
			snprintf(&hex[i * 3], 4, "%02X ", data[i]);
			ascii[i] = (data[i] >= 32 && data[i] <= 126) ? data[i] : '.';
		}
		printf("%08zx: %-48s %s\n", off, hex, ascii);
		off += LINE_WIDTH;
		data += LINE_WIDTH;
	}
}

int print_mem(uint64_t addr, uint64_t size)
{
	int ret;
	struct queue_info *q_info;
	struct queue_conf q_conf;

	q_conf.pci_bus = KERN_PCI_BUS;
	q_conf.pci_dev = KERN_PCI_DEV;
	q_conf.fun_id = KERN_FUN_ID;
	q_conf.is_vf = KERN_IS_VF;
	q_conf.q_start = KERN_Q_START + 1; //use a different queue id

	ret = queue_setup(&q_info, &q_conf);
	if (ret < 0) {
		return ret;
	}

	char * data = (char*) malloc(sizeof(char) * size);

	debug_print("Reading 0x%02x (%d) bytes @ 0x%08x\n", size, size, addr);

	int rsize = queue_read(q_info, data, size, addr);

	if (rsize == size)
	{
		hexdump(data, size, addr);
		debug_print("\n");
	}

	free(data);
	ret = queue_destroy(q_info);
	if (ret < 0) {
		return ret;
	}

	return 0;
}

int main(int argc, char *argv[])
{
	int ret;

	debug_print("In %s\n", __func__);


	debug_print("Initializing kernel @ 0x%08x\n", KERN_ADDR);
	void * kern;
	
	kern = helm_dev_init(KERN_ADDR, KERN_PCI_BUS, KERN_PCI_DEV,
			KERN_FUN_ID, KERN_IS_VF, KERN_Q_START);


	if (kern == NULL) {
		debug_print("Error during init!\n");
		return -1;
	}

	debug_print("Kernel initialized correctly!\n");

	(void) helm_reg_dump(kern);


	debug_print("Setting memory in addr  @ 0x%08x\n", MEM_IN_ADDR);
	(void) helm_set_in(kern, MEM_IN_ADDR);

	debug_print("Setting memory out addr @ 0x%08x\n", MEM_OUT_ADDR);
	(void) helm_set_out(kern, MEM_OUT_ADDR);

	debug_print("Setting num times to 1\n");
	(void) helm_set_numtimes(kern, 1);

	debug_print("Setting autorestart to 0\n");
	(void) helm_autorestart(kern, 0);

	debug_print("Setting interruptglobal to 0\n");
	(void) helm_interruptglobal(kern, 0);

	debug_print("Kernel is ready %d\n", helm_isready(kern));
	debug_print("Kernel is idle %d\n", helm_isidle(kern));


	(void) helm_reg_dump(kern);


	(void) print_mem(MEM_IN_ADDR, 0x40);
	(void) print_mem(MEM_OUT_ADDR, 0x40);

	debug_print("\nStarting kernel operations\n");
	(void) helm_start(kern);


	struct timespec ts = {0, 1000*1000};
	int count = 20*1000; //20 sec

	while (helm_isdone(kern) == 0 && --count != 0) {
		nanosleep(&ts, NULL);
		if ((count % 1000) == 0) {
			debug_print(" ."); fflush(stdout);
		}
	}

	if (count == 0) {
		debug_print("\nTIMEOUT reached\n\n");
	}


	(void) helm_reg_dump(kern);

	(void) print_mem(MEM_OUT_ADDR, 0x40);

	debug_print("\nDestroying kernel\n");
	(void) helm_dev_destroy(kern);

	return 0;
}

