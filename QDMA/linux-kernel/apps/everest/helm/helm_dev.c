/*
 * Copyright (C) 2023 - Virtual Open Systems SAS - All rights reserved.
 * Reproduction and communication of this document is strictly prohibited
 * unless specifically authorized in writing by Virtual Open Systems.
 * ****************************************************************************
 * File Name   : helm_dev.c
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

#include "helm_dev.h"
#include "helm_regs.h"
#include "qdma_queues.h"

typedef struct {
	uint64_t __sign;
	uint64_t base;
	struct queue_info *q_info;
} helm_dev_t;

#define REG_SIZE	(4) //size of registers in bytes
#define HELM_MAGIC	((uint64_t) 0xC001C0DE48656C6DULL)

// Check device pointer, return -EINVAL if invalid
#define CHECK_DEV_PTR(dev) do { \
    if ((dev == NULL) || \
			(((helm_dev_t*)dev)->q_info == NULL) || \
			(((helm_dev_t*)dev)->__sign != HELM_MAGIC) ) \
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

static inline int helm_reg_read(helm_dev_t *dev, uint32_t *data, uint16_t reg)
{
	return queue_read(dev->q_info, data, (uint64_t) REG_SIZE, (uint64_t) dev->base + reg) != REG_SIZE;
}

static inline uint64_t helm_reg_write(helm_dev_t *dev, uint32_t data, uint16_t reg)
{
	return queue_write(dev->q_info, &data, (uint64_t) REG_SIZE, (uint64_t) dev->base + reg) != REG_SIZE;
}

int helm_dev_destroy(void* dev)
{
	helm_dev_t *helm = (helm_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	helm->__sign = 0;

	debug_print("In %s: destroy queue for helm dev\n", __func__);
	(void) queue_destroy(helm->q_info);
	free(helm);

	return 0;
}

void* helm_dev_init(uint64_t dev_addr, int pci_bus, int pci_dev, int fun_id, int is_vf, int q_start)
{
	int ret;
	helm_dev_t *helm;
	struct queue_conf q_conf;
	uint32_t data;

	helm = (helm_dev_t*) malloc(sizeof(helm_dev_t));
	if (helm == NULL) {
		fprintf(stderr, "ERR: Cannot allocate %ld bytes\n", sizeof(helm_dev_t));
		return NULL;
	}

	q_conf.pci_bus = pci_bus;
	q_conf.pci_dev = pci_dev;
	q_conf.fun_id = fun_id;
	q_conf.is_vf = is_vf;
	q_conf.q_start = q_start;

	debug_print("In %s: setup queue for helm dev\n", __func__);
	ret = queue_setup(&helm->q_info, &q_conf);
	if (ret < 0) {
		free(helm);
		return NULL;
	}

	helm->base = dev_addr;
	debug_print("In %s: setup done, base addr 0x%016lx\n", __func__, helm->base);

	// Test if kernel control register is readable
	if (helm_reg_read(helm, &data, HELM_CTRL_ADDR_CTRL)) {
		fprintf(stderr, "ERR: Cannot access helm device @ 0x%016lx\n", dev_addr);
		helm_dev_destroy((void*)helm);
		return NULL;
	}

	helm->__sign = HELM_MAGIC;

	return (void*) helm;
}


int helm_start(void *dev)
{
	helm_dev_t *helm = (helm_dev_t*) dev;
	uint32_t data;

	CHECK_DEV_PTR(dev);

	if (helm_reg_read(helm, &data, HELM_CTRL_ADDR_CTRL)) {
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
	if (helm_reg_write(helm, data, HELM_CTRL_ADDR_CTRL)) {
		return -EIO;
	}

	return 0;
}

int helm_isdone(void *dev)
{
	helm_dev_t *helm = (helm_dev_t*) dev;
	uint32_t data;

	CHECK_DEV_PTR(dev);

	if (helm_reg_read(helm, &data, HELM_CTRL_ADDR_CTRL)) {
		return -EIO;
	}
	debug_print("In %s: CTRL reg is 0x%08x, done is %d\n",
		__func__, data, (data >> 1) & 0x01);

	// ap_done is BIT(1)
	return (data >> 1) & 0x01;
}

int helm_isidle(void *dev)
{
	helm_dev_t *helm = (helm_dev_t*) dev;
	uint32_t data;

	CHECK_DEV_PTR(dev);

	if (helm_reg_read(helm, &data, HELM_CTRL_ADDR_CTRL)) {
		return -EIO;
	}
	debug_print("In %s: CTRL reg is 0x%08x, idle is %d\n",
		__func__, data, (data >> 2) & 0x01);

	// ap_idle is BIT(2)
	return (data >> 2) & 0x01;
}

int helm_isready(void *dev)
{
	helm_dev_t *helm = (helm_dev_t*) dev;
	uint32_t data;

	CHECK_DEV_PTR(dev);

	if (helm_reg_read(helm, &data, HELM_CTRL_ADDR_CTRL)) {
		return -EIO;
	}
	debug_print("In %s: CTRL reg is 0x%08x, ready is %d\n",
		__func__, data, (data >> 3) & 0x01);

	// Do not check ready bit (BIT 3), check ap_start == 0 to see if the kernel is ready for next input
	return !(data & 0x01);
}

int helm_continue(void *dev)
{
	helm_dev_t *helm = (helm_dev_t*) dev;
	uint32_t data;

	CHECK_DEV_PTR(dev);

	if (helm_reg_read(helm, &data, HELM_CTRL_ADDR_CTRL)) {
		return -EIO;
	}
	debug_print("In %s: CTRL reg is 0x%08x", __func__, data);

	data &= 0x80; //keep only auto_restart bit
	data |= 0x10; //set ap_continue bit

	debug_print("  writing 0x%08x\n", data);
	if (helm_reg_write(helm, data, HELM_CTRL_ADDR_CTRL)) {
		return -EIO;
	}

	return 0;
}

int helm_autorestart(void *dev, int enable)
{
	helm_dev_t *helm = (helm_dev_t*) dev;
	uint32_t data;

	CHECK_DEV_PTR(dev);

	// Write 0x80 to enable, 0 to disable autorestart
	data = (enable == 0) ? 0 : 0x80;

	debug_print("In %s: writing 0x%08x to CTRL reg\n", __func__, data);
	if (helm_reg_write(helm, data, HELM_CTRL_ADDR_CTRL)) {
		return -EIO;
	}

	return 0;
}

int helm_set_in(void *dev, uint64_t data)
{
	helm_dev_t *helm = (helm_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	debug_print("In %s: writing 0x%08x to IN[0] reg\n", __func__, (uint32_t) data);
	if (helm_reg_write(helm, (uint32_t) data, HELM_CTRL_ADDR_IN_DATA)) {
		return -EIO;
	}

	debug_print("In %s: writing 0x%08x to IN[1] reg\n", __func__, (uint32_t) (data >> 32));
	if (helm_reg_write(helm, (uint32_t) (data >> 32) , HELM_CTRL_ADDR_IN_DATA + REG_SIZE)) {
		return -EIO;
	}

	return 0;
}

int helm_get_in(void *dev, uint64_t *data)
{
	helm_dev_t *helm = (helm_dev_t*) dev;
	uint32_t data0, data1;

	CHECK_DEV_PTR(dev);

	if (helm_reg_read(helm, &data0, HELM_CTRL_ADDR_IN_DATA)) {
		return -EIO;
	}
	debug_print("In %s: IN[0] reg is 0x%08x\n", __func__, data0);

	if (helm_reg_read(helm, &data1, HELM_CTRL_ADDR_IN_DATA + REG_SIZE)) {
		return -EIO;
	}
	debug_print("In %s: IN[1] reg is 0x%08x\n", __func__, data1);

	*data = ((uint64_t) data0) | ((uint64_t) data1 << 32);
	debug_print("In %s: IN[0-1] reg is 0x%016lx\n", __func__, *data);

	return 0;
}

int helm_set_out(void *dev, uint64_t data)
{
	helm_dev_t *helm = (helm_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	debug_print("In %s: writing 0x%08x to OUT[0] reg\n", __func__, (uint32_t) data);
	if (helm_reg_write(helm, (uint32_t) data, HELM_CTRL_ADDR_OUT_DATA)) {
		return -EIO;
	}

	debug_print("In %s: writing 0x%08x to OUT[1] reg\n", __func__, (uint32_t) (data >> 32));
	if (helm_reg_write(helm, (uint32_t) (data >> 32) , HELM_CTRL_ADDR_OUT_DATA + REG_SIZE)) {
		return -EIO;
	}

	return 0;
}

int helm_get_out(void *dev, uint64_t *data)
{
	helm_dev_t *helm = (helm_dev_t*) dev;
	uint32_t data0, data1;

	CHECK_DEV_PTR(dev);

	if (helm_reg_read(helm, &data0, HELM_CTRL_ADDR_OUT_DATA)) {
		return -EIO;
	}
	debug_print("In %s: OUT[0] reg is 0x%08x\n", __func__, data0);

	if (helm_reg_read(helm, &data1, HELM_CTRL_ADDR_OUT_DATA + REG_SIZE)) {
		return -EIO;
	}
	debug_print("In %s: OUT[1] reg is 0x%08x\n", __func__, data1);

	*data = ((uint64_t) data0) | ((uint64_t) data1 << 32);
	debug_print("In %s: OUT[0-1] reg is 0x%016lx\n", __func__, *data);

	return 0;
}

int helm_set_numtimes(void *dev, uint32_t data)
{
	helm_dev_t *helm = (helm_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	debug_print("In %s: writing 0x%08x to NUM_TIMES reg\n", __func__, data);
	if (helm_reg_write(helm, data, HELM_CTRL_ADDR_NUM_TIMES)) {
		return -EIO;
	}

	return 0;
}

int helm_get_numtimes(void *dev, uint32_t *data)
{
	helm_dev_t *helm = (helm_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	if (helm_reg_read(helm, data, HELM_CTRL_ADDR_NUM_TIMES)) {
		return -EIO;
	}
	debug_print("In %s: NUM_TIMES reg is 0x%08x\n", __func__, *data);

	return 0;
}

int helm_interruptglobal(void *dev, int enable)
{
	helm_dev_t *helm = (helm_dev_t*) dev;
	uint32_t data;

	CHECK_DEV_PTR(dev);

	// Write 0x01 to enable, 0 to disable interrupt global
	data = (enable == 0) ? 0 : 0x01;

	debug_print("In %s: writing 0x%08x to GIE reg\n", __func__, data);
	if (helm_reg_write(helm, data, HELM_CTRL_ADDR_GIE)) {
		return -EIO;
	}

	return 0;
}

int helm_set_interruptconf(void *dev, uint32_t data)
{
	helm_dev_t *helm = (helm_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	debug_print("In %s: writing 0x%08x to IER reg\n", __func__, data);
	if (helm_reg_write(helm, data, HELM_CTRL_ADDR_IER)) {
		return -EIO;
	}

	return 0;
}

int helm_get_interruptconf(void *dev, uint32_t *data)
{
	helm_dev_t *helm = (helm_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	if (helm_reg_read(helm, data, HELM_CTRL_ADDR_IER)) {
		return -EIO;
	}
	debug_print("In %s: IER reg is 0x%08x\n", __func__, *data);

	return 0;
}

int helm_get_interruptstatus(void *dev, uint32_t *data)
{
	helm_dev_t *helm = (helm_dev_t*) dev;

	CHECK_DEV_PTR(dev);

	// Current Interrupt Clear Behavior is Clear on Read(COR).
	if (helm_reg_read(helm, data, HELM_CTRL_ADDR_ISR)) {
		return -EIO;
	}
	debug_print("In %s: ISR reg is 0x%08x\n", __func__, *data);

	return 0;
}

// For debug only
#ifdef DEBUG
int helm_reg_dump(void *dev)
{
	helm_dev_t *helm = (helm_dev_t*) dev;
	uint32_t data = 0;

	CHECK_DEV_PTR(dev);

	debug_print("\nIn %s: Dumping device registers @ 0x%016lx\n", __func__, helm->base);

	(void) helm_reg_read(helm, &data, HELM_CTRL_ADDR_CTRL);
	debug_print("  0x%02x CTRL: 0x%08x ", HELM_CTRL_ADDR_CTRL, data);
	debug_print(" start %d", (data >> 0) & 0x01);
	debug_print(" done %d", (data >> 1) & 0x01);
	debug_print(" idle %d", (data >> 2) & 0x01);
	debug_print(" ready %d", (data >> 3) & 0x01);
	debug_print(" cont %d", (data >> 4) & 0x01);
	debug_print(" rest %d", (data >> 7) & 0x01);
	debug_print(" inter %d\n", (data >> 9) & 0x01);

	(void) helm_reg_read(helm, &data, HELM_CTRL_ADDR_GIE);
	debug_print("  0x%02x GIE:  0x%08x\n", HELM_CTRL_ADDR_GIE, data);

	(void) helm_reg_read(helm, &data, HELM_CTRL_ADDR_IER);
	debug_print("  0x%02x IER:  0x%08x\n", HELM_CTRL_ADDR_IER, data);

	(void) helm_reg_read(helm, &data, HELM_CTRL_ADDR_ISR);
	debug_print("  0x%02x ISR:  0x%08x\n", HELM_CTRL_ADDR_ISR, data);

	(void) helm_reg_read(helm, &data, HELM_CTRL_ADDR_IN_DATA);
	debug_print("  0x%02x IN0:  0x%08x\n", HELM_CTRL_ADDR_IN_DATA, data);

	(void) helm_reg_read(helm, &data, HELM_CTRL_ADDR_IN_DATA + REG_SIZE);
	debug_print("  0x%02x IN1:  0x%08x\n", HELM_CTRL_ADDR_IN_DATA + REG_SIZE, data);

	(void) helm_reg_read(helm, &data, HELM_CTRL_ADDR_OUT_DATA);
	debug_print("  0x%02x OUT0: 0x%08x\n", HELM_CTRL_ADDR_OUT_DATA, data);

	(void) helm_reg_read(helm, &data, HELM_CTRL_ADDR_OUT_DATA + REG_SIZE);
	debug_print("  0x%02x OUT1: 0x%08x\n", HELM_CTRL_ADDR_OUT_DATA + REG_SIZE, data);

	(void) helm_reg_read(helm, &data, HELM_CTRL_ADDR_NUM_TIMES);
	debug_print("  0x%02x NUM:  0x%08x\n\n", HELM_CTRL_ADDR_NUM_TIMES, data);

	return 0;
}

int helm_ctrl_dump(void *dev)
{
	helm_dev_t *helm = (helm_dev_t*) dev;
	uint32_t data = 0;

	CHECK_DEV_PTR(dev);

	(void) helm_reg_read(helm, &data, HELM_CTRL_ADDR_CTRL);
	debug_print("  0x%02x CTRL: 0x%08x ", HELM_CTRL_ADDR_CTRL, data);
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
