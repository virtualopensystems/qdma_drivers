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
#include <errno.h>
#include <time.h>
#include <string.h>
#include <ctype.h>

#include "ptdr_dev.h"
#include "ptdr_api.h"


/* Fixed in ptdrXHBM.bit */
#define MEM_BASE_ADDR       (0x0000000000001000ULL) // input @ 0
#ifdef HBM16GB
#pragma message "HBM set to 16 GB"
#define MEM_END_ADDR        (0x0000000400000000ULL) // Mem ends @ 16GB
#else
#define MEM_END_ADDR        (0x0000000200000000ULL) // Mem ends @ 8GB
#endif
#define KERN_BASE_ADDR      (0x0000000400000000ULL) // kernels starts after 16 GB of HBM
#define KERN_VF_INCR        (0x0000000000010000ULL) // kernels offset
#define VF_NUM_MAX          (252) // Max num of VF allowed by QDMA

#define EVEREST_VF_PATTERN  "everestvf"
#define EVEREST_FILEPATH    "/dev/virtio-ports"
#define DRIVER_TYPE         "ptdr"

#ifdef DEBUG
#define debug_print(format, ...)    printf("[PTDR] " format, ## __VA_ARGS__)
#else
#define debug_print(format, ...)    do { } while (0)
#endif

#define ERR_CHECK(err) do { \
    if (err < 0) { \
        fprintf(stderr, "Error %d\n", err); \
        return err;\
    } \
} while (0)

// Check device pointer, return -EINVAL if invalid
#define CHECK_DEV_PTR(dev) do { \
    if ( (dev == NULL) || \
            (((ptdr_t*)dev)->dev == NULL) || \
            (((ptdr_t*)dev)->__sign != PTDR_MAGIC) ) \
    { \
        fprintf(stderr, "ERR: invalid dev pointer\n"); \
        return -EINVAL; \
    } \
} while (0)

#define PTDR_MAGIC  ((uint64_t) 0x0050544452415049ULL)

typedef struct {
    uint64_t    __sign;
    uint64_t    mem_start;
    uint64_t    mem_end;
    void*       dev;
} ptdr_t;

static void lower_string(char *str) {
    for(int i = 0; str[i]; i++){
        str[i] = tolower(str[i]);
    }
}

static int get_vf_num(int *curr_vf_num, int *vf_idx, uint32_t *bdf)
{
    FILE *fp;
    char path[512];
    char vf_type[15];
    *curr_vf_num = 0;
    *vf_idx = -1;
    *bdf = -1;

    fp = popen("/bin/ls " EVEREST_FILEPATH, "r");
    if (fp == NULL) {
        fprintf(stderr, "ERR %d: Failed opening file " EVEREST_FILEPATH "\n", errno);
        return -1;
    }

    while (fgets(path, sizeof(path), fp) != NULL) {
        if (sscanf(path, EVEREST_VF_PATTERN "_%d_%d_%x_%14s", curr_vf_num, vf_idx, bdf, vf_type) == 4) {
            debug_print("VF %d of %d, id %06x, type %s \n", *vf_idx, *curr_vf_num, *bdf, vf_type);

            pclose(fp);
            lower_string(vf_type);
            if (strcmp(vf_type, DRIVER_TYPE) != 0) {
                fprintf(stderr, "ERR: VF type %s is not supported by this driver\n", vf_type);
                return -1;
            }
            if (*vf_idx < 0 || *vf_idx >= VF_NUM_MAX) {
                fprintf(stderr, "ERR: Invalid VF idx number %d\n", *vf_idx);
                return -1;
            }
            if (*curr_vf_num <= 0 || *curr_vf_num > VF_NUM_MAX) {
                fprintf(stderr, "ERR: Invalid current VF number %d\n", *curr_vf_num);
                return -1;
            }

            return 0;
        }
    }

    pclose(fp);
    fprintf(stderr, "ERR: Could not find any VF\n");
    return -1;
}

void* ptdr_init(uint64_t *mem_size)
{
    ptdr_t *ptdr;
    uint64_t kern_addr;
    uint64_t mem_start;
    uint64_t mem_end;
    int kern_pci_bus;
    int kern_pci_dev;
    int kern_pci_id;
    int is_vf;
    int ret;
    int curr_vf_num;
    int vf_idx;
    uint32_t bdf;

    ret = get_vf_num(&curr_vf_num, &vf_idx, &bdf);
    if (ret == -1) {
        return NULL;
    } else {
        // Addresses depends on VF num
        uint64_t mem_size_per_vf = (MEM_END_ADDR - MEM_BASE_ADDR) / curr_vf_num;
        mem_start       = MEM_BASE_ADDR + mem_size_per_vf * vf_idx;
        mem_end         = mem_start + mem_size_per_vf;
        kern_addr       = KERN_BASE_ADDR + KERN_VF_INCR * vf_idx;
        is_vf           = 1; //Activate VF mode
    }

    // Parse BDF argument
    if (bdf > 0x000FFFFF) {
        fprintf(stderr, "Invalid BDF ID 0x%08x\n", bdf);
        return NULL;
    } else {
        kern_pci_bus = (bdf >> 12) & 0x0FF;
        kern_pci_dev = (bdf >> 4) & 0x0FF;
        kern_pci_id = bdf & 0x0F;
    }

    debug_print("MEM     0x%016lx - 0x%016lx\n", mem_start, mem_end);
    debug_print("PCI dev %04x:%02x.%01x\n", kern_pci_bus, kern_pci_dev, kern_pci_id);

    ptdr = (ptdr_t*) malloc(sizeof(ptdr_t));
    if (ptdr == NULL) {
        fprintf(stderr, "ERR: Cannot allocate %ld bytes\n", sizeof(ptdr_t));
        return NULL;
    }

    debug_print("Initializing kernel @ 0x%016lx\n", kern_addr);
    ptdr->dev = ptdr_dev_init(kern_addr, kern_pci_bus, kern_pci_dev,
                kern_pci_id, is_vf, 0);

    if (ptdr->dev == NULL) {
        free(ptdr);
        return NULL;
    }

    debug_print("Setting num times to 1\n");
    ret = ptdr_set_numtimes(ptdr->dev, 1);
    if (ret != 0) {
        fprintf(stderr, "ERR: ptdr_set_numtimes failed with error %d\n", ret);
        ptdr_dev_destroy(ptdr->dev);
        return NULL;
    }

    debug_print("Setting autorestart to 0\n");
    ret = ptdr_autorestart(ptdr->dev, 0);
    if (ret != 0) {
        fprintf(stderr, "ERR: ptdr_autorestart failed with error %d\n", ret);
        ptdr_dev_destroy(ptdr->dev);
        return NULL;
    }

    debug_print("Setting interruptglobal to 0\n");
    ret = ptdr_interruptglobal(ptdr->dev, 0);
    if (ret != 0) {
        fprintf(stderr, "ERR: ptdr_interruptglobal failed with error %d\n", ret);
        ptdr_dev_destroy(ptdr->dev);
        return NULL;
    }

    debug_print("Kernel initialized correctly!\n");

    ptdr->mem_start = mem_start;
    ptdr->mem_end = mem_end;
    ptdr->__sign = PTDR_MAGIC;

    *mem_size = mem_end - mem_start;

    return (void*) ptdr;
}

int ptdr_destroy(void* dev)
{
    ptdr_t *ptdr = (ptdr_t*) dev;
    CHECK_DEV_PTR(dev);

    ptdr->__sign = 0;

    debug_print("Destroying kernel\n");
    ptdr_dev_destroy(ptdr->dev);

    free(ptdr);

    return 0;
}

int ptdr_pack_input(void* dev, char* route_file, uint64_t *duration_v,
        uint64_t samples_count, uint64_t routepos_index,
        uint64_t routepos_progress, uint64_t departure_time, uint64_t seed)
{
    int ret;
    ptdr_t *ptdr = (ptdr_t*) dev;
    CHECK_DEV_PTR(dev);

    if (route_file == NULL) {
        fprintf(stderr, "Invalid route file name!\n");
        return -EINVAL;
    }

    debug_print("Configuring kernel\n");
    // Create memory structure for kernel and fill it from file
    ret = ptdr_dev_conf(ptdr->dev, route_file, duration_v, samples_count,
            routepos_index, routepos_progress, departure_time, seed,
            ptdr->mem_start, ptdr->mem_end);
    ERR_CHECK(ret);

    return 0;
}

int ptdr_run_kernel(void* dev, uint64_t timeout_us)
{
    ptdr_t *ptdr = (ptdr_t*) dev;
    CHECK_DEV_PTR(dev);

    int ret;
    struct timespec ts = {0, 1000}; //1usec

    debug_print("Kernel is ready %d\n", ptdr_isready(ptdr->dev));
    debug_print("Kernel is idle %d\n", ptdr_isidle(ptdr->dev));

    debug_print("Waiting for kernel to be ready\n");

    if (timeout_us == 0) {
        while (ptdr_isready(ptdr->dev) == 0) {
            nanosleep(&ts, NULL); // sleep 1us
        }
    } else {
        uint64_t count = timeout_us;
        //wait until kernel is ready, up to  timeout us
        while ((ptdr_isready(ptdr->dev) == 0) && (--count != 0)) {
            nanosleep(&ts, NULL); // sleep 1us
            if ((count % 1000000) == 0) { // Print "." every sec
                debug_print(" ."); fflush(stdout);
            }
        }
        if (count == 0) {
            debug_print("TIMEOUT reached\n\n");
            ERR_CHECK(-EAGAIN);
        }
    }

    debug_print("Starting kernel operations\n");
    ret = ptdr_start(ptdr->dev);
    ERR_CHECK(ret);
    if (ptdr_isdone(ptdr->dev)) {
        // If this is not the first operation, the done bit will remain high.
        // To start again the procedure, we must also set the continue bit
        ret = ptdr_continue(ptdr->dev);
        ERR_CHECK(ret);
    }

    debug_print("Waiting for kernel to finish\n");
    if (timeout_us == 0) {
        while (ptdr_isready(ptdr->dev) == 0) {
            nanosleep(&ts, NULL); // sleep 1us
        }
    } else {
        uint64_t count = timeout_us;
        //wait until kernel finishes, up to  timeout us
        while ( !(ptdr_isdone(ptdr->dev) || ptdr_isidle(ptdr->dev)) && (--count != 0)) {
            nanosleep(&ts, NULL); // sleep 1us
            if ((count % 1000000) == 0) { // Print "." every sec
                debug_print(" ."); fflush(stdout);
            }
        }

        if (count == 0) {
            debug_print("TIMEOUT reached\n\n");
            ERR_CHECK(-EAGAIN);
        }
    }

    debug_print("Completed!\n");
    return 0;
}

int ptdr_unpack_output(void* dev, uint64_t *duration_v, uint64_t samples_count)
{
    int ret;
    ptdr_t *ptdr = (ptdr_t*) dev;
    CHECK_DEV_PTR(dev);

    ret = ptdr_dev_get_durv(ptdr->dev, duration_v, samples_count, ptdr->mem_start);
    ERR_CHECK(ret);

    return 0;
}

ssize_t mem_write(void *dev, void* data, size_t size, uint64_t offset)
{
    ptdr_t *ptdr = (ptdr_t*) dev;
    CHECK_DEV_PTR(dev);

    uint64_t mem_addr = ptdr->mem_start + offset;

    if ((mem_addr < ptdr->mem_start) || (mem_addr >= ptdr->mem_end)) {
        fprintf(stderr, "ERR: offset out of bounds\n");
        return -EFAULT;
    }
    if ((mem_addr + size) > ptdr->mem_end)  {
        fprintf(stderr, "ERR: size out of bounds\n");
        return -EFBIG;
    }

    return ptdr_mem_write(ptdr->dev, data, size, mem_addr);
}

ssize_t mem_read(void *dev, void* data, size_t size, uint64_t offset)
{
    ptdr_t *ptdr = (ptdr_t*) dev;
    CHECK_DEV_PTR(dev);

    uint64_t mem_addr = ptdr->mem_start + offset;

    if ((mem_addr < ptdr->mem_start) || (mem_addr >= ptdr->mem_end)) {
        fprintf(stderr, "ERR: offset out of bounds\n");
        return -EFAULT;
    }
    if ((mem_addr + size) > ptdr->mem_end)  {
        fprintf(stderr, "ERR: size out of bounds\n");
        return -EFBIG;
    }

    return ptdr_mem_read(ptdr->dev, data, size, mem_addr);
}

