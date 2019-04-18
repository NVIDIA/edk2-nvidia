/** @file

  XHCI Controller Driver private structures

  Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef XHCI_CONTROLLER_PRIVATE_H_
#define XHCI_CONTROLLER_PRIVATE_H_

#include <PiDxe.h>

#define NV_FIELD_MASK(x)        (0xFFFFFFFFUL>>(31-((1 ? x)%32)+((0 ? x)%32)))
#define NV_FIELD_SHIFT(x)          ((0 ? x)%32)

#ifndef _MK_ENUM_CONST
#define _MK_ENUM_CONST(_constant_) (_constant_ ## UL)
#endif

#ifndef _MK_ADDR_CONST
#define _MK_ADDR_CONST(_constant_) _constant_
#endif

#define NV_FIELD_SHIFTMASK(x)   (NV_FIELD_MASK(x) << (NV_FIELD_SHIFT(x)))

#define NV_DRF_DEF(d, r, f, c) \
        ((d##_##r##_0_##f##_##c) << NV_FIELD_SHIFT(d##_##r##_0_##f##_RANGE))

#define NV_FLD_SET_DRF_DEF(d, r, f, c, v) \
        (((v) & ~NV_FIELD_SHIFTMASK(d##_##r##_0_##f##_RANGE)) | \
        NV_DRF_DEF(d, r, f, c))
#define XUSB_CFG_1_0    _MK_ADDR_CONST(0x00000004)
#define XUSB_CFG_1_0_MEMORY_SPACE_RANGE 1 : 1
#define XUSB_CFG_1_0_MEMORY_SPACE_DISABLED      _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_MEMORY_SPACE_ENABLED       _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_BUS_MASTER_RANGE   2 : 2
#define XUSB_CFG_1_0_BUS_MASTER_DISABLED        _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_BUS_MASTER_ENABLED         _MK_ENUM_CONST(0x00000001)

#define XUSB_CFG_4_0    _MK_ADDR_CONST(0x00000010)

#define XUSB_OP_USBSTS  _MK_ADDR_CONST(0x00000004)
#define USBSTS_CNR      (1 << 11)
#define USBSTS_HCE      (1 << 12)
/* CFG 4 Address shift and Mask Values for T186 only. Later chips might have
 * different values  */
#define  CFG4_ADDR_SHIFT                   15
#define  CFG4_ADDR_MASK                    0x1ffff

#endif /* XHCI_CONTROLLER_PRIVATE_H_ */
