/** @file

  USB Pad Control Driver private structures

  Copyright (c) 2019-2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef USB_PADCTL_PRIVATE_H_
#define USB_PADCTL_PRIVATE_H_

#include <PiDxe.h>
#include <Protocol/UsbPadCtl.h>
#include <Protocol/ArmScmiClock2Protocol.h>

#define REG_VDD_USB0_5V  0xB3
#define REG_VDD_USB1_5V  0xB4

#ifndef _MK_ADDR_CONST
#define _MK_ADDR_CONST(_constant_)  _constant_
#endif

#ifndef _MK_ENUM_CONST
#define _MK_ENUM_CONST(_constant_)  (_constant_ ## UL)
#endif

#ifndef _MK_MASK_CONST
#define _MK_MASK_CONST(_constant_)  _constant_
#endif

#ifndef _MK_SHIFT_CONST
#define _MK_SHIFT_CONST(_constant_)  _constant_
#endif

#define NV_FIELD_LOWBIT(x)     (0 ? x)
#define NV_FIELD_HIGHBIT(x)    (1 ? x)
#define NV_FIELD_SIZE(x)       (NV_FIELD_HIGHBIT(x)-NV_FIELD_LOWBIT(x)+1)
#define NV_FIELD_SHIFT(x)      ((0 ? x)%32)
#define NV_FIELD_MASK(x)       (0xFFFFFFFFUL>>(31-((1 ? x)%32)+((0 ? x)%32)))
#define NV_FIELD_BITS(val, x)  (((val) & NV_FIELD_MASK(x)) << NV_FIELD_SHIFT(x))
#define NV_FIELD_SHIFTMASK(x)  (NV_FIELD_MASK(x) << (NV_FIELD_SHIFT(x)))

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

typedef struct {
  BOOLEAN    PortEnabled;
  BOOLEAN    OcEnabled;
  UINT32     PortNum;
  UINT32     CompanionPort; /* Stores the USB2 Companion Port for USB3 Port */
  UINT32     OcPin;
  UINT32     VbusSupply; /* Regulator ID read from Port's Property in DT */
  UINT32     FuseHsCurrLevel;
} PORT_INFO;

/* Stores Platform Specific Information */
typedef struct {
  UINT32       NumHsPhys;
  UINT32       NumSsPhys;
  UINT32       NumOcPins;
  PORT_INFO    *Usb2Ports;
  PORT_INFO    *Usb3Ports;
  UINT32       FuseHsSquelchLevel;
  UINT32       FuseHsTermRangeAdj;
  UINT32       FuseRpdCtrl;
  UINT32       *Usb2ClockIds;
  UINT32       NumUsb2Clocks;
} PADCTL_PLAT_CONFIG;

#define PADCTL_SIGNATURE  SIGNATURE_32('P','D','C','L')
typedef struct {
  UINT32                                    Signature;
  NVIDIA_USBPADCTL_PROTOCOL                 mUsbPadCtlProtocol;
  PADCTL_PLAT_CONFIG                        PlatConfig; /* Platform specific Config information */
  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL    *DeviceTreeNode;
  EFI_HANDLE                                ImageHandle;
  NVIDIA_REGULATOR_PROTOCOL                 *mRegulator;
  NVIDIA_EFUSE_PROTOCOL                     *mEfuse;
  NVIDIA_PINMUX_PROTOCOL                    *mPmux;
  SCMI_CLOCK2_PROTOCOL                      *mClockProtocol;
  EFI_EVENT                                 TimerEvent; /* Used for Over Current Handling */
  EFI_PHYSICAL_ADDRESS                      BaseAddress;
  BOOLEAN                                   HandleOverCurrent;
} USBPADCTL_DXE_PRIVATE;
#define PADCTL_PRIVATE_DATA_FROM_THIS(a)      CR(a, USBPADCTL_DXE_PRIVATE, mUsbPadCtlProtocol, PADCTL_SIGNATURE)
#define PADCTL_PRIVATE_DATA_FROM_PROTOCOL(a)  PADCTL_PRIVATE_DATA_FROM_THIS(a)

EFI_STATUS
InitUsbHw194 (
  IN  NVIDIA_USBPADCTL_PROTOCOL  *This
  );

VOID
DeInitUsbHw194 (
  IN  NVIDIA_USBPADCTL_PROTOCOL  *This
  );

extern PADCTL_PLAT_CONFIG  Tegra194UsbConfig;

EFI_STATUS
InitUsbHw234 (
  IN  NVIDIA_USBPADCTL_PROTOCOL  *This
  );

VOID
DeInitUsbHw234 (
  IN  NVIDIA_USBPADCTL_PROTOCOL  *This
  );

extern PADCTL_PLAT_CONFIG  Tegra234UsbConfig;

#endif /* USB_PADCTL_PRIVATE_H_ */
