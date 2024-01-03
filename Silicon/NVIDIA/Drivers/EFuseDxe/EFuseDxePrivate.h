/** @file

  EFUSE Driver private structures

  SPDX-FileCopyrightText: Copyright (c) 2019-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef EFUSEDXE_PRIVATE_H_
#define EFUSEDXE_PRIVATE_H_

#include <PiDxe.h>
#include <Protocol/EFuse.h>

#define EFUSE_SIGNATURE  SIGNATURE_32('E','F','S','E')
typedef struct {
  UINT32                   Signature;
  NVIDIA_EFUSE_PROTOCOL    EFuseProtocol;
  EFI_PHYSICAL_ADDRESS     BaseAddress;
  UINTN                    RegionSize;
  EFI_HANDLE               ImageHandle;
} EFUSE_DXE_PRIVATE;
#define EFUSE_PRIVATE_DATA_FROM_THIS(a)      CR(a, EFUSE_DXE_PRIVATE, EFuseProtocol, EFUSE_SIGNATURE)
#define EFUSE_PRIVATE_DATA_FROM_PROTOCOL(a)  EFUSE_PRIVATE_DATA_FROM_THIS(a)

#define PMC_FUSE_CTRL_ENABLE_REDIRECTION_STICKY  (1U << 1)
#define FUSE_DISABLEREGPROGRAM_0_VAL_MASK        0x1
#define FUSE_STROBE_PROGRAMMING_PULSE            5

#define PMC_MISC_FUSE_CONTROL_0                           0x10
#define PMC_MISC_FUSE_CONTROL_0_ENABLE_REDIRECTION_RANGE  0:0
#define PMC_MISC_FUSE_CONTROL_0_PS18_LATCH_SET_RANGE      8:8
#define PMC_MISC_FUSE_CONTROL_0_PS18_LATCH_CLEAR_RANGE    9:9

#define FUSE_RESERVED_ODM8_ADDR_0        0x16
#define FUSE_RESERVED_ODM8_ADDR_0_MASK   0x3F
#define FUSE_RESERVED_ODM8_ADDR_0_SHIFT  26
#define FUSE_RESERVED_ODM8_ADDR_1        0x18
#define FUSE_RESERVED_ODM8_ADDR_1_MASK   0xFFFFFFC0
#define FUSE_RESERVED_ODM8_ADDR_1_SHIFT  (32 - 26)

#define FUSE_RESERVED_ODM8_REDUNDANT_ADDR_0        0x17
#define FUSE_RESERVED_ODM8_REDUNDANT_ADDR_0_MASK   0x3F
#define FUSE_RESERVED_ODM8_REDUNDANT_ADDR_0_SHIFT  26
#define FUSE_RESERVED_ODM8_REDUNDANT_ADDR_1        0x19
#define FUSE_RESERVED_ODM8_REDUNDANT_ADDR_1_MASK   0xFFFFFFC0
#define FUSE_RESERVED_ODM8_REDUNDANT_ADDR_1_SHIFT  (32 - 26)

#define FUSE_RESERVED_ODM9_ADDR_0        0x18
#define FUSE_RESERVED_ODM9_ADDR_0_MASK   0x3F
#define FUSE_RESERVED_ODM9_ADDR_0_SHIFT  26
#define FUSE_RESERVED_ODM9_ADDR_1        0x1A
#define FUSE_RESERVED_ODM9_ADDR_1_MASK   0xFFFFFFC0
#define FUSE_RESERVED_ODM9_ADDR_1_SHIFT  (32 - 26)

#define FUSE_RESERVED_ODM9_REDUNDANT_ADDR_0        0x19
#define FUSE_RESERVED_ODM9_REDUNDANT_ADDR_0_MASK   0x3F
#define FUSE_RESERVED_ODM9_REDUNDANT_ADDR_0_SHIFT  26
#define FUSE_RESERVED_ODM9_REDUNDANT_ADDR_1        0x1B
#define FUSE_RESERVED_ODM9_REDUNDANT_ADDR_1_MASK   0xFFFFFFC0
#define FUSE_RESERVED_ODM9_REDUNDANT_ADDR_1_SHIFT  (32 - 26)

#define FUSE_RESERVED_ODM10_ADDR_0        0x1A
#define FUSE_RESERVED_ODM10_ADDR_0_MASK   0x3F
#define FUSE_RESERVED_ODM10_ADDR_0_SHIFT  26
#define FUSE_RESERVED_ODM10_ADDR_1        0x1C
#define FUSE_RESERVED_ODM10_ADDR_1_MASK   0xFFFFFFC0
#define FUSE_RESERVED_ODM10_ADDR_1_SHIFT  (32 - 26)

#define FUSE_RESERVED_ODM10_REDUNDANT_ADDR_0        0x1B
#define FUSE_RESERVED_ODM10_REDUNDANT_ADDR_0_MASK   0x3F
#define FUSE_RESERVED_ODM10_REDUNDANT_ADDR_0_SHIFT  26
#define FUSE_RESERVED_ODM10_REDUNDANT_ADDR_1        0x1D
#define FUSE_RESERVED_ODM10_REDUNDANT_ADDR_1_MASK   0xFFFFFFC0
#define FUSE_RESERVED_ODM10_REDUNDANT_ADDR_1_SHIFT  (32 - 26)

#define FUSE_RESERVED_ODM11_ADDR_0        0x1C
#define FUSE_RESERVED_ODM11_ADDR_0_MASK   0x3F
#define FUSE_RESERVED_ODM11_ADDR_0_SHIFT  26
#define FUSE_RESERVED_ODM11_ADDR_1        0x1E
#define FUSE_RESERVED_ODM11_ADDR_1_MASK   0xFFFFFFC0
#define FUSE_RESERVED_ODM11_ADDR_1_SHIFT  (32 - 26)

#define FUSE_RESERVED_ODM11_REDUNDANT_ADDR_0        0x1D
#define FUSE_RESERVED_ODM11_REDUNDANT_ADDR_0_MASK   0x3F
#define FUSE_RESERVED_ODM11_REDUNDANT_ADDR_0_SHIFT  26
#define FUSE_RESERVED_ODM11_REDUNDANT_ADDR_1        0x1F
#define FUSE_RESERVED_ODM11_REDUNDANT_ADDR_1_MASK   0xFFFFFFC0
#define FUSE_RESERVED_ODM11_REDUNDANT_ADDR_1_SHIFT  (32 - 26)

#define FUSE_FUSECTRL_0                                        0x0
#define FUSE_FUSECTRL_0_FUSECTRL_CMD_READ                      1UL
#define FUSE_FUSECTRL_0_FUSECTRL_CMD_WRITE                     2UL
#define FUSE_FUSECTRL_0_FUSECTRL_CMD_SENSE_CTRL                3UL
#define FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE              4UL
#define FUSE_FUSECTRL_0_FUSECTRL_CMD_RANGE                     1:0
#define FUSE_FUSECTRL_0_FUSECTRL_STATE_RANGE                   20:16
#define FUSE_FUSECTRL_0_FUSECTRL_PD_CTRL_RANGE                 26:26
#define FUSE_FUSECTRL_0_FUSECTRL_DISABLE_MIRROR_RANGE          28:28
#define FUSE_FUSECTRL_0_FUSECTRL_FUSE_SENSE_DONE_RANGE         30:30
#define FUSE_PRIV2INTFC_START_0_PRIV2INTFC_START_DATA_RANGE    0:0
#define FUSE_PRIV2INTFC_START_0_PRIV2INTFC_SKIP_RECORDS_RANGE  1:1
#define FUSE_PRIV2INTFC_START_0                                0x20
#define FUSE_FUSEADDR_0                                        0x4
#define FUSE_FUSERDATA_0                                       0x8
#define FUSE_FUSEWDATA_0                                       0xc
#define FUSE_DISABLEREGPROGRAM_0                               0x2c
#define FUSE_FUSETIME_PGM2_0                                   0x1c
#define FUSE_FUSETIME_PGM2_0_FUSETIME_PGM2_TWIDTH_PGM_RANGE    15:0
#define FUSE_WRITE_ACCESS_SW_0                                 0x30
#define FUSE_WRITE_ACCESS_SW_0_WRITE_ACCESS_SW_CTRL_RANGE      0:0
#define FUSE_RESERVED_ODM8_0                                   0x520
#define FUSE_RESERVED_ODM9_0                                   0x524
#define FUSE_RESERVED_ODM10_0                                  0x528
#define FUSE_RESERVED_ODM11_0                                  0x52c

#define NV_FIELD_SHIFT(x)      ((0 ? x)%32)
#define NV_FIELD_MASK(x)       (0xFFFFFFFFUL>>(31-((1 ? x)%32)+((0 ? x)%32)))
#define NV_FIELD_SHIFTMASK(x)  (NV_FIELD_MASK(x) << (NV_FIELD_SHIFT(x)))

#define NV_DRF_DEF(d, r, f, c) \
        ((d##_##r##_0_##f##_##c) << NV_FIELD_SHIFT(d##_##r##_0_##f##_RANGE))

#define NV_FLD_SET_DRF_DEF(d, r, f, c, v) \
        (((v) & ~NV_FIELD_SHIFTMASK(d##_##r##_0_##f##_RANGE)) | \
        NV_DRF_DEF(d, r, f, c))

#define NV_DRF_VAL(d, r, f, v) \
        (((v) >> NV_FIELD_SHIFT(d##_##r##_0_##f##_RANGE)) & \
        NV_FIELD_MASK(d##_##r##_0_##f##_RANGE))

#define NV_DRF_NUM(d, r, f, n) \
        (((n) & NV_FIELD_MASK(d##_##r##_0_##f##_RANGE)) << \
        NV_FIELD_SHIFT(d##_##r##_0_##f##_RANGE))

#define NV_FLD_SET_DRF_NUM(d, r, f, n, v) \
        (((v) & ~NV_FIELD_SHIFTMASK(d##_##r##_0_##f##_RANGE)) \
        | NV_DRF_NUM(d, r, f, n))

/**
  Burns the desired fuse.

  @param[in]  BaseAddressbase    Base address of fuse register
  @param[in]  RegisterOffset     Offset of the fuse to be burnt
  @param[in]  Buffer             Buffer data with which the fuse is to be burnt
  @param[in]  Size               Size (in bytes) of the fuse to be burnt

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.
**/
EFI_STATUS
EFuseWrite (
  EFI_PHYSICAL_ADDRESS  BaseAddress,
  UINT32                RegisterOffset,
  UINT32                *Buffer,
  UINT32                Size
  );

#endif /* EFUSEDXE_PRIVATE_H_ */
