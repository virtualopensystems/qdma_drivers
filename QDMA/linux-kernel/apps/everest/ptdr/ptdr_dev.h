/*
 * Copyright (C) 2023 - Virtual Open Systems SAS - All rights reserved.
 * Reproduction and communication of this document is strictly prohibited
 * unless specifically authorized in writing by Virtual Open Systems.
 * ****************************************************************************
 * File Name   : ptdr_dev.h
 * Author      : STEFANO CIRICI <s.cirici@virtualopensystems.com>
 * Description :
 *
 */

#ifndef PTDR_DEV_H
#define PTDR_DEV_H

#if defined(__BAMBU__) && !defined(STATIC)
#define STATIC
#endif

#define PTDR_AP_DONE_INTERRUPT 		(1 << 0)
#define PTDR_AP_READY_INTERRUPT 	(1 << 1)

void* ptdr_dev_init(uint64_t dev_addr, int pci_bus, int pci_dev, int fun_id, int is_vf, int q_start);

int ptdr_dev_destroy(void* dev);

int ptdr_dev_conf(void* dev, char* route_file, unsigned long long *duration_v, size_t duration_size,
    unsigned long long routepos_index, double routepos_progress, unsigned long long departure_time,
	unsigned long long seed, void ** data, size_t *data_size);

int ptdr_start(void *dev);

int ptdr_isdone(void *dev);

int ptdr_isidle(void *dev);

int ptdr_isready(void *dev);

int ptdr_continue(void *dev);

int ptdr_autorestart(void *dev, int enable);

int ptdr_set_numtimes(void *dev, uint32_t data);

int ptdr_get_numtimes(void *dev, uint32_t *data);

int ptdr_set_durations(void *dev, uint32_t data);

int ptdr_get_durations(void *dev, uint32_t *data);

int ptdr_set_route(void *dev, uint32_t data);

int ptdr_get_route(void *dev, uint32_t *data);

int ptdr_set_position(void *dev, uint32_t data);

int ptdr_get_position(void *dev, uint32_t *data);

int ptdr_set_departure(void *dev, uint32_t data);

int ptdr_get_departure(void *dev, uint32_t *data);

int ptdr_set_seed(void *dev, uint32_t data);

int ptdr_get_seed(void *dev, uint32_t *data);

int ptdr_set_base(void *dev, uint64_t data);

int ptdr_get_base(void *dev, uint64_t *data);

int ptdr_interruptglobal(void *dev, int enable);

int ptdr_set_interruptconf(void *dev, uint32_t data);

int ptdr_get_interruptconf(void *dev, uint32_t *data);

int ptdr_get_interruptstatus(void *dev, uint32_t *data);

#ifdef DEBUG
int ptdr_reg_dump(void *dev);
int ptdr_ctrl_dump(void *dev);
#endif

#endif
