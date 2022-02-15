/** @file

  Tegra Se RNG Driver private structures

  Copyright (c) 2019-2020, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SE_RNG_PRIVATE_H__
#define __SE_RNG_PRIVATE_H__

#include <Protocol/SeRngProtocol.h>
#include <PiDxe.h>

#define SE_RNG_SIGNATURE SIGNATURE_32('S','E','R','N')

typedef struct {
  UINT32                           Signature;
  UINT64                           BaseAddress;
  NVIDIA_SE_RNG_PROTOCOL           SeRngProtocol;
} SE_RNG_PRIVATE_DATA;

#define RNG1_TIMEOUT               2000

#define SE_RNG_PRIVATE_DATA_FROM_THIS(a)   CR(a, SE_RNG_PRIVATE_DATA, SeRngProtocol, SE_RNG_SIGNATURE)


#define TEGRA_SE_RNG1_CTRL_OFFSET           0xF00
#define RNG1_CMD_NOP                        0
#define RNG1_CMD_GEN_NOISE                  1
#define RNG1_CMD_GEN_NONCE                  2
#define RNG1_CMD_CREATE_STATE               3
#define RNG1_CMD_RENEW_STATE                4
#define RNG1_CMD_REFRESH_ADDIN              5
#define RNG1_CMD_GEN_RANDOM                 6
#define RNG1_CMD_ADVANCE_STATE              7
#define RNG1_CMD_KAT                        8
#define RNG1_CMD_ZEROIZE                    15


#define TEGRA_SE_RNG1_INT_EN_OFFSET         0xFC0
#define TEGRA_SE_RNG1_IE_OFFSET             0xF10

#define TEGRA_SE_RNG1_STATUS_OFFSET         0xF0C
#define TEGRA_SE_RNG1_STATUS_BUSY           BIT31

#define TEGRA_SE_RNG1_STATUS_SECURE         BIT6

#define TEGRA_SE_RNG1_ISTATUS_OFFSET        0xF14
#define TEGRA_SE_RNG1_ISTATUS_NOISE_RDY     BIT2
#define TEGRA_SE_RNG1_ISTATUS_DONE          BIT4
#define TEGRA_SE_RNG1_ISTATUS_KAT_COMPLETED BIT1
#define TEGRA_SE_RNG1_ISTATUS_ZEROIZED      BIT0

#define TEGRA_SE_RNG1_INT_STATUS_OFFSET     0xFC4
#define TEGRA_SE_RNG1_INT_STATUS_EIP0       BIT8

#define TEGRA_SE_RNG1_NPA_DATA0_OFFSET      0xF34

#define TEGRA_SE_RNG1_SE_MODE_OFFSET        0xF04
#define RNG1_MODE_ADDIN_PRESENT             BIT4
#define RNG1_MODE_SEC_ALG                   BIT0
#define RNG1_MODE_PRED_RESIST               BIT3

#define TEGRA_SE_RNG1_SE_SMODE_OFFSET       0xF08
#define TEGRA_SE_RNG1_SE_SMODE_SECURE       BIT1
#define TEGRA_SE_RNG1_SE_SMODE_NONCE        BIT0

#define TEGRA_SE_RNG1_RAND0_OFFSET          0xF24
#define TEGRA_SE_RNG1_ALARMS_OFFSET         0xF18


#endif
