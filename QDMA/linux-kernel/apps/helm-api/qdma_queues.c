/*
 * Copyright (C) 2023 - Virtual Open Systems SAS - All rights reserved.
 * Reproduction and communication of this document is strictly prohibited
 * unless specifically authorized in writing by Virtual Open Systems.
 * ****************************************************************************
 * File Name   : qdma_queues.c
 * Author      : STEFANO CIRICI <s.cirici@virtualopensystems.com>
 * Description :
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "qdma_nl.h"
#include "dmautils.h"
#include "qdma_queues.h"

#define QCONF_TO_BDF(qconf) (((unsigned int)qconf->pci_bus << 12) | \
							((unsigned int)qconf->pci_dev << 4) | \
							(q_conf->fun_id))
#define QDMA_Q_NAME_LEN     (32)

/* Additional debug prints  */
#ifdef DEBUG
#define debug_print(format, ...)	printf(format, ## __VA_ARGS__)
#else
#define debug_print(format, ...)	do { } while (0)
#endif

static int queue_validate(struct queue_conf *q_conf)
{
	struct xcmd_info xcmd;
	int ret;

	memset(&xcmd, 0, sizeof(struct xcmd_info));
	xcmd.op = XNL_CMD_DEV_INFO;
	xcmd.vf = q_conf->is_vf;
	xcmd.if_bdf = QCONF_TO_BDF(q_conf);

	/* Get dev info from qdma driver */
	ret = qdma_dev_info(&xcmd);
	if (ret < 0) {
		printf("Failed to read qmax for F id: %d, is vf %d\n", q_conf->fun_id, q_conf->is_vf);
		return ret;
	}

	//debug_print("In %s: qmax %d qbase %d\n",
	//		__func__, xcmd.resp.dev_info.qmax, xcmd.resp.dev_info.qbase);

	if (!xcmd.resp.dev_info.qmax) {
		//char aio_max_nr_cmd[100] = {'\0'};
		//snprintf(aio_max_nr_cmd, 100, "echo %u > /proc/sys/fs/aio-max-nr", aio_max_nr);
		//system(aio_max_nr_cmd);
		printf("Error: invalid qmax assigned to function :%d qmax :%u\n",
				q_conf->fun_id, xcmd.resp.dev_info.qmax);
		return -EINVAL;
	}

	/* Only one queue is needed */
	/*
	if (xcmd.resp.dev_info.qmax < q_conf->num_q) {
		printf("Error: Q Range is beyond QMAX %u "
				"Funtion: %x Q start :%u Q Range End :%u\n",
				xcmd.resp.dev_info.qmax, q_conf->fun_id, q_conf->q_start, q_conf->q_start + q_conf->num_q);
		return -EINVAL;
	}
	*/

	return 0;
}

static int queue_stop(struct queue_info *q_info)
{
	int ret;
	struct xcmd_info xcmd;
	struct xcmd_q_parm *qparm;

	memset(&xcmd, 0, sizeof(struct xcmd_info));

	qparm = &xcmd.req.qparm;

	xcmd.op = XNL_CMD_Q_STOP;
	xcmd.vf = q_info->is_vf;
	xcmd.if_bdf = q_info->bdf;
	qparm->idx = q_info->qid;
	qparm->num_q = 1;
	qparm->flags |= XNL_F_QMODE_MM;
	qparm->flags |= XNL_F_QDIR_BOTH;

	ret = qdma_q_stop(&xcmd);
	if (ret < 0) {
		printf("Q_STOP failed, ret :%d\n", ret);
	}
	return ret;
}

static int queue_del(struct queue_info *q_info)
{
	int ret;
	struct xcmd_info xcmd;
	struct xcmd_q_parm *qparm;

	memset(&xcmd, 0, sizeof(struct xcmd_info));

	qparm = &xcmd.req.qparm;

	xcmd.op = XNL_CMD_Q_DEL;
	xcmd.vf = q_info->is_vf;
	xcmd.if_bdf = q_info->bdf;
	qparm->idx = q_info->qid;
	qparm->num_q = 1;
	qparm->flags |= XNL_F_QMODE_MM;
	qparm->flags |= XNL_F_QDIR_BOTH;

	ret = qdma_q_del(&xcmd);
	if (ret < 0) {
		printf("Q_DEL failed, ret :%d\n", ret);
	}
	return ret;
}

static int queue_add(struct queue_info *q_info)
{
	int ret;
	struct xcmd_info xcmd;
	struct xcmd_q_parm *qparm;

	memset(&xcmd, 0, sizeof(struct xcmd_info));

	qparm = &xcmd.req.qparm;

	xcmd.op = XNL_CMD_Q_ADD;
	xcmd.vf = q_info->is_vf;
	xcmd.if_bdf = q_info->bdf;
	qparm->idx = q_info->qid;
	qparm->num_q = 1;
	qparm->flags |= XNL_F_QMODE_MM;
	qparm->flags |= XNL_F_QDIR_BOTH;
	qparm->sflags = qparm->flags;

	debug_print("In %s: BDF %05x QID %d is_vf %d\n",
			__func__, q_info->bdf, q_info->qid, q_info->is_vf);
	ret = qdma_q_add(&xcmd);
	if (ret < 0) {
		printf("Q_ADD failed, ret :%d\n", ret);
	}
	return ret;
}

static int queue_start(struct queue_info *q_info)
{
	int ret;
	struct xcmd_info xcmd;
	struct xcmd_q_parm *qparm;


	memset(&xcmd, 0, sizeof(struct xcmd_info));

	qparm = &xcmd.req.qparm;

	xcmd.op = XNL_CMD_Q_START;
	xcmd.vf = q_info->is_vf;
	xcmd.if_bdf = q_info->bdf;
	qparm->idx = q_info->qid;
	qparm->num_q = 1;
	qparm->flags |= XNL_F_QMODE_MM;
	qparm->flags |= XNL_F_QDIR_BOTH;
	//qparm->qrngsz_idx = 0;

	qparm->flags |= (XNL_F_CMPL_STATUS_EN | XNL_F_CMPL_STATUS_ACC_EN |
			XNL_F_CMPL_STATUS_PEND_CHK | XNL_F_CMPL_STATUS_DESC_EN |
			XNL_F_FETCH_CREDIT);

	ret = qdma_q_start(&xcmd);
	if (ret < 0) {
		printf("Q_START failed, ret :%d\n", ret);
	}
	return ret;
}

int queue_destroy(struct queue_info *q_info)
{
	if (!q_info) {
		printf("Error: Invalid queue info\n");
		return -EINVAL;
	}

	debug_print("In %s: destroying queue fd %d BDF %05x\n",
			__func__, q_info->fd, q_info->bdf);

	if (q_info->fd > 0) {
		close(q_info->fd);
	}

	queue_stop(q_info);
	queue_del(q_info);
	free(q_info);
	
	return 0;
}

int queue_setup(struct queue_info **pq_info, struct queue_conf *q_conf)
{
	int ret;
	struct queue_info *q_info;
	struct xcmd_info xcmd;
	char *q_name;

	if (!pq_info) {
		printf("Error: Invalid queue info\n");
		return -EINVAL;
	}

	debug_print("In %s: BUS 0x%04X DEV 0x%02x F %d is_VF %d q_start %d\n",
			__func__, q_conf->pci_bus, q_conf->pci_dev, q_conf->fun_id,
			q_conf->is_vf, q_conf->q_start);

	ret = queue_validate(q_conf);
	if (ret < 0) {
		return ret;
	}

	/* Allocate queue info structure */
	*pq_info = q_info = (struct queue_info *)calloc(1, sizeof(struct queue_info));
	if (!q_info) {
		printf("Error: OOM\n");
		return -ENOMEM;
	}

	q_info->bdf = QCONF_TO_BDF(q_conf);
	q_info->is_vf = q_conf->is_vf;
	q_info->qid = q_conf->q_start;

	/* Create (add) queue */
	ret = queue_add(q_info);
	if (ret < 0) {
		goto error;
	}

	/* Start queue */
	ret = queue_start(q_info);
	if (ret < 0) {
		goto error2;
	}

	/* Create queue name from queue info */
	q_name = calloc(QDMA_Q_NAME_LEN, 1);
	snprintf(q_name, QDMA_Q_NAME_LEN, "/dev/qdma%s%05x-MM-%d",
			(q_info->is_vf) ? "vf" : "",
			q_info->bdf,
			q_info->qid);

	debug_print("In %s: opening queue %s\n", __func__, q_name);
	ret = open(q_name, O_RDWR);
	free(q_name); //free queue name anyway

	if (ret < 0) {
		fprintf(stderr, "unable to open device %s, %d.\n", q_name, ret);
		perror("open device");
		goto error2;
	}
	q_info->fd = ret;

	return 0;

error2:
	queue_del(q_info);
error:
	free(q_info);
	return ret;
}

// read_to_buffer() from dma_xfer_utils.c
ssize_t queue_read(struct queue_info *q_info, void *data, uint64_t size, uint64_t addr)
{
	ssize_t ret;
	uint64_t count = 0;
	int fd = q_info->fd;
	char *buf = (char*) data;
	off_t offset = addr;
	

	do { /* Support zero byte transfer */
		uint64_t bytes = size - count;

		if (bytes > RW_MAX_SIZE)
			bytes = RW_MAX_SIZE;

		if (offset) {
			ret = lseek(fd, offset, SEEK_SET);
			if (ret < 0) {
				fprintf(stderr, "ERR: seek off 0x%lx failed %zd\n", offset, ret);
				return -EIO;
			}
			if (ret != offset) {
				fprintf(stderr, "ERR: seek off 0x%lx != 0x%lx\n", ret, offset);
				return -EIO;
			}
		}

		/* read data from device file into memory buffer */
		ret = read(fd, buf, bytes);
		if (ret < 0) {
			fprintf(stderr, "ERR: R off 0x%lx, 0x%lx failed %zd.\n", offset, bytes, ret);
			return -EIO;
		}
		if (ret != bytes) {
			fprintf(stderr, "ERR: R off 0x%lx, 0x%lx != 0x%lx.\n", offset, ret, bytes);
			return -EIO;
		}

		count += bytes;
		buf += bytes;
		offset += bytes;

	} while (count < size);

	if (count != size) {
		fprintf(stderr, "ERR: Read failed 0x%lx != 0x%lx.\n", count, size);
		return -EIO;
	}

	return count;
}


// write_from_buffer() from dma_xfer_utils.c
ssize_t queue_write(struct queue_info *q_info, void *data, uint64_t size, uint64_t addr)
{
	ssize_t ret;
	uint64_t count = 0;
	int fd = q_info->fd;
	char *buf = (char*) data;
	off_t offset = addr;

	do { /* Support zero byte transfer */
		uint64_t bytes = size - count;

		if (bytes > RW_MAX_SIZE)
			bytes = RW_MAX_SIZE;

		if (offset) {
			ret = lseek(fd, offset, SEEK_SET);
			if (ret < 0) {
				fprintf(stderr,
					"ERR: seek off 0x%lx failed %zd.\n", offset, ret);
				perror("seek file");
				return -EIO;
			}
			if (ret != offset) {
				fprintf(stderr,
					"ERR: seek off 0x%lx != 0x%lx.\n", ret, offset);
				return -EIO;
			}
		}

		/* write data to device file from memory buffer */
		ret = write(fd, buf, bytes);
		if (ret < 0) {
			fprintf(stderr, "ERR: W off 0x%lx, 0x%lx failed %zd.\n", offset, bytes, ret);
			perror("write file");
			return -EIO;
		}
		if (ret != bytes) {
			fprintf(stderr, "ERR: W off 0x%lx, 0x%lx != 0x%lx.\n", offset, ret, bytes);
			return -EIO;
		}

		count += bytes;
		buf += bytes;
		offset += bytes;
	} while (count < size);

	if (count != size) {
		fprintf(stderr, "ERR: Write failed 0x%lx != 0x%lx.\n", count, size);
		return -EIO;
	}
	return count;
}

