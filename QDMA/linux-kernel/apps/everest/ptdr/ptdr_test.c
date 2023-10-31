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
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#include "ptdr_api.h"

#ifndef SAMPLES_COUNT
#define SAMPLES_COUNT 10
#endif

#define info_print(fmt, ...) \
    do { \
        if (!quiet_flag) { \
            printf(fmt, ##__VA_ARGS__); \
        } \
    } while (0)

#define ERR_CHECK(err) \
    do { \
        if (err != 0) { \
            fprintf(stderr, "Test Error %d\n", err); \
            ptdr_destroy(kern); \
            exit(-err); \
        } \
    } while (0)

static void *kern = NULL;
static uint64_t vf_mem_size = 0;
static int quiet_flag = 0;

static void intHandler(int sig) {
    char c;
    int ret;

    signal(sig, SIG_IGN);

    printf("\nDo you really want to quit? [y/n] ");
    c = getchar();
    if (c == 'y' || c == 'Y') {
        if (kern != NULL) {
            info_print("\nDestroying kernel\n");
            ret = ptdr_destroy(kern);
            ERR_CHECK(ret);
        }
        exit(0);
    }
    signal(sig, intHandler);
}

static void print_usage(char*argv[])
{
    printf("EVEREST PTDR kernel test\n");
    printf("Usage: %s [OPTION]...\n", argv[0]);
    printf("  -i FILE        specify input FILE, mandatory\n");
    printf("  -t             also perform memory tests\n");
    printf("  -q             quiet output\n");
    printf("  -h             display this help and exit\n");
}

// mem_read and mem_write tests
void mem_tests()
{
    ssize_t ret;
    uint64_t read_test[SAMPLES_COUNT+3] = {0};
    ssize_t size = sizeof(read_test);
    uint64_t offset = 0;
    int test_fail = 0;
    int test_num = 0;

    printf("\n[TEST] Starting tests on mem_read and mem_write\n");

    printf("\n[TEST %02d] Read raw duration vector\n", ++test_num);
    ret = mem_read(kern, &read_test, size, offset);
    if (ret != size) {
        printf("[TEST %02d] Failed with error %ld, expected %ld\n", test_num, ret, size);
        test_fail++;
    } else {
        for (int i=0; i<SAMPLES_COUNT+3; i++) {
            printf("             dur_v[%02d] = %ld\n", i, read_test[i]);
            read_test[i] = 0x0F0FCAFE0F0F0000 + i;
        }
    }

    printf("\n[TEST %02d] Write other data in raw duration vector\n", ++test_num);
    ret = mem_write(kern, &read_test, size, offset);
    if (ret != size) {
        printf("[TEST %02d] Failed with error %ld, expected %ld\n", test_num, ret, size);
        test_fail++;
    }

    printf("\n[TEST %02d] Read raw duration vector again\n", ++test_num);
    ret = mem_read(kern, &read_test, size, offset);
    if (ret != size) {
        printf("[TEST %02d] Failed with error %ld, expected %ld\n", test_num, ret, size);
        test_fail++;
    } else {
        for (int i=0; i<SAMPLES_COUNT+3; i++) {
            printf("             dur_v[%02d] = %ld\n", i, read_test[i]);
        }
    }

    printf("\nAllocating big buffer for tests, size %ld bytes (%ld MB)\n", vf_mem_size+1, (vf_mem_size+1) >> 20);
    char* wr_test = (char*) malloc(vf_mem_size+1);
    if (wr_test == NULL) {
        printf("ERR %d while allocating %ld bytes for wr_test!\n", errno, vf_mem_size+1);
        ERR_CHECK(-errno);
    }
    printf("Now filling it with random data, this may take a while...\n");
    srand(time(0));
    for (uint64_t i = 0; i<vf_mem_size+1; i++) {
        wr_test[i] = (char) rand();
    }

    printf("\n[TEST %02d] Write max allowed size %ld (0x%lx) bytes\n", ++test_num, vf_mem_size, vf_mem_size);
    size = vf_mem_size;
    ret = mem_write(kern, wr_test, size, offset);
    if (ret != size) {
        printf("[TEST %02d] Failed with error %ld, expected %ld\n", test_num, ret, size);
        test_fail++;
    }

    printf("\nAllocating big buffer for read back test, size %ld bytes (%ld MB)\n", vf_mem_size+1, (vf_mem_size+1) >> 20);
    char* rd_test = (char*) calloc(vf_mem_size+1, 1);
    if (rd_test == NULL) {
        printf("ERR %d while allocating %ld bytes for rd_test, skipping test\n", errno, vf_mem_size+1);
        goto after_read;
    }

    printf("\n[TEST %02d] Read back memory and check content\n", ++test_num);
    ret = mem_read(kern, rd_test, size, offset);
    if (ret != size) {
        printf("[TEST %02d] Failed with error %ld, expected %ld\n", test_num, ret, size);
        test_fail++;
    } else if ((ret = memcmp(wr_test, rd_test, size)) != 0) {
        printf("[TEST %02d] Failed check, ret is %ld\n", test_num, ret);
        test_fail++;
    }
    free(rd_test);

after_read:
    printf("\n[TEST %02d] Write max allowed size\n", ++test_num);
    size = vf_mem_size;
    ret = mem_write(kern, wr_test, size, offset);
    if (ret != size) {
        printf("[TEST %02d] Failed with error %ld, expected %ld\n", test_num, ret, size);
        test_fail++;
    }

    printf("\n[TEST %02d] Write more than max allowed size\n", ++test_num);
    size = vf_mem_size+1;
    ret = mem_write(kern, wr_test, size, offset);
    if (ret != -EFBIG) {
        printf("[TEST %02d] Failed with error %ld, expected %d\n", test_num, ret, -EFBIG);
        test_fail++;
    }

    printf("\n[TEST %02d] Write at the end of the allowed range\n", ++test_num);
    size = 16;
    offset = vf_mem_size - size;
    ret = mem_write(kern, wr_test, size, offset);
    if (ret != size) {
        printf("[TEST %02d] Failed with error %ld, expected %ld\n", test_num, ret, size);
        test_fail++;
    }

    printf("\n[TEST %02d] Write after allowed range\n", ++test_num);
    size = 16;
    offset = vf_mem_size; // allowed offset is from 0 to vf_mem_size (not included)
    ret = mem_write(kern, wr_test, size, offset);
    if (ret != -EFAULT) {
        printf("[TEST %02d] Failed with error %ld, expected %d\n", test_num, ret, -EFAULT);
        test_fail++;
    }

    printf("\n[TEST %02d] Write max size after allowed range\n", ++test_num);
    size = vf_mem_size;
    offset = 1;
    ret = mem_write(kern, wr_test, size, offset);
    if (ret != -EFBIG) {
        printf("[TEST %02d] Failed with error %ld, expected %d\n", test_num, ret, -EFBIG);
        test_fail++;
    }

    free(wr_test);
    printf("\n[TEST] passed %d out of %d tests (failed %d)\n\n", test_num-test_fail, test_num, test_fail);
    return;
}

int main(int argc, char *argv[])
{
    int ret, opt;
    char *input_filename = NULL;
    int testing = 0;

    signal(SIGINT, intHandler); // Register interrupt handler on CTRL-c

    // Parse command line
    while ((opt = getopt(argc, argv, "hi:qt")) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv);
                exit(EXIT_SUCCESS);
            case 'i':
                input_filename = optarg;
                break;
            case 'q':
                quiet_flag = 1;
                break;
            case 't':
                testing = 1;
                break;
            case '?':
                /* Error message printed by getopt */
                exit(EXIT_FAILURE);
            default:
                /* Should never get here */
                printf("Invalid options\n\n");
                print_usage(argv);
                exit(EXIT_FAILURE);
        }
    }


    if (input_filename == NULL) {
        printf("Invalid input file name!\n");
        exit(EXIT_FAILURE);
    }

    info_print("Init PTDR kernel\n");
    kern = ptdr_init(&vf_mem_size);
    if (kern == NULL) {
        printf("Error during init!\n");
        exit(EXIT_FAILURE);
    }
    info_print("Kernel initialized, vf mem size is 0x%08lx\n", vf_mem_size);

    info_print("Pack inputs, samples_count %d\n", SAMPLES_COUNT);
    uint64_t dur_profiles[SAMPLES_COUNT] = {0};
    uint64_t routepos_index = 0;
    uint64_t routepos_progress = ((uint64_t) ((double)0.0));
    uint64_t departure_time = 1623823200ULL * 1000;
    uint64_t seed = 0xABCDE23456789;

    ret = ptdr_pack_input(kern, input_filename, dur_profiles, SAMPLES_COUNT,
            routepos_index, routepos_progress, departure_time, seed);
    ERR_CHECK(ret);

    info_print("Starting kernel operations\n");
    ret = ptdr_run_kernel(kern, 1000*1000*10); //10 sec
    ERR_CHECK(ret);


    info_print("Unpack output\n");
    ret = ptdr_unpack_output(kern, dur_profiles, SAMPLES_COUNT);
    ERR_CHECK(ret);

    for (int i=0; i<SAMPLES_COUNT; i++) {
        info_print(" DUR[%02d] = %ld\n", i, dur_profiles[i]);
    }

    if (testing) {
        mem_tests();
    }

    info_print("Destroying kernel\n");
    ret = ptdr_destroy(kern);
    ERR_CHECK(ret);

    exit(EXIT_SUCCESS);
}

