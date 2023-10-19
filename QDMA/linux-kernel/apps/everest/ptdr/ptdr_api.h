/*
 * Copyright (C) 2023 - Virtual Open Systems SAS - All rights reserved.
 * Reproduction and communication of this document is strictly prohibited
 * unless specifically authorized in writing by Virtual Open Systems.
 * ****************************************************************************
 * File Name   : ptdr_api.h
 * Author      : STEFANO CIRICI <s.cirici@virtualopensystems.com>
 * Description : PTDR device driver header
 *
 */

#ifndef PTDR_API_H
#define PTDR_API_H

#if defined(__BAMBU__) && !defined(STATIC)
#define STATIC
#endif

/*****************************************************************************/
/**
 * ptdr_init() - Initialize the PTDR device
 *
 * Return:      Pointer to the device, NULL on failure
 *
 *****************************************************************************/
void* ptdr_init();

/*****************************************************************************/
/**
 * ptdr_destroy() - Destroy an initialized PTDR device
 *
 * @dev:        Device pointer
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_destroy(void* dev);

/*****************************************************************************/
/**
 * ptdr_pack_input() - Configure kernel and pack input data to memory
 *
 * @dev:                Device pointer
 * @route_file:         Name of the file containing the route
 * @duration_v:         Array of durations
 * @samples_count:      Number of samples (elements in duration_v)
 * @routepos_index:     Initial position index
 * @routepos_progress:  Initial position progress
 * @departure_time:     Departure time
 * @seed:               Seed for the RNG
 *
 * Return:              0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_pack_input(void* dev, char* route_file, uint64_t *duration_v,
        uint64_t samples_count, uint64_t routepos_index,
        uint64_t routepos_progress, uint64_t departure_time, uint64_t seed);

/*****************************************************************************/
/**
 * ptdr_run_kernel() - Start operations on the PTDR kernel
 *
 * If timeout_us != 0, the function will wait at maximm timeout_us microseconds
 * for the kernel to be ready or to finish.
 * If timeout_us=0, the function will wait undefinitely.
 *
 * @dev:                Device pointer
 * @timeout_us:         Timeout in microseconds
 *
 * Return:              0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_run_kernel(void* dev, uint64_t timeout_us);

/*****************************************************************************/
/**
 * ptdr_unpack_output() - Get the output data from memory
 *
 * Get the duration vector from memory.
 *
 * @dev:                Device pointer
 * @duration_v:         Array of durations (of size samples_count)
 * @samples_count:      Number of samples to get (elements in duration_v)
 *
 * Return:              0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_unpack_output(void* dev, uint64_t *duration_v, uint64_t samples_count);

#endif //#define PTDR_API_H
