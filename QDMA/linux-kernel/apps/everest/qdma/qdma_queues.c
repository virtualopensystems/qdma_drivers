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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "../../include/qdma_nl.h"
#include "../../dma-utils/dmautils.h"
#include "qdma_queues.h"

#define QCONF_TO_BDF(qconf) (((unsigned int)qconf->pci_bus << 12) | \
                            ((unsigned int)qconf->pci_dev << 4) | \
                            (q_conf->fun_id))
#define QDMA_Q_NAME_LEN     (100)
#define QDMA_DEF_QUEUES     (2) // Number of queue to set

// MAX READ/WRITE SIZE LIMIT
// The write(2) function supports up to 0x7ffff000ULL bytes (on most systems)
// We are limited by a kmalloc in map_user_buf_to_sgl (cdev.c:274)
// Values empirically verified
#ifdef HBM16GB
#define RW_MAX_SIZE         (0x019998066ULL)
#else
#define RW_MAX_SIZE         (0x019998198ULL)
#endif

/* Additional debug prints  */
#ifdef DEBUG_QDMA
#define debug_print(format, ...)    printf("  [QDMA_Q] " format, ## __VA_ARGS__)
#else
#define debug_print(format, ...)    do { } while (0)
#endif


static int qmax_get(struct queue_conf *q_conf)
{
    struct xcmd_info xcmd;
    int ret;

    memset(&xcmd, 0, sizeof(struct xcmd_info));
    xcmd.op = XNL_CMD_DEV_INFO;
    xcmd.vf = q_conf->is_vf;
    xcmd.if_bdf = QCONF_TO_BDF(q_conf);

    debug_print("In %s: dev %07x is_vf %d\n", __func__, xcmd.if_bdf, q_conf->is_vf);

    /* Get dev info from qdma driver */
    ret = qdma_dev_info(&xcmd);
    if (ret < 0) {
        fprintf(stderr, "ERR: failed to read qmax of dev %07x, is vf %d\n",
                xcmd.if_bdf, q_conf->is_vf);
        return ret;
    }

    return xcmd.resp.dev_info.qmax;
}


static int queue_validate(struct queue_conf *q_conf)
{
    if (qmax_get(q_conf) < QDMA_DEF_QUEUES) {
        char qmax_cmd[100] = {'\0'};
        snprintf(qmax_cmd, 100, "echo %u > /sys/bus/pci/devices/0000:%02x:%02x.%01x/qdma/qmax",
                QDMA_DEF_QUEUES, q_conf->pci_bus, q_conf->pci_dev, q_conf->fun_id);

        debug_print("In %s: setting %d queues\n", __func__, QDMA_DEF_QUEUES);
        debug_print("  CMD: %s\n", qmax_cmd);

        int ret = system(qmax_cmd);
        if (ret != 0) {
            fprintf(stderr, "ERR: failed setting %d queues on dev %02x:%02x.%01x, ret %d\n",
                    QDMA_DEF_QUEUES, q_conf->pci_bus, q_conf->pci_dev, q_conf->fun_id, ret);
            return -EIO;
        }

        ret = qmax_get(q_conf);
        if (ret < QDMA_DEF_QUEUES) {
            fprintf(stderr, "ERR: failed setting %d queues, set %d instead\n",
                    QDMA_DEF_QUEUES, ret);
            return -EIO;
        }
    }

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

    debug_print("In %s: dev %07x qid %d is_vf %d\n",
            __func__, q_info->bdf, q_info->qid, q_info->is_vf);
    ret = qdma_q_stop(&xcmd);
    if (ret < 0) {
        fprintf(stderr, "ERR: qdma_q_stop failed with err %d\n", ret);
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

    debug_print("In %s: dev %07x qid %d is_vf %d\n",
            __func__, q_info->bdf, q_info->qid, q_info->is_vf);
    ret = qdma_q_del(&xcmd);
    if (ret < 0) {
        fprintf(stderr, "ERR: qdma_q_del failed with err %d\n", ret);
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

    debug_print("In %s: dev %07x qid %d is_vf %d\n",
            __func__, q_info->bdf, q_info->qid, q_info->is_vf);
    ret = qdma_q_add(&xcmd);
    if (ret < 0) {
        fprintf(stderr, "ERR: qdma_q_add failed with err %d\n", ret);
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
        fprintf(stderr, "ERR: qdma_q_start failed with err %d\n", ret);
    }
    return ret;
}

int queue_destroy(struct queue_info *q_info)
{
    if (!q_info) {
        fprintf(stderr, "ERR: Invalid queue info pointer\n");
        return -EINVAL;
    }

    debug_print("In %s: destroying queue fd %d dev %07x\n",
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
    char *q_name;

    if (!pq_info) {
        fprintf(stderr, "ERR: Invalid queue info pointer\n");
        return -EINVAL;
    }

    debug_print("In %s: BUS 0x%04x DEV 0x%02x F %d is_vf %d q_start %d\n",
            __func__, q_conf->pci_bus, q_conf->pci_dev, q_conf->fun_id,
            q_conf->is_vf, q_conf->q_start);

    ret = queue_validate(q_conf);
    if (ret < 0) {
        return ret;
    }

    /* Allocate queue info structure */
    *pq_info = q_info = (struct queue_info *)calloc(1, sizeof(struct queue_info));
    if (!q_info) {
        fprintf(stderr, "ERR: Cannot allocate %ld bytes\n", sizeof(struct queue_info));
        return -ENOMEM;
    }

    q_info->bdf = QCONF_TO_BDF(q_conf);
    q_info->is_vf = q_conf->is_vf;
    q_info->qid = q_conf->q_start;

    /* Create (add) queue */
    ret = queue_add(q_info);
    if (ret < 0) {
        free(q_info);
        return ret;
    }

    /* Start queue */
    ret = queue_start(q_info);
    if (ret < 0) {
        queue_del(q_info);
        free(q_info);
        return ret;
    }

    /* Create queue name from queue info */
    q_name = calloc(1, QDMA_Q_NAME_LEN);
    if (q_name == NULL) {
        fprintf(stderr, "ERR: Cannot allocate %d bytes\n", QDMA_Q_NAME_LEN);
        return -ENOMEM;
    }
    snprintf(q_name, QDMA_Q_NAME_LEN, "/dev/qdma%s%05x-MM-%d",
            (q_info->is_vf) ? "vf" : "",
            q_info->bdf,
            q_info->qid);

    debug_print("In %s: opening queue %s\n", __func__, q_name);
    ret = open(q_name, O_RDWR);

    if (ret < 0) {
        fprintf(stderr, "ERR %d: while opening device %s.\n", errno, q_name);
        free(q_name);
        queue_del(q_info);
        free(q_info);
        return -errno;
    }

    free(q_name);
    q_info->fd = ret;

    return 0;
}

// read_to_buffer() from dma_xfer_utils.c
ssize_t queue_read(struct queue_info *q_info, void *data, uint64_t size, uint64_t addr)
{
    ssize_t ret;
    uint64_t count = 0;
    int fd = q_info->fd;
    char *buf = (char*) data;
    off_t offset = addr;

    debug_print("In %s: R %lu bytes @ 0x%08lx dev %07x\n", __func__, size, addr, q_info->bdf);
    do { /* Support zero byte transfer */
        uint64_t bytes = size - count;

        if (bytes > RW_MAX_SIZE)
            bytes = RW_MAX_SIZE;

        if (offset) {
            ret = lseek(fd, offset, SEEK_SET);
            if (ret < 0) {
                fprintf(stderr, "ERR %d: seek off 0x%lx failed\n", errno, offset);
                return -errno;
            }
            if (ret != offset) {
                fprintf(stderr, "ERR: seek off 0x%lx != 0x%lx\n", ret, offset);
                return -EIO;
            }
        }

        /* read data from device file into memory buffer */
        ret = read(fd, buf, bytes);
        if (ret < 0) {
            fprintf(stderr, "ERR %d: R off 0x%lx, 0x%lx failed.\n", errno, offset, bytes);
            return -errno;
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

    debug_print("In %s: W 0x%lx bytes @ 0x%08lx dev %07x\n", __func__, size, addr, q_info->bdf);
    do { /* Support zero byte transfer */
        uint64_t bytes = size - count;

        if (bytes > RW_MAX_SIZE) {
            bytes = RW_MAX_SIZE;
        }

        if (offset) {
            ret = lseek(fd, offset, SEEK_SET);
            if (ret < 0) {
                fprintf(stderr, "ERR %d: seek off 0x%lx failed\n", errno, offset);
                return -errno;
            }
            if (ret != offset) {
                fprintf(stderr, "ERR: seek off 0x%lx != 0x%lx.\n", ret, offset);
                return -EIO;
            }
        }

        /* write data to device file from memory buffer */
        ret = write(fd, buf, bytes);
        if (ret < 0) {
            fprintf(stderr, "ERR %d: W off 0x%lx, 0x%lx failed.\n", errno, offset, bytes);
            return -errno;
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

