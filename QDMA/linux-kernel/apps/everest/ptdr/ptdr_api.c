/*
 * Copyright (C) 2023 - Virtual Open Systems SAS - All rights reserved.
 * Reproduction and communication of this document is strictly prohibited
 * unless specifically authorized in writing by Virtual Open Systems.
 * ****************************************************************************
 * File Name   : ptdr_api.c
 * Author      : STEFANO CIRICI <s.cirici@virtualopensystems.com>
 * Description : PTDR device API
 *
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include "dmautils.h"
#include "ptdr_dev.h"

#ifndef SAMPLES_COUNT
#define SAMPLES_COUNT 10
#endif

#define KERN_PCI_BUS        (0x0083)
#define KERN_PCI_VF_BUS     (0x0007)
#define KERN_PCI_DEV        (0x00)
#define KERN_FUN_ID         (0x00)
#define KERN_IS_VF          (0x00)
#define KERN_Q_START        (0)
#define VF_NUM_MAX          (252) // Max num of VF allowed by QDMA

/* ptdrXHBM.bit */
#define MEM_IN_BASE_ADDR    (0x0000000000001000ULL) // input @ 0
#define KERN_BASE_ADDR      (0x0000000400000000ULL) // kernels starts after 16 GB of HBM
#define KERN_VF_INCR        (0x0000000000010000ULL) // kernels offset

#define ROUND_UP(num, pow)  ( (num + (pow-1)) & (~(pow-1)) )
#define MEM_IN_SIZE         ( 0x700000 ) // sizeof data structure is 0x690AA8

#define TIMEOUT_COUNT_MS    (30*1000) // 30 seconds

#ifndef DEBUG
#define debug_print(format, ...)    printf("[PTDR] " format, ## __VA_ARGS__)
#else
#define debug_print(format, ...)    do { } while (0)
#endif

#define ERR_CHECK(err) do { \
    if (err < 0) { \
        fprintf(stderr, "Error %d\n", err); \
        ptdr_dev_destroy(ptdr); \
        return -err;\
    } \
} while (0)

// Check device pointer, return -EINVAL if invalid
#define CHECK_DEV_PTR(dev) do { \
    if ( (dev == NULL) || \
            (((ptdr_t*)dev)->dev == NULL) )\
    { \
        fprintf(stderr, "ERR: invalid dev pointer\n"); \
        return -EINVAL; \
    } \
} while (0)

typedef struct {
    uint64_t hbm_addr;
    void* dev;
} ptdr_t;




static int quiet_flag = 0;




void* ptdr_init(int vf_num)
{
    ptdr_t *ptdr;
    uint64_t kern_addr;
    uint64_t hbm_addr;
    int kern_pci_bus;
    int kern_pci_dev;
    int kern_pci_id;
    int is_vf;
    uint64_t bdf = 0xFFFFFFFF;

    // Parse VF option
    if (vf_num == -1) {
        kern_addr       = KERN_BASE_ADDR;
        hbm_addr        = MEM_IN_BASE_ADDR;
        kern_pci_bus    = KERN_PCI_BUS;
        kern_pci_dev    = KERN_PCI_DEV;
        kern_pci_id     = KERN_FUN_ID;
        is_vf           = 0;
        debug_print("PF mode:\n");
    }
    else if (vf_num < 0 || vf_num > VF_NUM_MAX) {
        printf("Invalid vf_num %d (max is %d)\n", vf_num, VF_NUM_MAX);
        return NULL;
    } else {
        // Addresses depends on VF num
        kern_addr = KERN_BASE_ADDR + KERN_VF_INCR * vf_num;
        hbm_addr = MEM_IN_BASE_ADDR + ROUND_UP(MEM_IN_SIZE,4096) * vf_num;
        kern_pci_bus = KERN_PCI_VF_BUS;
        kern_pci_dev = KERN_PCI_DEV;
        kern_pci_id = KERN_FUN_ID;
        is_vf = 1; //Activate VF mode
        debug_print("VF mode: VF num %d\n", vf_num);
    }

    // Parse BDF option
    if (bdf < 0x0FFFFFFF) {
        kern_pci_bus = (bdf >> 12) & 0x0FFFF;
        kern_pci_dev = (bdf >> 4) & 0x0FF;
        kern_pci_id = bdf & 0x0F;
    }

    debug_print("    MEM IN   0x%016lx - 0x%016lx\n", hbm_addr, hbm_addr+MEM_IN_SIZE);
    debug_print("    Kern PCI %04x:%02x.%01x\n", kern_pci_bus, kern_pci_dev, kern_pci_id);
    debug_print("    Samples %d\n", SAMPLES_COUNT);


    ptdr = (ptdr_t*) malloc(sizeof(ptdr_t));
    if (ptdr == NULL) {
        fprintf(stderr, "ERR: Cannot allocate %ld bytes\n", sizeof(ptdr_t));
        return NULL;
    }

    debug_print("\nInitializing kernel @ 0x%016lx\n", kern_addr);
    ptdr->dev = ptdr_dev_init(kern_addr, kern_pci_bus, kern_pci_dev,
                kern_pci_id, is_vf, KERN_Q_START);

    if (ptdr->dev == NULL) {
        printf("Error during init!\n");
        free(ptdr);
        return NULL;
    }
    debug_print("Kernel initialized correctly!\n");

    ptdr->hbm_addr = hbm_addr;

    return (void*) ptdr;
}

int ptdr_destroy(void* dev)
{
    ptdr_t *ptdr = (ptdr_t*) dev;
    CHECK_DEV_PTR(dev);

    int ret;
    debug_print("\nDestroying kernel\n");
    ret = ptdr_dev_destroy(ptdr->dev);
    ERR_CHECK(ret);

}

int ptdr_pack_input(void* dev, char* route_file, uint64_t *duration_v,
        uint64_t samples_count, uint64_t routepos_index,
        uint64_t routepos_progress, uint64_t departure_time, uint64_t seed)
{
    int ret;
    ptdr_t *ptdr = (ptdr_t*) dev;
    CHECK_DEV_PTR(dev);

    if (route_file == NULL) {
        printf("Invalid route file name!\n");
        return -EINVAL;
    }

    debug_print("\nConfiguring kernel\n");
    // Create memory structure for kernel and fill it from file
    ret = ptdr_dev_conf(ptdr->dev, route_file, duration_v, samples_count,
            routepos_index, routepos_progress, departure_time, seed,
            ptdr->hbm_addr);
    ERR_CHECK(ret);

    debug_print("Setting num times to 1\n");
    ret = ptdr_set_numtimes(ptdr->dev, 1);
    ERR_CHECK(ret);

    debug_print("Setting autorestart to 0\n");
    ret = ptdr_autorestart(ptdr->dev, 0);
    ERR_CHECK(ret);

    debug_print("Setting interruptglobal to 0\n");
    ret = ptdr_interruptglobal(ptdr->dev, 0);
    ERR_CHECK(ret);
}

int ptdr_run_kernel(void* dev, int timeout)
{
    ptdr_t *ptdr = (ptdr_t*) dev;
    CHECK_DEV_PTR(dev);

    int ret;
    struct timespec ts = {0, 1000*1000}; //1msec

    debug_print("Kernel is ready %d\n", ptdr_isready(ptdr->dev));
    debug_print("Kernel is idle %d\n", ptdr_isidle(ptdr->dev));

    debug_print("\nWaiting for kernel to be ready\n");
    int count = timeout;
    while ((ptdr_isready(ptdr->dev) == 0) && (--count != 0)) {
        nanosleep(&ts, NULL); // sleep 1ms
        if ((count % 1000) == 0) { // Print "." every sec
            debug_print(" ."); fflush(stdout);
        }
    }
    if (count == 0) {
        debug_print("\nTIMEOUT reached\n\n");
        ERR_CHECK(-EAGAIN);
    }

    debug_print("\nStarting kernel operations\n");
    ret = ptdr_start(ptdr->dev);
    if (ptdr_isdone(ptdr->dev)) {
        // If this is not the first operation, the done bit will remain high.
        // To start again the procedure, we must also set the continue bit
        ptdr_continue(ptdr->dev);
    }
    ERR_CHECK(ret);


    debug_print("Waiting for kernel to finish\n");
    count = TIMEOUT_COUNT_MS;
    while ( !(ptdr_isdone(ptdr->dev) || ptdr_isidle(ptdr->dev)) && (--count != 0)) {
        nanosleep(&ts, NULL); // sleep 1ms
        if ((count % 1000) == 0) { // Print "." every sec
            debug_print(" ."); fflush(stdout);
        }
    }

    if (count == 0) {
        debug_print("\nTIMEOUT reached\n\n");
        ERR_CHECK(-EAGAIN);
    } else {
        debug_print("FINISHED!\n\n");
    }
}

int ptdr_unpack_output(void* dev, uint64_t *duration_v, uint64_t samples_count)
{
    ptdr_t *ptdr = (ptdr_t*) dev;
    CHECK_DEV_PTR(dev);

    int ret;
    ret = ptdr_dev_get_durv(ptdr->dev, duration_v, samples_count, ptdr->hbm_addr);
    ERR_CHECK(ret);

    for (int i=0; i<SAMPLES_COUNT; i++) {
        debug_print(" DUR[%02d] = %ld\n", i, duration_v[i]);
    }

}

