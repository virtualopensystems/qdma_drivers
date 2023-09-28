/*
 * Copyright (C) 2023 - Virtual Open Systems SAS - All rights reserved.
 * Reproduction and communication of this document is strictly prohibited
 * unless specifically authorized in writing by Virtual Open Systems.
 * ****************************************************************************
 * File Name   : helm_dev.h
 * Author      : STEFANO CIRICI <s.cirici@virtualopensystems.com>
 * Description : Helmoltz device driver header
 *
 */

#ifndef HELM_DEV_H
#define HELM_DEV_H

#define HELM_AP_DONE_INTERRUPT      (1 << 0)
#define HELM_AP_READY_INTERRUPT     (1 << 1)

/*****************************************************************************/
/**
 * helm_dev_init() - Initialize the Helmholtz device
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
void* helm_dev_init(uint64_t dev_addr, int pci_bus, int pci_dev, int fun_id,
        int is_vf, int q_start);

/*****************************************************************************/
/**
 * helm_dev_destroy() - Destroy an initialized Helmholtz device
 *
 * @dev:        Device pointer
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int helm_dev_destroy(void* dev);

/*****************************************************************************/
/**
 * helm_start() - Start operations on the device
 *
 * @dev:        Device pointer
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int helm_start(void *dev);

/*****************************************************************************/
/**
 * helm_continue() - Continue operations on the device
 *
 * @dev:        Device pointer
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int helm_continue(void *dev);

/*****************************************************************************/
/**
 * helm_isdone() - Check if the device operation has finished
 *
 * @dev:        Device pointer
 *
 * Return:      1 if done, 0 if not (still running), negative errno otherwise
 *
 *****************************************************************************/
int helm_isdone(void *dev);

/*****************************************************************************/
/**
 * helm_isidle() - Check if the device is idle
 *
 * @dev:        Device pointer
 *
 * Return:      1 if device is idle, 0 if not, negative errno otherwise
 *
 *****************************************************************************/
int helm_isidle(void *dev);

/*****************************************************************************/
/**
 * helm_isready() - Check if the device is ready to start
 *
 * @dev:        Device pointer
 *
 * Return:      1 if device is ready, 0 if not, negative errno otherwise
 *
 *****************************************************************************/
int helm_isready(void *dev);

/*****************************************************************************/
/**
 * helm_autorestart() - Enable or disable autorestart of kernel operations
 *
 * @dev:        Device pointer
 * @enable:     1 to enable, 0 to disable
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int helm_autorestart(void *dev, int enable);

/*****************************************************************************/
/**
 * helm_interruptglobal() - Enable or disable global interrupt
 *
 * @dev:        Device pointer
 * @enable:     1 to enable, 0 to disable
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int helm_interruptglobal(void *dev, int enable);

/*****************************************************************************/
/**
 * helm_set_numtimes() - Set numtimes register
 *
 * The numtimes register indicates the number of times to restart operations
 *
 * @dev:        Device pointer
 * @data:       Value to set (repetitions)
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int helm_set_numtimes(void *dev, uint32_t data);

/*****************************************************************************/
/**
 * helm_get_numtimes() - Get value of numtimes register
 *
 * @dev:        Device pointer
 * @data:       Pointer where to store the register value
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int helm_get_numtimes(void *dev, uint32_t *data);

/*****************************************************************************/
/**
 * helm_set_interruptconf() - Set interrupt configuration register
 *
 * @dev:        Device pointer
 * @data:       Value to set
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int helm_set_interruptconf(void *dev, uint32_t data);

/*****************************************************************************/
/**
 * helm_get_interruptconf() - Get value of interrupt configuration register
 *
 * @dev:        Device pointer
 * @data:       Pointer where to store the register value
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int helm_get_interruptconf(void *dev, uint32_t *data);

/*****************************************************************************/
/**
 * helm_get_interruptstatus() - Get value of interrupt status register
 *
 * @dev:        Device pointer
 * @data:       Pointer where to store the register value
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int helm_get_interruptstatus(void *dev, uint32_t *data);

/*****************************************************************************/
/**
 * helm_set_in() - Set value of input register
 *
 * It contains the address in memory where the imput data is stored
 *
 * @dev:        Device pointer
 * @data:       Value to set
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int helm_set_in(void *dev, uint64_t data);

/*****************************************************************************/
/**
 * helm_get_in() - Get value of input register
 *
 * @dev:        Device pointer
 * @data:       Pointer where to store the register value
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int helm_get_in(void *dev, uint64_t *data);

/*****************************************************************************/
/**
 * helm_set_out() - Set value of output register
 *
 * It contains the address in memory where the output data will be written
 *
 * @dev:        Device pointer
 * @data:       Value to set
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int helm_set_out(void *dev, uint64_t data);

/*****************************************************************************/
/**
 * helm_get_out() - Get value of output register
 *
 * @dev:        Device pointer
 * @data:       Pointer where to store the register value
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int helm_get_out(void *dev, uint64_t *data);

#ifdef DEBUG
/*****************************************************************************/
/**
 * helm_reg_dump() - Print the value of all the device registers
 *
 * @dev:        Device pointer
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int helm_reg_dump(void *dev);

/*****************************************************************************/
/**
 * helm_ctrl_dump() - Print the value of the control register and its fields
 *
 * @dev:        Device pointer
 *
 * Return:      0 on success, negative errno otherwise
 *
 *****************************************************************************/
int helm_ctrl_dump(void *dev);
#endif

#endif //#define HELM_DEV_H
