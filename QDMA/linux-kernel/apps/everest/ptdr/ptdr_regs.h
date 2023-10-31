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

// 0x00 : Control signals
//        bit 0  - ap_start (Read/Write/COH)
//        bit 1  - ap_done (Read)
//        bit 2  - ap_idle (Read)
//        bit 3  - ap_ready (Read/COR)
//        bit 4  - ap_continue (Read/Write/SC)
//        bit 7  - auto_restart (Read/Write)
//        bit 9  - interrupt (Read)
//        others - reserved
// 0x04 : Global Interrupt Enable Register
//        bit 0  - Global Interrupt Enable (Read/Write)
//        others - reserved
// 0x08 : IP Interrupt Enable Register (Read/Write)
//        bit 0 - enable ap_done interrupt (Read/Write)
//        bit 1 - enable ap_ready interrupt (Read/Write)
//        others - reserved
// 0x0c : IP Interrupt Status Register (Read/COR)
//        bit 0 - ap_done (Read/COR)
//        bit 1 - ap_ready (Read/COR)
//        others - reserved
// 0x10 : Data signal of num_times
//        bit 31~0 - num_times[31:0] (Read/Write)
// 0x14 : reserved
// 0x18 : Data signal of durations
//        bit 31~0 - durations[31:0] (Read/Write)
// 0x1c : reserved
// 0x20 : Data signal of route
//        bit 31~0 - route[31:0] (Read/Write)
// 0x24 : reserved
// 0x28 : Data signal of position
//        bit 31~0 - posiiton[31:0] (Read/Write)
// 0x2c : reserved
// 0x30 : Data signal of departure
//        bit 31~0 - departure[31:0] (Read/Write)
// 0x34 : reserved
// 0x38 : Data signal of seed
//        bit 31~0 - seed[31:0] (Read/Write)
// 0x3c : reserved
// 0x40 : Data signal of base
//        bit 31~0 - base[31:0] (Read/Write)
// 0x44 : Data signal of base
//        bit 31~0 - base[63:32] (Read/Write)
// 0x48 : reserved
// 0x4c : reserved
//
// SC  = Self Clear
// COR = Clear on Read
// TOW = Toggle on Write
// COH = Clear on Handshake


#ifndef PTDR_REGS_H
#define PTDR_REGS_H

#define PTDR_CTRL_ADDR_CTRL             (0x00)
#define PTDR_CTRL_ADDR_GIE              (0x04)
#define PTDR_CTRL_ADDR_IER              (0x08)
#define PTDR_CTRL_ADDR_ISR              (0x0c)
#define PTDR_CTRL_ADDR_NUM_TIMES        (0x10)
#define PTDR_CTRL_ADDR_DUR              (0x18)
#define PTDR_CTRL_ADDR_ROUTE            (0x20)
#define PTDR_CTRL_ADDR_POS              (0x28)
#define PTDR_CTRL_ADDR_DEP              (0x30)
#define PTDR_CTRL_ADDR_SEED             (0x38)
#define PTDR_CTRL_ADDR_BASE             (0x40)

#endif //#define PTDR_REGS_H
