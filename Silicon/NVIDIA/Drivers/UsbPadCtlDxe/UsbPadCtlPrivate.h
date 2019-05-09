/** @file

  USB Pad Control Driver private structures

  Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#ifndef USB_PADCTL_PRIVATE_H_
#define USB_PADCTL_PRIVATE_H_

#include <PiDxe.h>
#include <Protocol/UsbPadCtl.h>

#define REG_VDD_USB0_5V  0xB3
#define REG_VDD_USB1_5V  0xB4

#ifndef _MK_ADDR_CONST
#define _MK_ADDR_CONST(_constant_) _constant_
#endif

#ifndef _MK_ENUM_CONST
#define _MK_ENUM_CONST(_constant_) (_constant_ ## UL)
#endif

#ifndef _MK_MASK_CONST
#define _MK_MASK_CONST(_constant_) _constant_
#endif

#ifndef _MK_SHIFT_CONST
#define _MK_SHIFT_CONST(_constant_) _constant_
#endif


#define NV_FIELD_LOWBIT(x)       (0 ? x)
#define NV_FIELD_HIGHBIT(x)      (1 ? x)
#define NV_FIELD_SIZE(x)         (NV_FIELD_HIGHBIT(x)-NV_FIELD_LOWBIT(x)+1)
#define NV_FIELD_SHIFT(x)        ((0 ? x)%32)
#define NV_FIELD_MASK(x)         (0xFFFFFFFFUL>>(31-((1 ? x)%32)+((0 ? x)%32)))
#define NV_FIELD_BITS(val, x)    (((val) & NV_FIELD_MASK(x)) << NV_FIELD_SHIFT(x))
#define NV_FIELD_SHIFTMASK(x)    (NV_FIELD_MASK(x) << (NV_FIELD_SHIFT(x)))

#define NV_PF_VAL(p, f, v) \
        (((v) >> NV_FIELD_SHIFT(p##_##f##_RANGE)) & \
        NV_FIELD_MASK(p##_##f##_RANGE))

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

#define NV_XUSB_PADCTL_READ(reg, value) \
        value = MmioRead32((NV_ADDRESS_MAP_APB_XUSB_PADCTL_BASE \
        + XUSB_PADCTL_##reg##_0))

#define NV_XUSB_PADCTL_WRITE(reg, value) \
        MmioWrite32((NV_ADDRESS_MAP_APB_XUSB_PADCTL_BASE \
        + XUSB_PADCTL_##reg##_0), value)

#define NV_ADDRESS_MAP_FUSE_BASE                                             58851328
#define NV_ADDRESS_MAP_CAR_BASE                                              1610637312
#define NV_ADDRESS_MAP_APB_XUSB_PADCTL_BASE                                  55705600
#define FUSE_USB_CALIB_0                                                     _MK_ADDR_CONST(0x1f0)
#define FUSE_USB_CALIB_EXT_0                                                 _MK_ADDR_CONST(0x350)
#define FUSE_USB_CALIB_0_HS_CURR_LEVEL_RANGE                                 5 : 0
#define FUSE_USB_CALIB_0_HS_CURR_LEVEL_P0_RANGE                              5 : 0
#define FUSE_USB_CALIB_0_HS_CURR_LEVEL_P1_RANGE                              16 : 11
#define FUSE_USB_CALIB_0_HS_CURR_LEVEL_P2_RANGE                              22 : 17
#define FUSE_USB_CALIB_0_HS_CURR_LEVEL_P3_RANGE                              28 : 23
/* This is just the old name for CURR_LEVEL */
#define FUSE_USB_CALIB_0_SETUP_RANGE                                         5 : 0
#define FUSE_USB_CALIB_0_TERM_RANGE_ADJ_RANGE                                10 : 7
#define FUSE_USB_CALIB_0_HS_SQUELCH_RANGE                                    31 : 29
#define FUSE_USB_CALIB_EXT_0_RPD_CTRL_RANGE                                  4 : 0
#define FUSE_USB_CALIB_HS_CURR_LEVEL_MASK                                    0x3F
#define FUSE_USB_CALIB_TERMRANGEADJ_MASK                                     0x780
#define FUSE_USB_CALIB_TERMRANGEADJ_SHIFT                                    7
#define FUSE_USB_CALIB_EXT_RPD_CTRL_MASK                                     0x1F

#define XUSB_HOST_CONFIGURATION_0                                            _MK_ADDR_CONST(0x180)
#define XUSB_HOST_CONFIGURATION_0_EN_FPCI_RANGE                              0 : 0

#define XUSB_PADCTL_USB2_PAD_MUX_0                                           _MK_ADDR_CONST(0x4)
#define XUSB_PADCTL_USB2_PAD_MUX_0_USB2_BIAS_PAD_RANGE                       19 : 18
#define XUSB_PADCTL_USB2_PAD_MUX_0_USB2_BIAS_PAD_XUSB                        _MK_ENUM_CONST(1)
#define XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT0_RANGE                  1 : 0
#define XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT0_XUSB                   _MK_ENUM_CONST(1)
#define XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT1_RANGE                  3 : 2
#define XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT1_XUSB                   _MK_ENUM_CONST(1)
#define XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT2_RANGE                  5 : 4
#define XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT2_XUSB                   _MK_ENUM_CONST(1)
#define XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT3_RANGE                  7 : 6
#define XUSB_PADCTL_USB2_PAD_MUX_0_USB2_OTG_PAD_PORT3_XUSB                   _MK_ENUM_CONST(1)

#define XUSB_PADCTL_USB2_PORT_CAP_0                                          _MK_ADDR_CONST(0x8)
#define XUSB_PADCTL_USB2_PORT_CAP_0_PORT0_CAP_RANGE                          1 : 0
#define XUSB_PADCTL_USB2_PORT_CAP_0_PORT0_CAP_HOST_ONLY                      _MK_ENUM_CONST(1)
#define XUSB_PADCTL_USB2_PORT_CAP_0_PORT1_CAP_RANGE                          5 : 4
#define XUSB_PADCTL_USB2_PORT_CAP_0_PORT1_CAP_HOST_ONLY                      _MK_ENUM_CONST(1)
#define XUSB_PADCTL_USB2_PORT_CAP_0_PORT2_CAP_RANGE                          9 : 8
#define XUSB_PADCTL_USB2_PORT_CAP_0_PORT2_CAP_HOST_ONLY                      _MK_ENUM_CONST(1)
#define XUSB_PADCTL_USB2_PORT_CAP_0_PORT3_CAP_RANGE                          13 : 12
#define XUSB_PADCTL_USB2_PORT_CAP_0_PORT3_CAP_HOST_ONLY                      _MK_ENUM_CONST(1)
#define XUSB_PADCTL_SNPS_OC_MAP_0                                            _MK_ADDR_CONST(0xc)

#define XUSB_PADCTL_SS_PORT_CAP_0                                            _MK_ADDR_CONST(0xc)
#define XUSB_PADCTL_SS_PORT_CAP_0_PORT0_CAP_RANGE                            1 : 0
#define XUSB_PADCTL_SS_PORT_CAP_0_PORT1_CAP_RANGE                            5 : 4
#define XUSB_PADCTL_SS_PORT_CAP_0_PORT2_CAP_RANGE                            9 : 8

#define XUSB_PADCTL_USB2_OC_MAP_0                                            _MK_ADDR_CONST(0x10)
#define XUSB_PADCTL_USB2_OC_MAP_0_PORT0_OC_PIN_RANGE                         3 : 0
#define XUSB_PADCTL_USB2_OC_MAP_0_PORT0_OC_PIN_OC_DETECTION_DISABLED         _MK_ENUM_CONST(15)
#define XUSB_PADCTL_USB2_OC_MAP_0_PORT0_OC_PIN_OC_DETECTED0                  _MK_ENUM_CONST(0)
#define XUSB_PADCTL_USB2_OC_MAP_0_PORT0_OC_PIN_OC_DETECTED1                  _MK_ENUM_CONST(1)
#define XUSB_PADCTL_USB2_OC_MAP_0_PORT1_OC_PIN_RANGE                         7 : 4
#define XUSB_PADCTL_USB2_OC_MAP_0_PORT1_OC_PIN_OC_DETECTED0                  _MK_ENUM_CONST(0)
#define XUSB_PADCTL_USB2_OC_MAP_0_PORT1_OC_PIN_OC_DETECTED1                  _MK_ENUM_CONST(1)
#define XUSB_PADCTL_USB2_OC_MAP_0_PORT1_OC_PIN_OC_DETECTION_DISABLED         _MK_ENUM_CONST(15)
#define XUSB_PADCTL_USB2_OC_MAP_0_PORT2_OC_PIN_RANGE                         11 : 8
#define XUSB_PADCTL_USB2_OC_MAP_0_PORT2_OC_PIN_OC_DETECTED0                  _MK_ENUM_CONST(0)
#define XUSB_PADCTL_USB2_OC_MAP_0_PORT2_OC_PIN_OC_DETECTED1                  _MK_ENUM_CONST(1)
#define XUSB_PADCTL_USB2_OC_MAP_0_PORT2_OC_PIN_OC_DETECTED2                  _MK_ENUM_CONST(2)
#define XUSB_PADCTL_USB2_OC_MAP_0_PORT2_OC_PIN_OC_DETECTION_DISABLED         _MK_ENUM_CONST(15)
#define XUSB_PADCTL_USB2_OC_MAP_0_PORT3_OC_PIN_RANGE                         15 : 12
#define XUSB_PADCTL_USB2_OC_MAP_0_PORT3_OC_PIN_OC_DETECTED0                  _MK_ENUM_CONST(0)
#define XUSB_PADCTL_USB2_OC_MAP_0_PORT3_OC_PIN_OC_DETECTED1                  _MK_ENUM_CONST(1)
#define XUSB_PADCTL_USB2_OC_MAP_0_PORT3_OC_PIN_OC_DETECTED3                  _MK_ENUM_CONST(3)
#define XUSB_PADCTL_USB2_OC_MAP_0_PORT3_OC_PIN_OC_DETECTION_DISABLED         _MK_ENUM_CONST(15)
#define XUSB_PADCTL_SS_PORT_MAP_0                                            _MK_ADDR_CONST(0x14)

#define XUSB_PADCTL_SS_OC_MAP_0                                              _MK_ADDR_CONST(0x14)
#define XUSB_PADCTL_SS_OC_MAP_0_PORT0_OC_PIN_RANGE                           3 : 0
#define XUSB_PADCTL_SS_OC_MAP_0_PORT0_OC_PIN_OC_DETECTED0                    _MK_ENUM_CONST(0)
#define XUSB_PADCTL_SS_OC_MAP_0_PORT1_OC_PIN_RANGE                           7 : 4
#define XUSB_PADCTL_SS_OC_MAP_0_PORT1_OC_PIN_OC_DETECTED1                    _MK_ENUM_CONST(1)
#define XUSB_PADCTL_SS_OC_MAP_0_PORT2_OC_PIN_RANGE                           11 : 8
#define XUSB_PADCTL_SS_OC_MAP_0_PORT2_OC_PIN_OC_DETECTED2                    _MK_ENUM_CONST(2)
#define XUSB_PADCTL_SS_OC_MAP_0_PORT3_OC_PIN_RANGE                           15 : 12
#define XUSB_PADCTL_SS_OC_MAP_0_PORT3_OC_PIN_OC_DETECTED3                    _MK_ENUM_CONST(3)

#define XUSB_PADCTL_VBUS_OC_MAP_0                                            _MK_ADDR_CONST(0x18)
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE0_RANGE                         0 : 0
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE0_NO                            _MK_ENUM_CONST(0)
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE0_YES                           _MK_ENUM_CONST(1)
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE0_OC_MAP_RANGE                  4 : 1
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE0_OC_MAP_OC_DETECTION_DISABLED  _MK_ENUM_CONST(15)
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE0_OC_MAP_OC_DETECTED0           _MK_ENUM_CONST(0)
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE1_RANGE                         5 : 5
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE1_NO                            _MK_ENUM_CONST(0)
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE1_YES                           _MK_ENUM_CONST(1)
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE1_OC_MAP_RANGE                  9 : 6
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE1_OC_MAP_OC_DETECTED1           _MK_ENUM_CONST(1)
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE1_OC_MAP_OC_DETECTION_DISABLED  _MK_ENUM_CONST(15)
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE2_RANGE                         10 : 10
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE2_NO                            _MK_ENUM_CONST(0)
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE2_YES                           _MK_ENUM_CONST(1)
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE2_OC_MAP_RANGE                  14 : 11
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE2_OC_MAP_OC_DETECTED1           _MK_ENUM_CONST(1)
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE2_OC_MAP_OC_DETECTION_DISABLED  _MK_ENUM_CONST(15)
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE3_RANGE                         15 : 15
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE3_NO                            _MK_ENUM_CONST(0)
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE3_YES                           _MK_ENUM_CONST(1)
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE3_OC_MAP_RANGE                  19 : 16
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE3_OC_MAP_OC_DETECTED1           _MK_ENUM_CONST(1)
#define XUSB_PADCTL_VBUS_OC_MAP_0_VBUS_ENABLE3_OC_MAP_OC_DETECTION_DISABLED  _MK_ENUM_CONST(15)

#define XUSB_PADCTL_OC_DET_0                                                 _MK_ADDR_CONST(0x1c)
#define XUSB_PADCTL_OC_DET_0_SET_OC_DETECTED0_RANGE                          0 : 0
#define XUSB_PADCTL_OC_DET_0_SET_OC_DETECTED0_NO                             _MK_ENUM_CONST(0)
#define XUSB_PADCTL_OC_DET_0_SET_OC_DETECTED0_YES                            _MK_ENUM_CONST(1)
#define XUSB_PADCTL_OC_DET_0_SET_OC_DETECTED1_RANGE                          1 : 1
#define XUSB_PADCTL_OC_DET_0_SET_OC_DETECTED1_NO                             _MK_ENUM_CONST(0)
#define XUSB_PADCTL_OC_DET_0_SET_OC_DETECTED1_YES                            _MK_ENUM_CONST(1)
#define XUSB_PADCTL_OC_DET_0_SET_OC_DETECTED2_RANGE                          2 : 2
#define XUSB_PADCTL_OC_DET_0_SET_OC_DETECTED2_NO                             _MK_ENUM_CONST(0)
#define XUSB_PADCTL_OC_DET_0_SET_OC_DETECTED2_YES                            _MK_ENUM_CONST(1)
#define XUSB_PADCTL_OC_DET_0_SET_OC_DETECTED3_RANGE                          3 : 3
#define XUSB_PADCTL_OC_DET_0_SET_OC_DETECTED3_NO                             _MK_ENUM_CONST(0)
#define XUSB_PADCTL_OC_DET_0_SET_OC_DETECTED3_YES                            _MK_ENUM_CONST(1)
#define XUSB_PADCTL_OC_DET_0_OC_DETECTED0_RANGE                              8 : 8
#define XUSB_PADCTL_OC_DET_0_OC_DETECTED0_YES                                _MK_ENUM_CONST(1)
#define XUSB_PADCTL_OC_DET_0_OC_DETECTED1_RANGE                              9 : 9
#define XUSB_PADCTL_OC_DET_0_OC_DETECTED1_YES                                _MK_ENUM_CONST(1)
#define XUSB_PADCTL_OC_DET_0_OC_DETECTED2_RANGE                              10 : 10
#define XUSB_PADCTL_OC_DET_0_OC_DETECTED2_YES                                _MK_ENUM_CONST(1)
#define XUSB_PADCTL_OC_DET_0_OC_DETECTED3_RANGE                              11 : 11
#define XUSB_PADCTL_OC_DET_0_OC_DETECTED3_YES                                _MK_ENUM_CONST(1)
#define XUSB_PADCTL_OC_DET_0_OC_DETECTED_VBUS_PAD0_RANGE                     12 : 12
#define XUSB_PADCTL_OC_DET_0_OC_DETECTED_VBUS_PAD0_NO                        _MK_ENUM_CONST(0)
#define XUSB_PADCTL_OC_DET_0_OC_DETECTED_VBUS_PAD0_YES                       _MK_ENUM_CONST(1)
#define XUSB_PADCTL_OC_DET_0_OC_DETECTED_VBUS_PAD1_RANGE                     13 : 13
#define XUSB_PADCTL_OC_DET_0_OC_DETECTED_VBUS_PAD1_NO                        _MK_ENUM_CONST(0)
#define XUSB_PADCTL_OC_DET_0_OC_DETECTED_VBUS_PAD1_YES                       _MK_ENUM_CONST(1)
#define XUSB_PADCTL_OC_DET_0_OC_DETECTED_VBUS_PAD2_RANGE                     14 : 14
#define XUSB_PADCTL_OC_DET_0_OC_DETECTED_VBUS_PAD2_NO                        _MK_ENUM_CONST(0)
#define XUSB_PADCTL_OC_DET_0_OC_DETECTED_VBUS_PAD2_YES                       _MK_ENUM_CONST(1)
#define XUSB_PADCTL_OC_DET_0_OC_DETECTED_VBUS_PAD3_RANGE                     15 : 15
#define XUSB_PADCTL_OC_DET_0_OC_DETECTED_VBUS_PAD3_NO                        _MK_ENUM_CONST(0)
#define XUSB_PADCTL_OC_DET_0_OC_DETECTED_VBUS_PAD3_YES                       _MK_ENUM_CONST(1)

#define XUSB_PADCTL_ELPG_PROGRAM_1_0                                         _MK_ADDR_CONST(0x24)
#define XUSB_PADCTL_ELPG_PROGRAM_1_0_SSP0_ELPG_CLAMP_EN_RANGE                0 : 0
#define XUSB_PADCTL_ELPG_PROGRAM_1_0_SSP0_ELPG_CLAMP_EN_EARLY_RANGE          1 : 1
#define XUSB_PADCTL_ELPG_PROGRAM_1_0_SSP0_ELPG_VCORE_DOWN_RANGE              2 : 2
#define XUSB_PADCTL_ELPG_PROGRAM_1_0_SSP1_ELPG_CLAMP_EN_RANGE                3 : 3
#define XUSB_PADCTL_ELPG_PROGRAM_1_0_SSP1_ELPG_CLAMP_EN_EARLY_RANGE          4 : 4
#define XUSB_PADCTL_ELPG_PROGRAM_1_0_SSP1_ELPG_VCORE_DOWN_RANGE              5 : 5
#define XUSB_PADCTL_ELPG_PROGRAM_1_0_SSP2_ELPG_CLAMP_EN_RANGE                6 : 6
#define XUSB_PADCTL_ELPG_PROGRAM_1_0_SSP2_ELPG_CLAMP_EN_EARLY_RANGE          7 : 7
#define XUSB_PADCTL_ELPG_PROGRAM_1_0_SSP2_ELPG_VCORE_DOWN_RANGE              8 : 8
#define XUSB_PADCTL_ELPG_PROGRAM_1_0_SSP3_ELPG_CLAMP_EN_RANGE                9 : 9
#define XUSB_PADCTL_ELPG_PROGRAM_1_0_SSP3_ELPG_CLAMP_EN_EARLY_RANGE          10 : 10
#define XUSB_PADCTL_ELPG_PROGRAM_1_0_SSP3_ELPG_VCORE_DOWN_RANGE              11 : 11

#define XUSB_PADCTL_USB3_PAD_MUX_0                                           _MK_ADDR_CONST(0x28)

#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0_0                         _MK_ADDR_CONST(0x80)
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0_0_PD_CHG_RANGE            0 : 0
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0_0_PD_CHG_NO               _MK_ENUM_CONST(0)
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL0_0_PD_CHG_YES              _MK_ENUM_CONST(1)

#define VREG_DIR_IN                                                          0x1
#define VREG_DIR_OUT                                                         0x2
#define VREG_LEVEL_500MA                                                     0x0
#define VREG_LEVEL_900MA                                                     0x1
#define VREG_LEVEL_2A                                                        0x3

#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0                         _MK_ADDR_CONST(0x84)
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0_VREG_FIX18_RANGE        6 : 6
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0_VREG_DIR_RANGE          12 : 11
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0_VREG_LEV_RANGE          8 : 7
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD0_CTL1_0_PD_VREG_RANGE           6 : 6

#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD1_CTL0_0                         _MK_ADDR_CONST(0xc0)
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD1_CTL0_0_PD_CHG_RANGE            0 : 0
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD1_CTL0_0_PD_CHG_NO               _MK_ENUM_CONST(0)
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD1_CTL0_0_PD_CHG_YES              _MK_ENUM_CONST(1)

#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD1_CTL1_0                         _MK_ADDR_CONST(0xc4)
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD1_CTL1_0_VREG_FIX18_RANGE        6 : 6
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD1_CTL1_0_VREG_DIR_RANGE          12 : 11
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD1_CTL1_0_VREG_LEV_RANGE          8 : 7
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD1_CTL1_0_PD_VREG_RANGE           6 : 6

#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD2_CTL0_0                         _MK_ADDR_CONST(0x100)
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD2_CTL0_0_PD_CHG_RANGE            0 : 0
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD2_CTL0_0_PD_CHG_NO               _MK_ENUM_CONST(0)
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD2_CTL0_0_PD_CHG_YES              _MK_ENUM_CONST(1)

#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD2_CTL1_0                         _MK_ADDR_CONST(0x104)
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD2_CTL1_0_VREG_FIX18_RANGE        6 : 6
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD2_CTL1_0_PD_VREG_RANGE           6 : 6
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD2_CTL1_0_VREG_DIR_RANGE          12 : 11
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD2_CTL1_0_VREG_DIR_RANGE          12 : 11
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD2_CTL1_0_VREG_LEV_RANGE          8 : 7

#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD3_CTL0_0                         _MK_ADDR_CONST(0x140)
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD3_CTL0_0_PD_CHG_RANGE            0 : 0
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD3_CTL0_0_PD_CHG_NO               _MK_ENUM_CONST(0)
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD3_CTL0_0_PD_CHG_YES              _MK_ENUM_CONST(1)

#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD3_CTL1_0                         _MK_ADDR_CONST(0x144)
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD3_CTL1_0_VREG_FIX18_RANGE        6 : 6
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD3_CTL1_0_VREG_DIR_RANGE          12 : 11
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD3_CTL1_0_VREG_LEV_RANGE          8 : 7
#define XUSB_PADCTL_USB2_BATTERY_CHRG_OTGPAD3_CTL1_0_PD_VREG_RANGE           6 : 6

#define XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0                                    _MK_ADDR_CONST(0x88)
#define XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_HS_CURR_LEVEL_RANGE                5 : 0
#define XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_LS_RSLEW_RANGE                     20 : 17
#define XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_LS_FSLEW_RANGE                     24 : 21
#define XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_TERM_SEL_RANGE                     25 : 25
#define XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_PD_RANGE                           26 : 26
#define XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_PD_SW_DEFAULT                      _MK_MASK_CONST(0x0)
#define XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_PD_ZI_RANGE                        29 : 29
#define XUSB_PADCTL_USB2_OTG_PAD0_CTL_0_0_PD_ZI_SW_DEFAULT                   _MK_MASK_CONST(0x0)

#define XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0                                    _MK_ADDR_CONST(0x8c)
#define XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0_PD_DR_RANGE                        2 : 2
#define XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0_PD_DR_SW_DEFAULT                   _MK_MASK_CONST(0x0)
#define XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0_TERM_RANGE_ADJ_RANGE               6 : 3
#define XUSB_PADCTL_USB2_OTG_PAD0_CTL_1_0_RPD_CTRL_RANGE                     30 : 26

#define XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0                                    _MK_ADDR_CONST(0xc8)
#define XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_HS_CURR_LEVEL_RANGE                5 : 0
#define XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_LS_RSLEW_RANGE                     20 : 17
#define XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_LS_FSLEW_RANGE                     24 : 21
#define XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_TERM_SEL_RANGE                     25 : 25
#define XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_PD_RANGE                           26 : 26
#define XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_PD_SW_DEFAULT                      _MK_MASK_CONST(0x0)
#define XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_PD_ZI_RANGE                        29 : 29
#define XUSB_PADCTL_USB2_OTG_PAD1_CTL_0_0_PD_ZI_SW_DEFAULT                   _MK_MASK_CONST(0x0)

#define XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0                                    _MK_ADDR_CONST(0xcc)
#define XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0_PD_DR_RANGE                        2 : 2
#define XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0_PD_DR_SW_DEFAULT                   _MK_MASK_CONST(0x0)
#define XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0_TERM_RANGE_ADJ_RANGE               6 : 3
#define XUSB_PADCTL_USB2_OTG_PAD1_CTL_1_0_RPD_CTRL_RANGE                     30 : 26

#define XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0                                    _MK_ADDR_CONST(0x108)
#define XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_HS_CURR_LEVEL_RANGE                5 : 0
#define XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_LS_RSLEW_RANGE                     20 : 17
#define XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_LS_FSLEW_RANGE                     24 : 21
#define XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_TERM_SEL_RANGE                     25 : 25
#define XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_PD_RANGE                           26 : 26
#define XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_PD_SW_DEFAULT                      _MK_MASK_CONST(0x0)
#define XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_PD_ZI_RANGE                        29 : 29
#define XUSB_PADCTL_USB2_OTG_PAD2_CTL_0_0_PD_ZI_SW_DEFAULT                   _MK_MASK_CONST(0x0)

#define XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0                                    _MK_ADDR_CONST(0x10c)
#define XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0_PD_DR_RANGE                        2 : 2
#define XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0_PD_DR_SW_DEFAULT                   _MK_MASK_CONST(0x0)
#define XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0_TERM_RANGE_ADJ_RANGE               6 : 3
#define XUSB_PADCTL_USB2_OTG_PAD2_CTL_1_0_RPD_CTRL_RANGE                     30 : 26

#define XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0                                    _MK_ADDR_CONST(0x148)
#define XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0_HS_CURR_LEVEL_RANGE                5 : 0
#define XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0_LS_RSLEW_RANGE                     20 : 17
#define XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0_LS_FSLEW_RANGE                     24 : 21
#define XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0_TERM_SEL_RANGE                     25 : 25
#define XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0_PD_RANGE                           26 : 26
#define XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0_PD_SW_DEFAULT                      _MK_MASK_CONST(0x0)
#define XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0_PD_ZI_RANGE                        29 : 29
#define XUSB_PADCTL_USB2_OTG_PAD3_CTL_0_0_PD_ZI_SW_DEFAULT                   _MK_MASK_CONST(0x0)

#define XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0                                    _MK_ADDR_CONST(0x14c)
#define XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0_PD_DR_RANGE                        2 : 2
#define XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0_PD_DR_SW_DEFAULT                   _MK_MASK_CONST(0x0)
#define XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0_TERM_RANGE_ADJ_RANGE               6 : 3
#define XUSB_PADCTL_USB2_OTG_PAD3_CTL_1_0_RPD_CTRL_RANGE                     30 : 26

#define XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0                                    _MK_ADDR_CONST(0x284)
#define XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0_PD_RANGE                           11 : 11
#define XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0_PD_SW_DEFAULT                      _MK_MASK_CONST(0x0)
#define XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0_HS_SQUELCH_LEVEL_RANGE             2 : 0
#define XUSB_PADCTL_USB2_BIAS_PAD_CTL_0_0_HS_DISCON_LEVEL_RANGE              5 : 3

#define XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0                                    _MK_ADDR_CONST(0x288)
#define XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0_PD_TRK_RANGE                       26 : 26
#define XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0_PD_TRK_SW_DEFAULT                  _MK_MASK_CONST(0x0)
#define XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0_TRK_START_TIMER_RANGE              18 : 12
#define XUSB_PADCTL_USB2_BIAS_PAD_CTL_1_0_TRK_DONE_RESET_TIMER_RANGE         25 : 19

#define XUSB_PADCTL_HSIC_PAD_TRK_CTL_0_0                                     _MK_ADDR_CONST(0x340)
#define XUSB_PADCTL_HSIC_PAD_TRK_CTL_0_0_TRK_START_TIMER_RANGE               11 : 5
#define XUSB_PADCTL_HSIC_PAD_TRK_CTL_0_0_TRK_DONE_RESET_TIMER_RANGE          18 : 12
#define XUSB_PADCTL_HSIC_PAD_TRK_CTL_0_0_PD_TRK_RANGE                        19 : 19
#define XUSB_PADCTL_HSIC_PAD_TRK_CTL_0_0_PD_TRK_SW_DEFAULT                   _MK_MASK_CONST(0x0)

#define XUSB_PADCTL_HSIC_PAD1_CTL_0_0                                        _MK_ADDR_CONST(0x320)
#define XUSB_PADCTL_HSIC_PAD1_CTL_0_0_PD_TX_DATA0_RANGE                      1 : 1
#define XUSB_PADCTL_HSIC_PAD1_CTL_0_0_PD_TX_DATA0_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define XUSB_PADCTL_HSIC_PAD1_CTL_0_0_PD_TX_DATA1_RANGE                      2 : 2
#define XUSB_PADCTL_HSIC_PAD1_CTL_0_0_PD_TX_DATA1_SW_DEFAULT                 _MK_MASK_CONST(0x0)
#define XUSB_PADCTL_HSIC_PAD1_CTL_0_0_PD_TX_STROBE_RANGE                     3 : 3
#define XUSB_PADCTL_HSIC_PAD1_CTL_0_0_PD_TX_STROBE_SW_DEFAULT                _MK_MASK_CONST(0x0)

#define XUSB_PADCTL_USB2_VBUS_ID_0                                           _MK_ADDR_CONST(0xc60)
#define XUSB_PADCTL_USB2_VBUS_ID_0_VBUS_VALID_ST_CHNG_RANGE                  4 : 4
#define XUSB_PADCTL_USB2_VBUS_ID_0_IDDIG_ST_CHNG_RANGE                       10 : 10
#define XUSB_PADCTL_USB2_VBUS_ID_0_ID_SOURCE_SELECT_RANGE                    17 : 16
#define XUSB_PADCTL_USB2_VBUS_ID_0_ID_SOURCE_SELECT_ID_OVERRIDE              _MK_ENUM_CONST(1)
#define XUSB_PADCTL_USB2_VBUS_ID_0_VBUS_SOURCE_SELECT_RANGE                  13 : 12
#define XUSB_PADCTL_USB2_VBUS_ID_0_VBUS_SOURCE_SELECT_VBUS_OVERRIDE          _MK_ENUM_CONST(1)
#define XUSB_PADCTL_USB2_VBUS_ID_0_ID_OVERRIDE_RANGE                         21 : 18

/* Define for Boot Port enum */
enum {
  USB_BOOT_PORT_OTG0 = 0,
  USB_BOOT_PORT_OTG1 = 1,
  USB_BOOT_PORT_OTG2 = 2,
  USB_BOOT_PORT_OTG3 = 3
};

enum {
  VBUS_ENABLE_0 = 0x1,
  VBUS_ENABLE_1 = 0x2
};

typedef struct {
  NVIDIA_USBPADCTL_PROTOCOL  UsbPadCtlProtocol;
  EFI_HANDLE                 ImageHandle;
} USBPADCTL_DXE_PRIVATE;

#endif /* USB_PADCTL_PRIVATE_H_ */
