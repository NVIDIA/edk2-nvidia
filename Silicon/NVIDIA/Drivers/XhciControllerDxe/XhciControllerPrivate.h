/** @file

  XHCI Controller Driver private structures

  SPDX-FileCopyrightText: Copyright (c) 2019-2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef XHCI_CONTROLLER_PRIVATE_H_
#define XHCI_CONTROLLER_PRIVATE_H_

#include <PiDxe.h>

#define NV_FIELD_MASK(x)   (0xFFFFFFFFUL>>(31-((1 ? x)%32)+((0 ? x)%32)))
#define NV_FIELD_SHIFT(x)  ((0 ? x)%32)

#ifndef _MK_ENUM_CONST
#define _MK_ENUM_CONST(_constant_)  (_constant_ ## UL)
#endif

#ifndef _MK_ADDR_CONST
#define _MK_ADDR_CONST(_constant_)  _constant_
#endif

#define NV_FIELD_SHIFTMASK(x) \
        (NV_FIELD_MASK(x) << (NV_FIELD_SHIFT(x)))

#define NV_DRF_DEF(d, r, f, c) \
        ((d##_##r##_0_##f##_##c) << NV_FIELD_SHIFT(d##_##r##_0_##f##_RANGE))

#define NV_FLD_SET_DRF_DEF(d, r, f, c, v) \
        (((v) & ~NV_FIELD_SHIFTMASK(d##_##r##_0_##f##_RANGE)) | \
        NV_DRF_DEF(d, r, f, c))

#define XUSB_CFG_1_0                        _MK_ADDR_CONST(0x00000004)
#define XUSB_CFG_1_0_MEMORY_SPACE_RANGE     1 : 1
#define XUSB_CFG_1_0_MEMORY_SPACE_DISABLED  _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_MEMORY_SPACE_ENABLED   _MK_ENUM_CONST(0x00000001)
#define XUSB_CFG_1_0_BUS_MASTER_RANGE       2 : 2
#define XUSB_CFG_1_0_BUS_MASTER_DISABLED    _MK_ENUM_CONST(0x00000000)
#define XUSB_CFG_1_0_BUS_MASTER_ENABLED     _MK_ENUM_CONST(0x00000001)

#define XUSB_CFG_4_0  _MK_ADDR_CONST(0x00000010)

#define XUSB_CFG_7_0  _MK_ADDR_CONST(0x0000001C)

#define XUSB_CFG_AXI_CFG_0  _MK_ADDR_CONST(0x000000F8)

#define XUSB_OP_USBSTS  _MK_ADDR_CONST(0x00000004)
#define USBSTS_HCH      BIT0
#define USBSTS_CNR      BIT11
#define USBSTS_HCE      BIT12

#define XUSB_CAP_HCSPARAMS1        _MK_ADDR_CONST(0x00000004)
#define HCSPARAMS1_MAXPORTS_SHIFT  24

#define XUSB_OP_PORTSC_BASE    _MK_ADDR_CONST(0x00000400)
#define XUSB_OP_PORTSC_STRIDE  _MK_ADDR_CONST(0x00000010)

/* PORTSC.PED is RW1CS: write one to disable. */
#define PORTSC_CCS    BIT0
#define PORTSC_PED    BIT1
#define PORTSC_OCA    BIT3
#define PORTSC_PLS    (BIT5 | BIT6 | BIT7 | BIT8)
#define PORTSC_PP     BIT9
#define PORTSC_SPEED  (BIT10 | BIT11 | BIT12 | BIT13)
#define PORTSC_PIC    (BIT14 | BIT15)
#define PORTSC_WCE    BIT25
#define PORTSC_WDE    BIT26
#define PORTSC_WOE    BIT27
#define PORTSC_DR     BIT30

/* Preserve PORTSC RO and RWS fields during read-modify-write. */
#define PORTSC_WAKE      (PORTSC_WCE | PORTSC_WDE | PORTSC_WOE)
#define PORTSC_RO        (PORTSC_CCS | PORTSC_OCA | PORTSC_SPEED | PORTSC_DR)
#define PORTSC_RWS       (PORTSC_PLS | PORTSC_PP | PORTSC_PIC | PORTSC_WAKE)
#define PORTSC_PRESERVE  (PORTSC_RO | PORTSC_RWS)

#define XUSB_HALT_TIMEOUT_US          16000
#define XUSB_RUN_TIMEOUT_US           16000
#define XUSB_PORT_DISABLE_TIMEOUT_US  10000
#define XUSB_POLL_STEP_US             50

#define XUSB_OP_USBCMD_RS  BIT0

#define XUSB_BASE_ADDR_SHIFT  15
#define XUSB_BASE_ADDR_MASK   0x1ffff

#define XUSB_T234_BASE2_ADDR_SHIFT  16
#define XUSB_T234_BASE2_ADDR_MASK   0xffff

/* Stores Platform Specific Information */
typedef struct {
  UINT32                  Cfg4AddrShift;
  UINT32                  Cfg4AddrMask;
  UINT32                  Cfg7AddrShift;
  UINT32                  Cfg7AddrMask;
  EFI_PHYSICAL_ADDRESS    BaseAddress;
  EFI_PHYSICAL_ADDRESS    CfgAddress;
  EFI_PHYSICAL_ADDRESS    Base2Address;
} TEGRA_XUSB_SOC;

TEGRA_XUSB_SOC  Tegra234Soc = {
  .Cfg4AddrShift = XUSB_BASE_ADDR_SHIFT,
  .Cfg4AddrMask  = XUSB_BASE_ADDR_MASK,
  .Cfg7AddrShift = XUSB_T234_BASE2_ADDR_SHIFT,
  .Cfg7AddrMask  = XUSB_T234_BASE2_ADDR_MASK,
};

#define XHCICONTROLLER_SIGNATURE  SIGNATURE_32('X','H','C','I')
typedef struct {
  UINT32                            Signature;
  NVIDIA_XHCICONTROLLER_PROTOCOL    XhciControllerProtocol;
  TEGRA_XUSB_SOC                    *XusbSoc;
  EFI_HANDLE                        ImageHandle;
  NVIDIA_USBPADCTL_PROTOCOL         *mUsbPadCtlProtocol;
  NVIDIA_USBFW_PROTOCOL             *mUsbFwProtocol;
  EFI_EVENT                         ExitBootServicesEvent;
  EFI_HANDLE                        ControllerHandle;
} XHCICONTROLLER_DXE_PRIVATE;
#define XHCICONTROLLER_PRIVATE_DATA_FROM_THIS(a)  CR(a, XHCICONTROLLER_DXE_PRIVATE, XhciControllerProtocol, XHCICONTROLLER_SIGNATURE)

#endif /* XHCI_CONTROLLER_PRIVATE_H_ */
