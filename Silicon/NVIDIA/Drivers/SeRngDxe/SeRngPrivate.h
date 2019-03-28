/** @file

  Tegra Se RNG Driver private structures

  Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

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

#define SE_MAX_POLL_COUNT          0x08000000U
#define RNG1_TIMEOUT               2000
#define RANDOM_BYTES               16U

#define SE_RNG_PRIVATE_DATA_FROM_THIS(a)   CR(a, SE_RNG_PRIVATE_DATA, SeRngProtocol, SE_RNG_SIGNATURE)

#define SE0_AES0_CONFIG_0                  0x1004

#define SE0_AES0_CONFIG_0_DST_SHIFT        2
#define SE0_AES0_CONFIG_0_DST_MEMORY       (0 << SE0_AES0_CONFIG_0_DST_SHIFT)

#define SE0_AES0_CONFIG_0_DEC_ALG_SHIFT    8
#define SE0_AES0_CONFIG_0_DEC_ALG_NOP      (0 << SE0_AES0_CONFIG_0_DEC_ALG_SHIFT)
#define SE0_AES0_CONFIG_0_ENC_ALG_SHIFT    12
#define SE0_AES0_CONFIG_0_ENC_ALG_RNG      (2 << SE0_AES0_CONFIG_0_ENC_ALG_SHIFT)

#define SE0_AES0_CONFIG_0_ENC_MODE_SHIFT   24
#define SE0_AES0_CONFIG_0_ENC_MODE__KEY256 (2 << SE0_AES0_CONFIG_0_ENC_MODE_SHIFT)


#define SE0_AES0_CRYPTO_CONFIG_0           0x1008

#define SE0_AES0_CRYPTO_CONFIG_0_XOR_POS_SHIFT    1
#define SE0_AES0_CRYPTO_CONFIG_0_XOR_POS_BYPASS   (0 << SE0_AES0_CRYPTO_CONFIG_0_XOR_POS_SHIFT)
#define SE0_AES0_CRYPTO_CONFIG_0_INPUT_SEL_SHIFT  3
#define SE0_AES0_CRYPTO_CONFIG_0_INPUT_SEL_RANDOM (1 << SE0_AES0_CRYPTO_CONFIG_0_INPUT_SEL_SHIFT)
#define SE0_AES0_CRYPTO_CONFIG_0_CORE_SEL_SHIFT   9
#define SE0_AES0_CRYPTO_CONFIG_0_CORE_SEL_ENCRYPT (1 << SE0_AES0_CRYPTO_CONFIG_0_CORE_SEL_SHIFT)
#define SE0_AES0_CRYPTO_CONFIG_0_HASH_ENB_SHIFT   0
#define SE0_AES0_CRYPTO_CONFIG_0_HASH_ENB_DISABLE (0 << SE0_AES0_CRYPTO_CONFIG_0_HASH_ENB_SHIFT)




#define SE0_AES0_OUT_ADDR_0                0x1014
#define SE0_AES0_OUT_ADDR_HI_0             0x1018
#define SE0_AES0_OUT_ADDR_HI_0_SZ_SHIFT    0
#define SE0_AES0_OUT_ADDR_HI_0_SZ_MASK     0x00FFFFFF
#define SE0_AES0_OUT_ADDR_HI_0_MSB_SHIFT   24
#define SE0_AES0_OUT_ADDR_HI_0_MSB_MASK    0xFF000000

#define SE0_AES0_CRYPTO_LAST_BLOCK_0       0x102c

#define SE0_AES0_RNG_CONFIG_0                   0x1034
#define SE0_AES0_RNG_CONFIG_0_SRC_SHIFT         2
#define SE0_AES0_RNG_CONFIG_0_SRC_ENTROPY       (1 << SE0_AES0_RNG_CONFIG_0_SRC_SHIFT)
#define SE0_AES0_RNG_CONFIG_0_MODE_SHIFT        0
#define SE0_AES0_RNG_CONFIG_0_MODE_FORCE_RESEED (2 << SE0_AES0_RNG_CONFIG_0_MODE_SHIFT)


#define SE0_AES0_OPERATION_0               0x1038
#define SE0_AES0_OPERATION_0_LASTBUF_FIELD BIT16

#define SE_UNIT_OPERATION_PKT_OP_START     BIT0

#define SE0_AES0_RNG_RESEED_INTERVAL_0     0x10dc
#define SE0_AES0_STATUS_0                  0x10f4

//Tegra RNG1 Registers
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
