/*
 * Copyright (C) 2023 - Virtual Open Systems SAS - All rights reserved.
 * Reproduction and communication of this document is strictly prohibited
 * unless specifically authorized in writing by Virtual Open Systems.
 * ****************************************************************************
 * File Name   : ptdr_test.c
 * Author      : STEFANO CIRICI <s.cirici@virtualopensystems.com>
 * Description : PTDR device test application
 *
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
        if (err < 0) { \
            fprintf(stderr, "Error %d\n", err); \
            ptdr_destroy(kern); \
            exit(-err); \
        } \
    } while (0)

static void *kern;
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
    printf("  -i FILE        specify input FILE\n");
    printf("  -o FILE        specify output FILE\n");
    printf("  -v vf_num      specify VF number (-1 to use PF, default is -1)\n");
    printf("  -d device_id   specify device BDF\n");
    printf("  -q             quiet output\n");
    printf("  -h             display this help and exit\n");
}

int main(int argc, char *argv[])
{
    int ret, opt;
    char *input_filename = NULL;
    char *output_filename = NULL;
    int vf_num = -1;
    uint64_t bdf = 0xFFFFFFFF;

    signal(SIGINT, intHandler); // Register interrupt handler on CTRL-c

    // Parse command line
    while ((opt = getopt(argc, argv, "hi:o:v:d:q")) != -1) {
        switch (opt) {
            case 'h':
                print_usage(argv);
                exit(EXIT_SUCCESS);
            case 'i':
                input_filename = optarg;
                break;
            case 'o':
                output_filename = optarg;
                break;
            case 'v':
                vf_num = strtol(optarg, NULL, 0);
                break;
            case 'd':
                bdf = strtol(optarg, NULL, 16);
                break;
            case 'q':
                quiet_flag = 1;
                break;
            case '?':
                /* Error message printed by getopt */
                exit(EXIT_FAILURE);
            default:
                /* Should never get here */
                abort();
        }
    }


    if (input_filename == NULL || output_filename == NULL) {
        printf("Invalid input or output file names!\n");
        exit(EXIT_FAILURE);
    }

    info_print("Init PTDR VF %d BDF 0x%06lx\n", vf_num, bdf);
    kern = ptdr_init(vf_num, bdf);

    if (kern == NULL) {
        printf("Error during init!\n");
        exit(EXIT_FAILURE);
    }


    info_print("Pack inputs, sampl_counts %d\n", SAMPLES_COUNT);
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
        printf(" DUR[%02d] = %ld\n", i, dur_profiles[i]);
    }


    info_print("Destroying kernel\n");
    ret = ptdr_destroy(kern);
    ERR_CHECK(ret);

    exit(EXIT_SUCCESS);
}

