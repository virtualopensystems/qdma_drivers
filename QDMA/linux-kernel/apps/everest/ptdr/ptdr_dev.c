/*
 * Copyright (C) 2023 - Virtual Open Systems SAS - All rights reserved.
 * Reproduction and communication of this document is strictly prohibited
 * unless specifically authorized in writing by Virtual Open Systems.
 * ****************************************************************************
 * File Name   : ptdr_dev.c
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
#include <assert.h>
#include <fcntl.h>

#include "ptdr_dev.h"
#include "ptdr_regs.h"
#include "qdma_queues.h"

typedef struct {
	uint64_t __sign;
	uint64_t base;
	struct queue_info *q_info;
} ptdr_dev_t;

#define REG_SIZE	(4) //size of registers in bytes
#define PTDR_MAGIC	((uint64_t) 0xC001C0DE50544452ULL)

// Check device pointer, return -EINVAL if invalid
#define CHECK_DEV_PTR(dev) do { \
    if ((dev == NULL) || \
			(((ptdr_dev_t*)dev)->q_info == NULL) || \
			(((ptdr_dev_t*)dev)->__sign != PTDR_MAGIC) ) \
	{ \
        fprintf(stderr, "ERR: invalid dev pointer\n"); \
        return -EINVAL; \
    } \
} while (0)


/* Additional debug prints  */
#ifdef DEBUG
#define debug_print(format, ...)	printf(format, ## __VA_ARGS__)
#else
#define debug_print(format, ...)	do { } while (0)
#endif

static inline int ptdr_reg_read(ptdr_dev_t *dev, uint32_t *data, uint16_t reg)
{
	return queue_read(dev->q_info, data, (uint64_t) REG_SIZE, (uint64_t) dev->base + reg) != REG_SIZE;
}

static inline uint64_t ptdr_reg_write(ptdr_dev_t *dev, uint32_t data, uint16_t reg)
{
	return queue_write(dev->q_info, &data, (uint64_t) REG_SIZE, (uint64_t) dev->base + reg) != REG_SIZE;
}

int ptdr_dev_destroy(void* dev)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	ptdr->__sign = 0;

	debug_print("In %s: destroy queue for ptdr dev\n", __func__);
	(void) queue_destroy(ptdr->q_info);
	free(ptdr);

	return 0;
}

void* ptdr_dev_init(uint64_t dev_addr, int pci_bus, int pci_dev, int fun_id, int is_vf, int q_start)
{
	int ret;
	ptdr_dev_t *ptdr;
	struct queue_conf q_conf;
	uint32_t data;

	ptdr = (ptdr_dev_t*) malloc(sizeof(ptdr_dev_t));
	if (ptdr == NULL) {
		fprintf(stderr, "ERR: Cannot allocate %ld bytes\n", sizeof(ptdr_dev_t));
		return NULL;
	}

	q_conf.pci_bus = pci_bus;
	q_conf.pci_dev = pci_dev;
	q_conf.fun_id = fun_id;
	q_conf.is_vf = is_vf;
	q_conf.q_start = q_start;

	debug_print("In %s: setup queue for ptdr dev\n", __func__);
	ret = queue_setup(&ptdr->q_info, &q_conf);
	if (ret < 0) {
		free(ptdr);
		return NULL;
	}

	ptdr->base = dev_addr;
	debug_print("In %s: setup done, base addr 0x%016lx\n", __func__, ptdr->base);

	// Test if kernel control register is readable
	if (ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_CTRL)) {
		fprintf(stderr, "ERR: Cannot access ptdr device @ 0x%016lx\n", dev_addr);
		ptdr_dev_destroy((void*)ptdr);
		return NULL;
	}

	ptdr->__sign = PTDR_MAGIC;

	return (void*) ptdr;
}


int ptdr_start(void *dev)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
	uint32_t data;

	CHECK_DEV_PTR(dev);

	if (ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_CTRL)) {
		return -EIO;
	}
	debug_print("In %s: CTRL reg is 0x%08x", __func__, data);

	if (data & 0x01) {
		// Not a fatal error
		debug_print("In %s: kernel is not ready! (ctrl reg is 0x%08x)", __func__, data);
		return -EBUSY;
	}

	data &= 0x80; //keep only auto_restart bit
	data |= 0x01; //set ap_start bit

	debug_print("  writing 0x%08x\n", data);
	if (ptdr_reg_write(ptdr, data, PTDR_CTRL_ADDR_CTRL)) {
		return -EIO;
	}

	return 0;
}

int ptdr_isdone(void *dev)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
	uint32_t data;

	CHECK_DEV_PTR(dev);

	if (ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_CTRL)) {
		return -EIO;
	}
	debug_print("In %s: CTRL reg is 0x%08x, done is %d\n",
		__func__, data, (data >> 1) & 0x01);

	// ap_done is BIT(1)
	return (data >> 1) & 0x01;
}

int ptdr_isidle(void *dev)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
	uint32_t data;

	CHECK_DEV_PTR(dev);

	if (ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_CTRL)) {
		return -EIO;
	}
	debug_print("In %s: CTRL reg is 0x%08x, idle is %d\n",
		__func__, data, (data >> 2) & 0x01);

	// ap_idle is BIT(2)
	return (data >> 2) & 0x01;
}

int ptdr_isready(void *dev)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
	uint32_t data;

	CHECK_DEV_PTR(dev);

	if (ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_CTRL)) {
		return -EIO;
	}
	debug_print("In %s: CTRL reg is 0x%08x, ready is %d\n",
		__func__, data, (data >> 3) & 0x01);

	// Do not check ready bit (BIT 3), check ap_start == 0 to see if the kernel is ready for next input
	return !(data & 0x01);
}

int ptdr_continue(void *dev)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
	uint32_t data;

	CHECK_DEV_PTR(dev);

	if (ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_CTRL)) {
		return -EIO;
	}
	debug_print("In %s: CTRL reg is 0x%08x", __func__, data);

	data &= 0x80; //keep only auto_restart bit
	data |= 0x10; //set ap_continue bit

	debug_print("  writing 0x%08x\n", data);
	if (ptdr_reg_write(ptdr, data, PTDR_CTRL_ADDR_CTRL)) {
		return -EIO;
	}

	return 0;
}

int ptdr_autorestart(void *dev, int enable)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
	uint32_t data;

	CHECK_DEV_PTR(dev);

	// Write 0x80 to enable, 0 to disable autorestart
	data = (enable == 0) ? 0 : 0x80;

	debug_print("In %s: writing 0x%08x to CTRL reg\n", __func__, data);
	if (ptdr_reg_write(ptdr, data, PTDR_CTRL_ADDR_CTRL)) {
		return -EIO;
	}

	return 0;
}

int ptdr_set_numtimes(void *dev, uint32_t data)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	debug_print("In %s: writing 0x%08x to NUM_TIMES reg\n", __func__, data);
	if (ptdr_reg_write(ptdr, data, PTDR_CTRL_ADDR_NUM_TIMES)) {
		return -EIO;
	}

	return 0;
}

int ptdr_get_numtimes(void *dev, uint32_t *data)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	if (ptdr_reg_read(ptdr, data, PTDR_CTRL_ADDR_NUM_TIMES)) {
		return -EIO;
	}
	debug_print("In %s: NUM_TIMES reg is 0x%08x\n", __func__, *data);

	return 0;
}

int ptdr_set_durations(void *dev, uint32_t data)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	debug_print("In %s: writing 0x%08x to DUR reg\n", __func__, data);
	if (ptdr_reg_write(ptdr, data, PTDR_CTRL_ADDR_DUR)) {
		return -EIO;
	}

	return 0;
}

int ptdr_get_durations(void *dev, uint32_t *data)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	if (ptdr_reg_read(ptdr, data, PTDR_CTRL_ADDR_DUR)) {
		return -EIO;
	}
	debug_print("In %s: DUR reg is 0x%08x\n", __func__, *data);

	return 0;
}

int ptdr_set_route(void *dev, uint32_t data)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	debug_print("In %s: writing 0x%08x to ROUTE reg\n", __func__, data);
	if (ptdr_reg_write(ptdr, data, PTDR_CTRL_ADDR_ROUTE)) {
		return -EIO;
	}

	return 0;
}

int ptdr_get_route(void *dev, uint32_t *data)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	if (ptdr_reg_read(ptdr, data, PTDR_CTRL_ADDR_ROUTE)) {
		return -EIO;
	}
	debug_print("In %s: ROUTE reg is 0x%08x\n", __func__, *data);

	return 0;
}

int ptdr_set_position(void *dev, uint32_t data)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	debug_print("In %s: writing 0x%08x to POS reg\n", __func__, data);
	if (ptdr_reg_write(ptdr, data, PTDR_CTRL_ADDR_POS)) {
		return -EIO;
	}

	return 0;
}

int ptdr_get_position(void *dev, uint32_t *data)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	if (ptdr_reg_read(ptdr, data, PTDR_CTRL_ADDR_POS)) {
		return -EIO;
	}
	debug_print("In %s: POS reg is 0x%08x\n", __func__, *data);

	return 0;
}

int ptdr_set_departure(void *dev, uint32_t data)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	debug_print("In %s: writing 0x%08x to DEP reg\n", __func__, data);
	if (ptdr_reg_write(ptdr, data, PTDR_CTRL_ADDR_DEP)) {
		return -EIO;
	}

	return 0;
}

int ptdr_get_departure(void *dev, uint32_t *data)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	if (ptdr_reg_read(ptdr, data, PTDR_CTRL_ADDR_DEP)) {
		return -EIO;
	}
	debug_print("In %s: DEP reg is 0x%08x\n", __func__, *data);

	return 0;
}

int ptdr_set_seed(void *dev, uint32_t data)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	debug_print("In %s: writing 0x%08x to SEED reg\n", __func__, data);
	if (ptdr_reg_write(ptdr, data, PTDR_CTRL_ADDR_SEED)) {
		return -EIO;
	}

	return 0;
}

int ptdr_get_seed(void *dev, uint32_t *data)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	if (ptdr_reg_read(ptdr, data, PTDR_CTRL_ADDR_SEED)) {
		return -EIO;
	}
	debug_print("In %s: SEED reg is 0x%08x\n", __func__, *data);

	return 0;
}

int ptdr_set_base(void *dev, uint64_t data)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	debug_print("In %s: writing 0x%08x to BASE[0] reg\n", __func__, (uint32_t) data);
	if (ptdr_reg_write(ptdr, (uint32_t) data, PTDR_CTRL_ADDR_BASE)) {
		return -EIO;
	}

	debug_print("In %s: writing 0x%08x to BASE[1] reg\n", __func__, (uint32_t) (data >> 32));
	if (ptdr_reg_write(ptdr, (uint32_t) (data >> 32) , PTDR_CTRL_ADDR_BASE + REG_SIZE)) {
		return -EIO;
	}

	return 0;
}

int ptdr_get_base(void *dev, uint64_t *data)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
	uint32_t data0, data1;

	CHECK_DEV_PTR(dev);

	if (ptdr_reg_read(ptdr, &data0, PTDR_CTRL_ADDR_BASE)) {
		return -EIO;
	}
	debug_print("In %s: BASE[0] reg is 0x%08x\n", __func__, data0);

	if (ptdr_reg_read(ptdr, &data1, PTDR_CTRL_ADDR_BASE + REG_SIZE)) {
		return -EIO;
	}
	debug_print("In %s: BASE[1] reg is 0x%08x\n", __func__, data1);

	*data = ((uint64_t) data0) | ((uint64_t) data1 << 32);
	debug_print("In %s: BASE[0-1] reg is 0x%016lx\n", __func__, *data);

	return 0;
}

int ptdr_interruptglobal(void *dev, int enable)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
	uint32_t data;

	CHECK_DEV_PTR(dev);

	// Write 0x01 to enable, 0 to disable interrupt global
	data = (enable == 0) ? 0 : 0x01;

	debug_print("In %s: writing 0x%08x to GIE reg\n", __func__, data);
	if (ptdr_reg_write(ptdr, data, PTDR_CTRL_ADDR_GIE)) {
		return -EIO;
	}

	return 0;
}

int ptdr_set_interruptconf(void *dev, uint32_t data)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	debug_print("In %s: writing 0x%08x to IER reg\n", __func__, data);
	if (ptdr_reg_write(ptdr, data, PTDR_CTRL_ADDR_IER)) {
		return -EIO;
	}

	return 0;
}

int ptdr_get_interruptconf(void *dev, uint32_t *data)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	if (ptdr_reg_read(ptdr, data, PTDR_CTRL_ADDR_IER)) {
		return -EIO;
	}
	debug_print("In %s: IER reg is 0x%08x\n", __func__, *data);

	return 0;
}

int ptdr_get_interruptstatus(void *dev, uint32_t *data)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	// Current Interrupt Clear Behavior is Clear on Read(COR).
	if (ptdr_reg_read(ptdr, data, PTDR_CTRL_ADDR_ISR)) {
		return -EIO;
	}
	debug_print("In %s: ISR reg is 0x%08x\n", __func__, *data);

	return 0;
}

int ptdr_route_parse(char *buff, size_t buff_size, ptdr_route_t *route, size_t *route_size)
{
	uint64_t count;
	char* buff_ptr = buff;

	debug_print("In %s: Reading buffer size %ld\n", __func__, buff_size);

	route->frequency_seconds = * (double*) buff_ptr;
	buff_ptr += sizeof(double);
	debug_print("  Frequency %f\n", route->frequency_seconds);

	count = * (uint64_t*) buff_ptr;
	buff_ptr += sizeof(uint64_t);
	debug_print("  Segments 0x%08lx %ld\n", count, count);

	if (count > MAX_SIZE_SEGMENTS) {
		fprintf(stderr, "ERR: Invalid Segments %ld > MAX_SIZE_SEGMENTS %lld\n", count, MAX_SIZE_SEGMENTS);
		return -EINVAL;
	}

	for (int i = 0; i < count; i++) {
		// Ignore the ID, it's not needed to be loaded into memory
		uint64_t id_num;
		id_num = * (uint64_t*) buff_ptr;
		buff_ptr += sizeof(uint64_t);
		//info_print("Ignoring ID_num 0x%08lx %ld\n", id_num, id_num);
		buff_ptr += id_num;

		route->segments[i].segment.length = * (double*) buff_ptr;
		buff_ptr += sizeof(double);

		route->segments[i].segment.speed = * (double*) buff_ptr;
		buff_ptr += sizeof(double);

		for (int j = 0; j < PROFILES_NUM; j++) {
			for(int k = 0; k < PROFILE_VAL_NUM; k++) {
				route->segments[i].profiles[j].values[k] = * (double*) buff_ptr;
				buff_ptr += sizeof(double);
			}
			for(int k = 0; k < PROFILE_VAL_NUM; k++) {
				route->segments[i].profiles[j].cum_probs[k] = * (double*) buff_ptr;
				buff_ptr += sizeof(double);
			}
		}
	}

	*route_size = buff_ptr-buff;
	if ( *route_size > buff_size) {
		fprintf(stderr, "ERR: Invalid route_size %ld > buff_size %ld\n", *route_size, buff_size);
		return -EINVAL;
	}

	return 0;
}

// For debug only
#ifdef DEBUG
int ptdr_reg_dump(void *dev)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
	uint32_t data = 0;

	CHECK_DEV_PTR(dev);

	debug_print("\nIn %s: Dumping device registers @ 0x%016lx\n", __func__, ptdr->base);

	(void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_CTRL);
	debug_print("  0x%02x CTRL: 0x%08x ", PTDR_CTRL_ADDR_CTRL, data);
	debug_print(" start %d", (data >> 0) & 0x01);
	debug_print(" done %d", (data >> 1) & 0x01);
	debug_print(" idle %d", (data >> 2) & 0x01);
	debug_print(" ready %d", (data >> 3) & 0x01);
	debug_print(" cont %d", (data >> 4) & 0x01);
	debug_print(" rest %d", (data >> 7) & 0x01);
	debug_print(" inter %d\n", (data >> 9) & 0x01);

	(void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_GIE);
	debug_print("  0x%02x GIE:  0x%08x\n", PTDR_CTRL_ADDR_GIE, data);

	(void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_IER);
	debug_print("  0x%02x IER:  0x%08x\n", PTDR_CTRL_ADDR_IER, data);

	(void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_ISR);
	debug_print("  0x%02x ISR:  0x%08x\n", PTDR_CTRL_ADDR_ISR, data);

	(void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_NUM_TIMES);
	debug_print("  0x%02x NUM:  0x%08x\n\n", PTDR_CTRL_ADDR_NUM_TIMES, data);

	(void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_DUR);
	debug_print("  0x%02x DUR:  0x%08x\n\n", PTDR_CTRL_ADDR_DUR, data);

	(void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_ROUTE);
	debug_print("  0x%02x ROUTE:  0x%08x\n\n", PTDR_CTRL_ADDR_ROUTE, data);

	(void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_POS);
	debug_print("  0x%02x POS:  0x%08x\n\n", PTDR_CTRL_ADDR_POS, data);

	(void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_DEP);
	debug_print("  0x%02x DEP:  0x%08x\n\n", PTDR_CTRL_ADDR_DEP, data);

	(void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_SEED);
	debug_print("  0x%02x SEED:  0x%08x\n\n", PTDR_CTRL_ADDR_SEED, data);

	(void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_BASE);
	debug_print("  0x%02x BASE0:  0x%08x\n", PTDR_CTRL_ADDR_BASE, data);

	(void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_BASE + REG_SIZE);
	debug_print("  0x%02x BASE1:  0x%08x\n", PTDR_CTRL_ADDR_BASE + REG_SIZE, data);

	return 0;
}

int ptdr_ctrl_dump(void *dev)
{
	ptdr_dev_t *ptdr = (ptdr_dev_t*) dev;
	uint32_t data = 0;

	CHECK_DEV_PTR(dev);

	(void) ptdr_reg_read(ptdr, &data, PTDR_CTRL_ADDR_CTRL);
	debug_print("  0x%02x CTRL: 0x%08x ", PTDR_CTRL_ADDR_CTRL, data);
	debug_print(" start %d", (data >> 0) & 0x01);
	debug_print(" done %d", (data >> 1) & 0x01);
	debug_print(" idle %d", (data >> 2) & 0x01);
	debug_print(" ready %d", (data >> 3) & 0x01);
	debug_print(" cont %d", (data >> 4) & 0x01);
	debug_print(" rest %d", (data >> 7) & 0x01);
	debug_print(" inter %d\n", (data >> 9) & 0x01);

	return 0;
}
#endif
