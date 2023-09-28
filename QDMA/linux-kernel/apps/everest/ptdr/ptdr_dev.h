/*
 * Copyright (C) 2023 - Virtual Open Systems SAS - All rights reserved.
 * Reproduction and communication of this document is strictly prohibited
 * unless specifically authorized in writing by Virtual Open Systems.
 * ****************************************************************************
 * File Name   : ptdr_dev.h
 * Author      : STEFANO CIRICI <s.cirici@virtualopensystems.com>
 * Description : PTDR device driver header
 *
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
 * Return in @data the structure to write @base_reg_address
 *
 * @dev:                Device pointer
 * @route_file:         Name of the file containing the route
 * @duration_v:         Array of durations
 * @duration_size:      Number of elements in duration_v
 * @routepos_index:     Initial position index
 * @routepos_progress:  Initial position progress
 * @departure_time:     Departure time
 * @seed:               Seed for the RNG
 * @data:               Pointer where to return the allocated filled structure
 * @data_size:          Pointer where to return the size of data
 *
 * Return:              0 on success, negative errno otherwise
 *
 *****************************************************************************/
int ptdr_dev_conf(void* dev, char* route_file, unsigned long long *duration_v,
        size_t duration_size, unsigned long long routepos_index,
        double routepos_progress, unsigned long long departure_time,
        unsigned long long seed, void ** data, size_t *data_size);

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
