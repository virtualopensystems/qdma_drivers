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

#ifndef PTDR_DEV_H
#define PTDR_DEV_H

#if defined(__BAMBU__) && !defined(STATIC)
#define STATIC
#endif

#define PTDR_AP_DONE_INTERRUPT      (1 << 0)
#define PTDR_AP_READY_INTERRUPT     (1 << 1)

/*****************************************************************************/
/**
 * ptdr_dev_init() - Initialize the PTDR device
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
void* ptdr_dev_init(uint64_t dev_addr, int pci_bus, int pci_dev, int fun_id,
        int is_vf, int q_start);

/*****************************************************************************/
/**
 * ptdr_dev_destroy() - Destroy an initialized PTDR device
 *
 * @dev:        Device pointer
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_dev_destroy(void* dev);

/*****************************************************************************/
/**
 * ptdr_dev_conf() - Configure device and return data structure
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
 * @end:                End of the available memory space
 *
 * Return:              0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_dev_conf(void* dev, char* route_file, uint64_t *duration_v,
        uint64_t samples_count, uint64_t routepos_index,
        uint64_t routepos_progress, uint64_t departure_time,
        uint64_t seed, uint64_t base, uint64_t end);

/*****************************************************************************/
/**
 * ptdr_dev_get_durv() - Get the duration vector from memory
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
int ptdr_dev_get_durv(void* dev, uint64_t *duration_v, uint64_t samples_count,
        uint64_t base);

/*****************************************************************************/
/**
 * ptdr_start() - Start operations on the device
 *
 * @dev:        Device pointer
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_start(void *dev);

/*****************************************************************************/
/**
 * ptdr_continue() - Continue operations on the device
 *
 * @dev:        Device pointer
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_continue(void *dev);

/*****************************************************************************/
/**
 * ptdr_isdone() - Check if the device operation has finished
 *
 * @dev:        Device pointer
 *
 * Return:      1 if done, 0 if not (still running), negative errno otherwise
 *
 *****************************************************************************/
int ptdr_isdone(void *dev);

/*****************************************************************************/
/**
 * ptdr_isidle() - Check if the device is idle
 *
 * @dev:        Device pointer
 *
 * Return:      1 if device is idle, 0 if not, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_isidle(void *dev);

/*****************************************************************************/
/**
 * ptdr_isready() - Check if the device is ready to start
 *
 * @dev:        Device pointer
 *
 * Return:      1 if device is ready, 0 if not, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_isready(void *dev);

/*****************************************************************************/
/**
 * ptdr_autorestart() - Enable or disable autorestart of kernel operations
 *
 * @dev:        Device pointer
 * @enable:     1 to enable, 0 to disable
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_autorestart(void *dev, int enable);

/*****************************************************************************/
/**
 * ptdr_interruptglobal() - Enable or disable global interrupt
 *
 * @dev:        Device pointer
 * @enable:     1 to enable, 0 to disable
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_interruptglobal(void *dev, int enable);

/*****************************************************************************/
/**
 * ptdr_set_numtimes() - Set numtimes register
 *
 * The numtimes register indicates the number of times to restart operations
 *
 * @dev:        Device pointer
 * @data:       Value to set (repetitions)
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_set_numtimes(void *dev, uint32_t data);

/*****************************************************************************/
/**
 * ptdr_get_numtimes() - Get value of numtimes register
 *
 * @dev:        Device pointer
 * @data:       Pointer where to store the register value
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_get_numtimes(void *dev, uint32_t *data);

/*****************************************************************************/
/**
 * ptdr_set_interruptconf() - Set interrupt configuration register
 *
 * @dev:        Device pointer
 * @data:       Value to set
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_set_interruptconf(void *dev, uint32_t data);

/*****************************************************************************/
/**
 * ptdr_get_interruptconf() - Get value of interrupt configuration register
 *
 * @dev:        Device pointer
 * @data:       Pointer where to store the register value
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_get_interruptconf(void *dev, uint32_t *data);

/*****************************************************************************/
/**
 * ptdr_get_interruptstatus() - Get value of interrupt status register
 *
 * @dev:        Device pointer
 * @data:       Pointer where to store the register value
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_get_interruptstatus(void *dev, uint32_t *data);

/*****************************************************************************/
/**
 * ptdr_set_durations() - Set durations register
 *
 * Shall write the offset in memory from the base register
 *
 * @dev:        Device pointer
 * @data:       Value to set
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_set_durations(void *dev, uint32_t data);

/*****************************************************************************/
/**
 * ptdr_get_durations() - Get value of durations register
 *
 * @dev:        Device pointer
 * @data:       Pointer where to store the register value
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_get_durations(void *dev, uint32_t *data);

/*****************************************************************************/
/**
 * ptdr_set_route() - Set route register
 *
 * Shall write the offset in memory from the base register
 *
 * @dev:        Device pointer
 * @data:       Value to set
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_set_route(void *dev, uint32_t data);

/*****************************************************************************/
/**
 * ptdr_get_route() - Get value of route register
 *
 * @dev:        Device pointer
 * @data:       Pointer where to store the register value
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_get_route(void *dev, uint32_t *data);

/*****************************************************************************/
/**
 * ptdr_set_position() - Set position register
 *
 * Shall write the offset in memory from the base register
 *
 * @dev:        Device pointer
 * @data:       Value to set
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_set_position(void *dev, uint32_t data);

/*****************************************************************************/
/**
 * ptdr_get_position() - Get value of position register
 *
 * @dev:        Device pointer
 * @data:       Pointer where to store the register value
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_get_position(void *dev, uint32_t *data);

/*****************************************************************************/
/**
 * ptdr_set_departure() - Set departure register
 *
 * Shall write the offset in memory from the base register
 *
 * @dev:        Device pointer
 * @data:       Value to set
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_set_departure(void *dev, uint32_t data);

/*****************************************************************************/
/**
 * ptdr_get_departure() - Get value of numtimes register
 *
 * @dev:        Device pointer
 * @data:       Pointer where to store the register value
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_get_departure(void *dev, uint32_t *data);

/*****************************************************************************/
/**
 * ptdr_set_seed() - Set seed register
 *
 * Shall write the offset in memory from the base register
 *
 * @dev:        Device pointer
 * @data:       Value to set
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_set_seed(void *dev, uint32_t data);

/*****************************************************************************/
/**
 * ptdr_get_seed() - Get value of seed register
 *
 * @dev:        Device pointer
 * @data:       Pointer where to store the register value
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_get_seed(void *dev, uint32_t *data);

/*****************************************************************************/
/**
 * ptdr_set_base() - Set base register
 *
 * The base register indicates the address in memory from where all the
 * offsets are computed
 *
 * @dev:        Device pointer
 * @data:       Value to set
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_set_base(void *dev, uint64_t data);

/*****************************************************************************/
/**
 * ptdr_get_base() - Get value of base register
 *
 * @dev:        Device pointer
 * @data:       Pointer where to store the register value
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_get_base(void *dev, uint64_t *data);

/*****************************************************************************/
/**
 * ptdr_mem_write() - Write into FPGA memory
 *
 * @dev:        Device pointer
 * @data:       Pointer to data to write
 * @size:       Size of the data to write
 * @mem_addr:   Address where to write to
 *
 * Return:      number of data written on success, negative errno otherwise
 *
 *****************************************************************************/
ssize_t ptdr_mem_write(void *dev, void* data, size_t size, uint64_t mem_addr);

/*****************************************************************************/
/**
 * ptdr_mem_read() - Read from FPGA memory
 *
 * @dev:        Device pointer
 * @data:       Pointer to buffer where to read the data into
 * @size:       Size of the data to read
 * @mem_addr:   Address where to read from
 *
 * Return:      number of data read on success, negative errno otherwise
 *
 *****************************************************************************/
ssize_t ptdr_mem_read(void *dev, void* data, size_t size, uint64_t mem_addr);

#ifdef DEBUG
/*****************************************************************************/
/**
 * ptdr_reg_dump() - Print the value of all the device registers
 *
 * @dev:        Device pointer
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_reg_dump(void *dev);

/*****************************************************************************/
/**
 * ptdr_ctrl_dump() - Print the value of the control register and its fields
 *
 * @dev:        Device pointer
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_ctrl_dump(void *dev);
#endif

#endif //#define PTDR_DEV_H
