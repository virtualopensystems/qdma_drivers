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
 * @dev_addr:   Address of the kernel in the FPGA memory
 * @pci_bus:    PCI Bus ID of the kernel
 * @pci_dev:    PCI Device ID of the kernel
 * @fun_id:     PCI Function ID of the kernel
 * @is_vf:      0 if the device is a PF, 1 if it is a VF
 * @q_start:    ID of the queue to use
 *
 * Return:      Pointer to the device, NULL on failure
 *
 *****************************************************************************/
void* ptdr_init(int vf_num);
//uint64_t dev_addr, int pci_bus, int pci_dev, int fun_id, int is_vf, int q_start);

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
 * ptdr_pack_input() - Configure device and return data structure
 *
 * Configure the PTDR device memory space, also writing the durations, route,
 * position, departure and seed registers.
 *
 * @dev:                Device pointer
 * @route_file:         Name of the file containing the route
 * @duration_v:         Array of durations
 * @samples_count:      Number of samples (elements in duration_v)
 * @routepos_index:     Initial position index
 * @routepos_progress:  Initial position progress
 * @departure_time:     Departure time
 * @seed:               Seed for the RNG
 * @base:               Base address in memory where to write the data struct
 *
 * Return:              0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_pack_input(void* dev, char* route_file, uint64_t *duration_v,
        uint64_t samples_count, uint64_t routepos_index,
        uint64_t routepos_progress, uint64_t departure_time, uint64_t seed);

/*****************************************************************************/
/**
 * ptdr_run_kernel() - Initialize the PTDR device
 *
 * @dev_addr:           Address of the kernel in the FPGA memory
 * @pci_bus:            PCI Bus ID of the kernel
 * @pci_dev:            PCI Device ID of the kernel
 * @fun_id:             PCI Function ID of the kernel
 * @is_vf:              0 if the device is a PF, 1 if it is a VF
 * @q_start:            ID of the queue to use
 *
 * Return:              0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_run_kernel(void* dev, int timeout);

/*****************************************************************************/
/**
 * ptdr_unpack_output() - Get the duration vector from memory
 *
 * Get the duration vector from memory, where the kernel will write the
 * output after the execution.
 *
 * @dev:                Device pointer
 * @duration_v:         Array of durations
 * @samples_count:      Number of samples to get (elements in duration_v)
 * @base:               Base address in memory where the data struct is
 *
 * Return:              0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_unpack_output(void* dev, uint64_t *duration_v, uint64_t samples_count);

#endif //#define PTDR_API_H
