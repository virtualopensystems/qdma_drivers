/*
 * Copyright (C) 2023 - Virtual Open Systems SAS - All rights reserved.
 * Reproduction and communication of this document is strictly prohibited
 * unless specifically authorized in writing by Virtual Open Systems.
 * ****************************************************************************
 * File Name   : helm_dev.h
 * Author      : STEFANO CIRICI <s.cirici@virtualopensystems.com>
 * Description :
 *
 */

#ifndef HELM_DEV_H
#define HELM_DEV_H

#define HELM_AP_DONE_INTERRUPT 		(1 << 0)
#define HELM_AP_READY_INTERRUPT 	(1 << 1)


int helm_dev_destroy(void* dev);

void* helm_dev_init(uint64_t dev_addr, int pci_bus, int pci_dev, int fun_id, int is_vf, int q_start);

int helm_start(void *dev);

int helm_isdone(void *dev);

int helm_isidle(void *dev);

int helm_isready(void *dev);

int helm_continue(void *dev);

int helm_autorestart(void *dev, int enable);

int helm_set_in(void *dev, uint64_t data);

int helm_get_in(void *dev, uint64_t *data);

int helm_set_out(void *dev, uint64_t data);

int helm_get_out(void *dev, uint64_t *data);

int helm_set_numtimes(void *dev, uint32_t data);

int helm_get_numtimes(void *dev, uint32_t *data);

int helm_interruptglobal(void *dev, int enable);

int helm_set_interruptconf(void *dev, uint32_t data);

int helm_get_interruptconf(void *dev, uint32_t *data);

int helm_get_interruptstatus(void *dev, uint32_t *data);

#endif
