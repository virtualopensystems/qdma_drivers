/*
 * This file is part of the Xilinx DMA IP Core driver for Linux
 *
 * Copyright (c) 2017-2022, Xilinx, Inc. All rights reserved.
 * Copyright (c) 2022, Advanced Micro Devices, Inc. All rights reserved.
 * Copyright (c) 2023-2024 Virtual Open Systems SAS - All rights reserved.
 *
 * This source code is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 * ****************************************************************************
 * Author      : STEFANO CIRICI <s.cirici@virtualopensystems.com>
 */

#ifndef QDMA_QUEUES_H
#define QDMA_QUEUES_H

struct queue_info {
    int fd;
    int bdf;
    int qid;
    int is_vf;
};

struct queue_conf {
    int pci_bus;
    int pci_dev;
    int fun_id;
    int is_vf;
    int q_start;
};

/*****************************************************************************/
/**
 * queue_setup() - Setup a QDMA queue
 *
 * Setup a queue given its configuration structure and save the queue
 * information into the pq_info structure
 *
 * @pq_info:    Pointer to queue information structure's pointer
 * @q_conf:     Pointer to queue configuration structure
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int queue_setup(struct queue_info **pq_info, struct queue_conf *q_conf);

/*****************************************************************************/
/**
 * queue_destroy() - Destroy a queue
 *
 * @q_info:     Pointer to queue information structure
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int queue_destroy(struct queue_info *q_info);

/*****************************************************************************/
/**
 * queue_read() - Read data from the specified address
 *
 * The provided data buffer must be at least size bytes big
 *
 * @q_info:     Pointer to queue information structure
 * @data:       Pointer to data buffer where to store the data read
 * @size:       Size (in bytes) of the data to read
 * @addr:       Address in memory where to read from
 *
 * Return:      Count of bytes read on success, negative errno otherwise
 *
 *****************************************************************************/
ssize_t queue_read(struct queue_info *q_info, void *data, uint64_t size,
        uint64_t addr);

/*****************************************************************************/
/**
 * queue_write() - Write data at the specified address
 *
 * @q_info:     Pointer to queue information structure
 * @data:       Pointer to data buffer containing the data to be written
 * @size:       Size (in bytes) of the data buffer to write
 * @addr:       Address in memory where to write to
 *
 * Return:      Count of bytes written on success, negative errno otherwise
 *
 *****************************************************************************/
ssize_t queue_write(struct queue_info *q_info, void *data, uint64_t size,
        uint64_t addr);

#endif //#define QDMA_QUEUES_H
