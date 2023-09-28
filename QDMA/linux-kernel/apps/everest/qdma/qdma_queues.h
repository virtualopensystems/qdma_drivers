/*
 * Copyright (C) 2023 - Virtual Open Systems SAS - All rights reserved.
 * Reproduction and communication of this document is strictly prohibited
 * unless specifically authorized in writing by Virtual Open Systems.
 * ****************************************************************************
 * File Name   : qdma_queues.h
 * Author      : STEFANO CIRICI <s.cirici@virtualopensystems.com>
 * Description : QDMA queue library header
 *
 */

#ifndef QDMA_QUEUES_H
#define QDMA_QUEUES_H

#define RW_MAX_SIZE (0x7ffff000)

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
size_t queue_read(struct queue_info *q_info, void *data, uint64_t size,
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
size_t queue_write(struct queue_info *q_info, void *data, uint64_t size,
        uint64_t addr);

#endif //#define QDMA_QUEUES_H
