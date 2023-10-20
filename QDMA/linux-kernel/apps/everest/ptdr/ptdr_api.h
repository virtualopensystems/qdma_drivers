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
 * @mem_size:           Pointer where to return size of available mem for VF
 *
 * Return:              Pointer to the device, NULL on failure
 *
 *****************************************************************************/
void* ptdr_init(uint64_t *mem_size);

/*****************************************************************************/
/**
 * ptdr_destroy() - Destroy an initialized PTDR device
 *
 * @dev:                Device pointer
 *
 * Return:              0 on success, negative errno otherwise
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

/*****************************************************************************/
/**
 * mem_write() - Write into VF-allocated memory
 *
 * @dev:        Device pointer
 * @data:       Pointer to data to write
 * @size:       Size of the data to write
 * @offset:     Address where to write to
 *
 * Return:      number of data written on success, negative errno otherwise
 *
 *****************************************************************************/
ssize_t mem_write(void *dev, void* data, size_t size, uint64_t offset);

/*****************************************************************************/
/**
 * mem_read() - Read from VF-allocated memory
 *
 * @dev:        Device pointer
 * @data:       Pointer to buffer where to read the data into
 * @size:       Size of the data to read
 * @offset:     Address where to read from
 *
 * Return:      number of data read on success, negative errno otherwise
 *
 *****************************************************************************/
ssize_t mem_read(void *dev, void* data, size_t size, uint64_t offset);

#endif //#define PTDR_API_H
