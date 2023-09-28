/*
 * Copyright (C) 2023 - Virtual Open Systems SAS - All rights reserved.
 * Reproduction and communication of this document is strictly prohibited
 * unless specifically authorized in writing by Virtual Open Systems.
 * ****************************************************************************
 * File Name   : helm_regs.h
 * Author      : STEFANO CIRICI <s.cirici@virtualopensystems.com>
 * Description : Helmoltz device registers description
 *
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
// 0x10 : Data signal of in_r
//        bit 31~0 - in_r[31:0] (Read/Write)
// 0x14 : Data signal of in_r
//        bit 31~0 - in_r[63:32] (Read/Write)
// 0x18 : reserved
// 0x1c : Data signal of out_r
//        bit 31~0 - out_r[31:0] (Read/Write)
// 0x20 : Data signal of out_r
//        bit 31~0 - out_r[63:32] (Read/Write)
// 0x24 : reserved
// 0x28 : Data signal of num_times
//        bit 31~0 - num_times[31:0] (Read/Write)
// 0x2c : reserved
//
// SC  = Self Clear
// COR = Clear on Read
// TOW = Toggle on Write
// COH = Clear on Handshake

#ifndef HELM_REGS_H
#define HELM_REGS_H

#define HELM_CTRL_ADDR_CTRL             (0x00)
#define HELM_CTRL_ADDR_GIE              (0x04)
#define HELM_CTRL_ADDR_IER              (0x08)
#define HELM_CTRL_ADDR_ISR              (0x0c)
#define HELM_CTRL_ADDR_IN_DATA          (0x10)
#define HELM_CTRL_ADDR_OUT_DATA         (0x1c)
#define HELM_CTRL_ADDR_NUM_TIMES        (0x28)

#endif //#define HELM_REGS_H
