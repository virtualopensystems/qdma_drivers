/*
 * Copyright (C) 2023 - Virtual Open Systems SAS - All rights reserved.
 * Reproduction and communication of this document is strictly prohibited
 * unless specifically authorized in writing by Virtual Open Systems.
 * ****************************************************************************
 * File Name   : qdma_queues.h
 * Author      : STEFANO CIRICI <s.cirici@virtualopensystems.com>
 * Description :
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
 * helm_setup_queue()
 * Setup a queue given its configuration structure and save queue information
 * into pq_info structure
 *
 * @param pq_info	pointer to queue information structure's pointer
 * @param q_conf	pointer to queue configuration structure
 *
 * @returns		0 for success and <0 for error
 *
 *****************************************************************************/
int queue_setup(struct queue_info **pq_info, struct queue_conf *q_conf);

/*****************************************************************************/
/**
 * helm_destroy_queue() - destroy a queue given its info pointer
 *
 * @param q_info	pointer to queue information structure
 *
 * @returns		0 for success and <0 for error
 *
 *****************************************************************************/
int queue_destroy(struct queue_info *q_info);

ssize_t queue_read(struct queue_info *q_info, void *data, uint64_t size, uint64_t addr);
ssize_t queue_write(struct queue_info *q_info, void *data, uint64_t size, uint64_t addr);

#endif /* QDMA_QUEUES_H */
