/** @file

  USB Pad Control Driver Platform Specific Definitions/Functions

  Copyright (c) 2019-2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Protocol/Regulator.h>
#include <Protocol/EFuse.h>
#include <Protocol/PinMux.h>
#include <libfdt.h>
#include "UsbPadCtlPrivate.h"

#define USB2_OTG_PADX_CTL_0(i)  (0x88 + (i) * 0x40)
#define USB2_OTG_PADX_CTL_1(i)  (0x8c + (i) * 0x40)
#define USB2_OTG_PD_ZI  (1 << 29)
#define TERM_SEL        (1 << 25)
#define HS_CURR_LEVEL(x)   ((x) & 0x3f)
#define LS_FSLEW(x)        (((x) & 0xf) << 21)
#define LS_RSLEW(x)        (((x) & 0xf) << 17)
#define TERM_RANGE_ADJ(x)  (((x) & 0xf) << 3)
#define RPD_CTRL(x)        (((x) & 0x1f) << 26)
#define USB2_OTG_PD     (1 << 26)
#define USB2_OTG_PD_DR  (1 << 2)

#define USB2_BATTERY_CHRG_OTGPADX_CTL1(x)  (0x84 + (x) * 0x40)
#define VREG_LEVEL_500MA  0x0
#define VREG_LEVEL_900MA  0x1
#define VREG_LEVEL_2A     0x3
#define VREG_LEV(x)  (((x) & 0x3) << 7)
#define VREG_DIR(x)  (((x) & 0x3) << 11)
#define VREG_DIR_IN   VREG_DIR(1)
#define VREG_DIR_OUT  VREG_DIR(2)
#define PD_VREG       (1 << 6)

#define FUSE_USB_CALIB_0                   _MK_ADDR_CONST(0x1f0)
#define FUSE_USB_CALIB_TERMRANGEADJ_MASK   0x780
#define FUSE_USB_CALIB_TERMRANGEADJ_SHIFT  7
#define FUSE_USB_CALIB_SQUELCHLEVEL_MASK   0xE0000000
#define FUSE_USB_CALIB_SQUELCHLEVEL_SHIFT  29
#define HS_CURR_LEVEL_PADX_SHIFT(x)  ((x) ? (11 + (x - 1) * 6) : 0)
#define HS_CURR_LEVEL_PAD_MASK  (0x3f)

#define FUSE_USB_CALIB_EXT_0              _MK_ADDR_CONST(0x350)
#define FUSE_USB_CALIB_EXT_RPD_CTRL_MASK  0x1F

#define XUSB_PADCTL_USB2_PAD_MUX_0  _MK_ADDR_CONST(0x4)
#define USB2_PAD_MUX_PORT_SHIFT(x)  ((x) * 2)
#define USB2_PAD_MUX_PORT_MASK  (0x3)
#define PAD_MUX_PORT_XUSB       (1)

#define XUSB_PADCTL_USB2_PORT_CAP_0  _MK_ADDR_CONST(0x8)
#define USB2_PORTX_CAP_SHIFT(x)  ((x) * 4)
#define USB2_PORT_CAP_MASK  (0x3)
#define PORT_CAP_HOST       _MK_ENUM_CONST(1)

#define XUSB_PADCTL_SS_PORT_CAP_0  _MK_ADDR_CONST(0xc)
#define SS_PORTX_CAP_SHIFT(x)  ((x) * 4)
#define SS_PORT_CAP_MASK  (0x3)

#define XUSB_PADCTL_USB2_OC_MAP_0  _MK_ADDR_CONST(0x10)
#define PORTX_OC_PIN_SHIFT(x)  ((x) * 4)
#define PORT_OC_PIN_MASK           (0xf)
#define OC_PIN_DETECTION_DISABLED  (0xf)
#define OC_PIN_DETECTED(x)           (x)
#define OC_PIN_DETECTED_VBUS_PAD(x)  ((x) + 4)

#define XUSB_PADCTL_SS_OC_MAP_0  _MK_ADDR_CONST(0x14)

#define XUSB_PADCTL_VBUS_OC_MAP_0  _MK_ADDR_CONST(0x18)
#define VBUS_OC_MAP_SHIFT(x)  ((x) * 5 + 1)
#define VBUS_OC_MAP_MASK            (0xf)
#define VBUS_OC_DETECTION_DISABLED  (0xf)
#define VBUS_OC_DETECTED(x)           (x)
#define VBUS_OC_DETECTED_VBUS_PAD(x)  ((x) + 4)
#define VBUS_ENABLE(x)                (1 << (x) * 5)

#define XUSB_PADCTL_OC_DET_0  _MK_ADDR_CONST(0x1c)
#define OC_DETECTED_VBUS_PAD(x)  (1 << (12 + (x)))
#define OC_DETECTED_VBUS_PAD_MASK  (0xf << 12)
#define OC_DETECTED_INT_EN_VBUS_PAD(x)  (1 << (24 + (x)))

#define XUSB_PADCTL_ELPG_PROGRAM_1_0  _MK_ADDR_CONST(0x24)
#define SSPX_ELPG_CLAMP_EN(x)        (1 << (0 + (x) * 3))
#define SSPX_ELPG_CLAMP_EN_EARLY(x)  (1 << (1 + (x) * 3))
#define SSPX_ELPG_VCORE_DOWN(x)      (1 << (2 + (x) * 3))

#define XUSB_PADCTL_USB2_BIAS_PAD_CTL0  (0x284)
#define BIAS_PAD_PD                     (1 << 11)
#define HS_SQUELCH_LEVEL(x)  (((x) & 0x7) << 0)
#define HS_DISCON_LEVEL(x)   (((x) & 0x7) << 3)

#define XUSB_PADCTL_USB2_BIAS_PAD_CTL1  (0x288)
#define USB2_TRK_START_TIMER(x)       (((x) & 0x7f) << 12)
#define USB2_TRK_DONE_RESET_TIMER(x)  (((x) & 0x7f) << 19)
#define USB2_PD_TRK  (1 << 26)

#define XUSB_PADCTL_USB2_VBUS_ID  (0x360)
#define VBUS_OVERRIDE             (1 << 14)
#define ID_OVERRIDE(x)  (((x) & 0xf) << 18)
#define ID_OVERRIDE_FLOATING  ID_OVERRIDE(8)
#define ID_OVERRIDE_GROUNDED  ID_OVERRIDE(0)
#define VBUS_SOURCE_SELECT(x)  (((x) & 0x3) << 12)
#define ID_SOURCE_SELECT(x)    (((x) & 0x3) << 16)
#define SOURCE_VBUS_OVERRIDE  0x1
#define SOURCE_ID_OVERRIDE    0x1
#define IDDIG_STATUS_CHANGE   (1 << 10)
#define VBUS_VALID_ST_CHANGE  (1 << 4)

/* This Register is in PinMux Space which is used to configure VBUS_EN
 * Pins as either GPIO/SFIO for Over Current Handling
 */
#define PADCTL_UART_USB_VBUS_EN(i)  (0xd0d0 + (i) * 0x8)
#define UART_USB_E_IO_HV_ENABLE    (0x1 << 5)
#define UART_USB_E_INPUT_ENABLE    (0x1 << 6)
#define UART_USB_PM_MASK           (0x3)
#define UART_USB_PM_USB            0
#define UART_USB_PM_RSVD1          1
#define UART_USB_VBUS_EN_TRISTATE  (0x1 << 4)
#define UART_USB_SF_SEL_HSIO       (0x1 << 10)

/* Number of USB Pads on the Platform */
#define TEGRA234_USB3_PHYS   (4)
#define TEGRA234_UTMI_PHYS   (4)
#define TEGRA234_OC_PIN_NUM  (2)

#define ENABLE_FUSE  (0)

PADCTL_PLAT_CONFIG  Tegra234UsbConfig = {
  .NumHsPhys = TEGRA234_UTMI_PHYS,
  .NumSsPhys = TEGRA234_USB3_PHYS,
  .NumOcPins = TEGRA234_OC_PIN_NUM,
};

STATIC
UINT32
PadCtlRead (
  IN USBPADCTL_DXE_PRIVATE  *Private,
  IN UINT32                 Offset
  )
{
  return MmioRead32 (Private->BaseAddress + Offset);
}

STATIC
VOID
PadCtlWrite (
  IN USBPADCTL_DXE_PRIVATE  *Private,
  IN UINT32                 Offset,
  IN UINT32                 RegVal
  )
{
  MmioWrite32 (Private->BaseAddress + Offset, RegVal);
}

STATIC
VOID
InitUsb2PadX (
  IN USBPADCTL_DXE_PRIVATE  *Private
  )
{
  UINT32              RegData, RpdCtrl, i, TermRangeAdj;
  PADCTL_PLAT_CONFIG  *PlatConfig = &(Private->PlatConfig);
  PORT_INFO           *Usb2Ports  = PlatConfig->Usb2Ports;

  TermRangeAdj = PlatConfig->FuseHsTermRangeAdj;
  RpdCtrl      = PlatConfig->FuseRpdCtrl;

  for (i = 0; i < PlatConfig->NumHsPhys; i++) {
    /* Enable PADS only for Ports that are enabled in DT */
    if (Usb2Ports[i].PortEnabled == FALSE) {
      continue;
    }

    /* Clear Each PAD's PD and PD_DR Bits */
    RegData  = PadCtlRead (Private, USB2_OTG_PADX_CTL_0 (i));
    RegData &= ~USB2_OTG_PD;
    PadCtlWrite (Private, USB2_OTG_PADX_CTL_0 (i), RegData);

    RegData  = PadCtlRead (Private, USB2_OTG_PADX_CTL_1 (i));
    RegData &=  ~USB2_OTG_PD_DR;
    PadCtlWrite (Private, USB2_OTG_PADX_CTL_1 (i), RegData);

    /* Assign Each PADS to USB instead of UART */
    RegData  = PadCtlRead (Private, XUSB_PADCTL_USB2_PAD_MUX_0);
    RegData &= ~(USB2_PAD_MUX_PORT_MASK << USB2_PAD_MUX_PORT_SHIFT (i));
    RegData |= (PAD_MUX_PORT_XUSB << USB2_PAD_MUX_PORT_SHIFT (i));
    PadCtlWrite (Private, XUSB_PADCTL_USB2_PAD_MUX_0, RegData);

    /* Assign port capabilities */
    RegData  = PadCtlRead (Private, XUSB_PADCTL_USB2_PORT_CAP_0);
    RegData &= ~(USB2_PORT_CAP_MASK << USB2_PORTX_CAP_SHIFT (i));
    RegData |= (PORT_CAP_HOST << USB2_PORTX_CAP_SHIFT (i));
    PadCtlWrite (Private, XUSB_PADCTL_USB2_PORT_CAP_0, RegData);

    /* Program PD_ZI, TERM_SEL, HsCurrLevel, RpdCtrl and term_range
    *  parameters read from FUSE for all PAD's
    */
    RegData  = PadCtlRead (Private, USB2_OTG_PADX_CTL_0 (i));
    RegData &= ~USB2_OTG_PD_ZI;
    RegData |= TERM_SEL;

    if (ENABLE_FUSE) {
      RegData &= ~HS_CURR_LEVEL(~0);
      RegData |= HS_CURR_LEVEL (Usb2Ports[i].FuseHsCurrLevel);
      RegData &= ~LS_FSLEW(~0);
      RegData |= LS_FSLEW (6);
      RegData &= ~LS_RSLEW(~0);
      RegData |= LS_RSLEW (6);
      PadCtlWrite (Private, USB2_OTG_PADX_CTL_0 (i), RegData);

      RegData  = PadCtlRead (Private, USB2_OTG_PADX_CTL_1 (i));
      RegData &= ~TERM_RANGE_ADJ(~0);
      RegData |= TERM_RANGE_ADJ (PlatConfig->FuseHsTermRangeAdj);
      RegData &= ~RPD_CTRL(~0);
      RegData |= RPD_CTRL (PlatConfig->FuseRpdCtrl);
      PadCtlWrite (Private, USB2_OTG_PADX_CTL_1 (i), RegData);
    } else {
      RegData &= ~LS_FSLEW(~0);
      RegData |= LS_FSLEW (6);
      RegData &= ~LS_RSLEW(~0);
      RegData |= LS_RSLEW (6);

      PadCtlWrite (Private, USB2_OTG_PADX_CTL_0 (i), RegData);
    }

    /* USB Pad protection circuit activation for Enabled PADS. Programming
    * Voltage Direction = HOST and Protection level set to 2A.
    */
    RegData  = PadCtlRead (Private, USB2_BATTERY_CHRG_OTGPADX_CTL1 (i));
    RegData &= ~PD_VREG;
    RegData &= ~VREG_DIR(~0);
    RegData |= VREG_DIR_OUT;
    RegData &= ~VREG_LEV(~0);
    RegData |= VREG_LEV (VREG_LEVEL_2A);
    PadCtlWrite (Private, USB2_BATTERY_CHRG_OTGPADX_CTL1 (i), RegData);
  }
}

STATIC
VOID
InitUsb3PadX (
  USBPADCTL_DXE_PRIVATE  *Private
  )
{
  UINT32              RegData, i, Pin;
  PADCTL_PLAT_CONFIG  *PlatConfig = &(Private->PlatConfig);
  PORT_INFO           *Usb3Ports  = PlatConfig->Usb3Ports;

  for (i = 0; i < PlatConfig->NumSsPhys; i++) {
    if (Usb3Ports[i].PortEnabled == FALSE) {
      continue;
    }

    /* Configure the Port to be in Host Mode */
    RegData  = PadCtlRead (Private, XUSB_PADCTL_SS_PORT_CAP_0);
    RegData &= ~(SS_PORT_CAP_MASK << SS_PORTX_CAP_SHIFT (i));
    RegData |= (PORT_CAP_HOST << SS_PORTX_CAP_SHIFT (i));
    PadCtlWrite (Private, XUSB_PADCTL_SS_PORT_CAP_0, RegData);

    /* Setting SS OC Map */
    if (Usb3Ports[i].OcEnabled == TRUE) {
      Pin      = Usb3Ports[i].OcPin;
      RegData  = PadCtlRead (Private, XUSB_PADCTL_SS_OC_MAP_0);
      RegData &=  ~(PORT_OC_PIN_MASK << PORTX_OC_PIN_SHIFT (i));
      RegData |= (OC_PIN_DETECTED_VBUS_PAD (Pin) & PORT_OC_PIN_MASK) <<
                 PORTX_OC_PIN_SHIFT (i);
      PadCtlWrite (Private, XUSB_PADCTL_SS_OC_MAP_0, RegData);
    }

    /* Release XUSB SS Wake Logic Latching */
    RegData  = PadCtlRead (Private, XUSB_PADCTL_ELPG_PROGRAM_1_0);
    RegData &= ~SSPX_ELPG_CLAMP_EN(i);
    RegData &= ~SSPX_ELPG_CLAMP_EN_EARLY(i);
    RegData &= ~SSPX_ELPG_VCORE_DOWN(i);
    PadCtlWrite (Private, XUSB_PADCTL_ELPG_PROGRAM_1_0, RegData);
  }
}

STATIC
VOID
InitBiasPad (
  USBPADCTL_DXE_PRIVATE  *Private
  )
{
  UINT32              RegVal;
  PADCTL_PLAT_CONFIG  *PlatConfig = &(Private->PlatConfig);
  UINT32              Index;
  EFI_STATUS          Status;

  /* Enable USB2 related Clocks like Usb2 Tracking Clock etc */
  for (Index = 0; Index < PlatConfig->NumUsb2Clocks; Index++) {
    Status = Private->mClockProtocol->Enable (Private->mClockProtocol, PlatConfig->Usb2ClockIds[Index], TRUE);
    if (EFI_ERROR (Status)) {
      /* Print Error and Conitnue as USB3(Super Speed) might still be partially working */
      DEBUG ((
        EFI_D_ERROR,
        "Unable to Enable USB2 Clock:%d Status: %x\n",
        PlatConfig->Usb2ClockIds[Index],
        Status
        ));
      continue;
    }
  }

  /* Program hs_squelch_level and power up the BIAS Pad */
  RegVal  = PadCtlRead (Private, XUSB_PADCTL_USB2_BIAS_PAD_CTL0);
  RegVal &= ~BIAS_PAD_PD;
  RegVal &= ~HS_SQUELCH_LEVEL(~0);
  RegVal |= HS_SQUELCH_LEVEL (PlatConfig->FuseHsSquelchLevel);
  RegVal &= ~HS_DISCON_LEVEL(~0);
  RegVal |= HS_DISCON_LEVEL (0x7);
  PadCtlWrite (Private, XUSB_PADCTL_USB2_BIAS_PAD_CTL0, RegVal);

  /* Start Bias PAD Tracking. usb2_trk clock should arleady be enabled
   * by now when this driver is loaded by DeviceDiscovery Library */
  RegVal  = PadCtlRead (Private, XUSB_PADCTL_USB2_BIAS_PAD_CTL1);
  RegVal &= ~USB2_TRK_START_TIMER(~0);
  RegVal |= USB2_TRK_START_TIMER (0x1E);
  RegVal &= ~USB2_TRK_DONE_RESET_TIMER(~0);
  RegVal |= USB2_TRK_DONE_RESET_TIMER (0xA);
  PadCtlWrite (Private, XUSB_PADCTL_USB2_BIAS_PAD_CTL1, RegVal);

  gBS->Stall (1);

  RegVal  = PadCtlRead (Private, XUSB_PADCTL_USB2_BIAS_PAD_CTL1);
  RegVal &= ~USB2_PD_TRK;
  PadCtlWrite (Private, XUSB_PADCTL_USB2_BIAS_PAD_CTL1, RegVal);
}

STATIC
VOID
VbusIdOverride (
  USBPADCTL_DXE_PRIVATE  *Private
  )
{
  UINT32  RegVal;

  /* Local override for VBUS and ID status reporting. */
  RegVal  = PadCtlRead (Private, XUSB_PADCTL_USB2_VBUS_ID);
  RegVal &= ~ID_SOURCE_SELECT(~0);
  RegVal |= ID_SOURCE_SELECT (SOURCE_ID_OVERRIDE);
  RegVal &= ~VBUS_SOURCE_SELECT(~0);
  RegVal |= VBUS_SOURCE_SELECT (SOURCE_VBUS_OVERRIDE);
  RegVal &= ~ID_OVERRIDE(~0);
  RegVal |= ID_OVERRIDE_GROUNDED;
  PadCtlWrite (Private, XUSB_PADCTL_USB2_VBUS_ID, RegVal);

  /* Clear false reporting of VBUS and ID status changes. */
  RegVal  =  PadCtlRead (Private, XUSB_PADCTL_USB2_VBUS_ID);
  RegVal |= IDDIG_STATUS_CHANGE;
  RegVal |= VBUS_VALID_ST_CHANGE;
  PadCtlWrite (Private, XUSB_PADCTL_USB2_VBUS_ID, RegVal);
}

/* This function Enables VBUS and OC Detection on a given port
 * This function will be called during init when enabling VBUS
 * on all ports or from a Over Current Handler to enable Vbus
 * again after Vbus is powered off on individual port due to OC
 * detection
 */
STATIC
VOID
EnablePortVbusOc (
  IN UINT32              PortIndex,
  USBPADCTL_DXE_PRIVATE  *Private
  )
{
  PADCTL_PLAT_CONFIG  *PlatConfig = &(Private->PlatConfig);
  UINT32              RegData, Pin;

  Pin = PlatConfig->Usb2Ports[PortIndex].OcPin;

  /* Need to Disable OC_DETECTION before enabling VBUS */
  RegData  = PadCtlRead (Private, XUSB_PADCTL_USB2_OC_MAP_0);
  RegData &= ~(PORT_OC_PIN_MASK << PORTX_OC_PIN_SHIFT (PortIndex));
  RegData |= OC_PIN_DETECTION_DISABLED << PORTX_OC_PIN_SHIFT (PortIndex);
  PadCtlWrite (Private, XUSB_PADCTL_USB2_OC_MAP_0, RegData);

  RegData  = PadCtlRead (Private, XUSB_PADCTL_VBUS_OC_MAP_0);
  RegData &= ~(VBUS_OC_MAP_MASK << VBUS_OC_MAP_SHIFT (Pin));
  RegData |= VBUS_OC_DETECTION_DISABLED << VBUS_OC_MAP_SHIFT (Pin);
  PadCtlWrite (Private, XUSB_PADCTL_VBUS_OC_MAP_0, RegData);

  /* clear false OC_DETECTED VBUS_PADx */
  RegData  = PadCtlRead (Private, XUSB_PADCTL_OC_DET_0);
  RegData &= ~OC_DETECTED_VBUS_PAD_MASK;
  RegData |= OC_DETECTED_VBUS_PAD (Pin);
  PadCtlWrite (Private, XUSB_PADCTL_OC_DET_0, RegData);

  gBS->Stall (100);

  /* Enable VBUS */
  RegData  = PadCtlRead (Private, XUSB_PADCTL_VBUS_OC_MAP_0);
  RegData |= VBUS_ENABLE (Pin);
  PadCtlWrite (Private, XUSB_PADCTL_VBUS_OC_MAP_0, RegData);

  /* vbus has been supplied to device. A finite time (>10ms) for OC
   * detection pin to be pulled-up
   */
  gBS->Stall (2000);

  /* check and clear if there is any stray OC */
  RegData = PadCtlRead (Private, XUSB_PADCTL_OC_DET_0);
  if (RegData & OC_DETECTED_VBUS_PAD (Pin)) {
    RegData  = PadCtlRead (Private, XUSB_PADCTL_VBUS_OC_MAP_0);
    RegData &= ~VBUS_ENABLE(Pin);
    PadCtlWrite (Private, XUSB_PADCTL_VBUS_OC_MAP_0, RegData);

    RegData  = PadCtlRead (Private, XUSB_PADCTL_OC_DET_0);
    RegData &= ~OC_DETECTED_VBUS_PAD_MASK;
    RegData |= OC_DETECTED_VBUS_PAD (Pin);
    PadCtlWrite (Private, XUSB_PADCTL_OC_DET_0, RegData);

    /* Enable Vbus back after clearing stray OC */
    RegData  = PadCtlRead (Private, XUSB_PADCTL_VBUS_OC_MAP_0);
    RegData |= VBUS_ENABLE (Pin);
    PadCtlWrite (Private, XUSB_PADCTL_VBUS_OC_MAP_0, RegData);
  }

  /* change the OC_MAP source and enable OC interrupt */
  RegData  = PadCtlRead (Private, XUSB_PADCTL_USB2_OC_MAP_0);
  RegData &= ~(PORT_OC_PIN_MASK << PORTX_OC_PIN_SHIFT (PortIndex));
  RegData |= (OC_PIN_DETECTED_VBUS_PAD (Pin) & PORT_OC_PIN_MASK) << PORTX_OC_PIN_SHIFT (PortIndex);
  PadCtlWrite (Private, XUSB_PADCTL_USB2_OC_MAP_0, RegData);

  RegData  = PadCtlRead (Private, XUSB_PADCTL_OC_DET_0);
  RegData &= ~OC_DETECTED_VBUS_PAD_MASK;
  RegData |= OC_DETECTED_INT_EN_VBUS_PAD (Pin);
  PadCtlWrite (Private, XUSB_PADCTL_OC_DET_0, RegData);

  RegData  = PadCtlRead (Private, XUSB_PADCTL_VBUS_OC_MAP_0);
  RegData &= ~(VBUS_OC_MAP_MASK << VBUS_OC_MAP_SHIFT (Pin));
  RegData |= (VBUS_OC_DETECTED_VBUS_PAD (Pin) & VBUS_OC_MAP_MASK) <<
             VBUS_OC_MAP_SHIFT (Pin);
  PadCtlWrite (Private, XUSB_PADCTL_VBUS_OC_MAP_0, RegData);
}

STATIC
VOID
DisablePortVbusOc (
  IN UINT32              PortIndex,
  USBPADCTL_DXE_PRIVATE  *Private
  )
{
  PADCTL_PLAT_CONFIG  *PlatConfig = &(Private->PlatConfig);
  UINT32              RegData, Pin;

  Pin = PlatConfig->Usb2Ports[PortIndex].OcPin;

  /* Disable OC Interrupt */
  RegData  = PadCtlRead (Private, XUSB_PADCTL_OC_DET_0);
  RegData |= OC_DETECTED_VBUS_PAD_MASK;
  RegData &= ~OC_DETECTED_INT_EN_VBUS_PAD(Pin);
  PadCtlWrite (Private, XUSB_PADCTL_OC_DET_0, RegData);

  RegData  = PadCtlRead (Private, XUSB_PADCTL_USB2_OC_MAP_0);
  RegData &= ~(PORT_OC_PIN_MASK << PORTX_OC_PIN_SHIFT (PortIndex));
  RegData |= OC_PIN_DETECTION_DISABLED << PORTX_OC_PIN_SHIFT (PortIndex);
  PadCtlWrite (Private, XUSB_PADCTL_USB2_OC_MAP_0, RegData);

  RegData  = PadCtlRead (Private, XUSB_PADCTL_VBUS_OC_MAP_0);
  RegData &= ~(VBUS_OC_MAP_MASK << VBUS_OC_MAP_SHIFT (Pin));
  RegData |= VBUS_OC_DETECTION_DISABLED << VBUS_OC_MAP_SHIFT (Pin);
  PadCtlWrite (Private, XUSB_PADCTL_VBUS_OC_MAP_0, RegData);

  /* Disable VBUS */
  RegData  = PadCtlRead (Private, XUSB_PADCTL_VBUS_OC_MAP_0);
  RegData &= ~VBUS_ENABLE(Pin);
  PadCtlWrite (Private, XUSB_PADCTL_VBUS_OC_MAP_0, RegData);
}

STATIC
VOID
OverCurrentHandler (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  UINT32  RegData;

  UINT32                 Pin, i;
  USBPADCTL_DXE_PRIVATE  *Private    = (USBPADCTL_DXE_PRIVATE *)Context;
  PADCTL_PLAT_CONFIG     *PlatConfig = &(Private->PlatConfig);

  RegData = PadCtlRead (Private, XUSB_PADCTL_OC_DET_0);
  for (i = 0; i < PlatConfig->NumHsPhys; i++) {
    if (PlatConfig->Usb2Ports[i].OcEnabled == FALSE) {
      continue;
    }

    Pin = PlatConfig->Usb2Ports[i].OcPin;
    if (RegData & OC_DETECTED_VBUS_PAD (Pin)) {
      /* First Clear the Interrupts */
      RegData &= ~OC_DETECTED_VBUS_PAD_MASK;
      RegData |= OC_DETECTED_VBUS_PAD (Pin);
      RegData &= ~OC_DETECTED_INT_EN_VBUS_PAD(Pin);
      PadCtlWrite (Private, XUSB_PADCTL_OC_DET_0, RegData);

      RegData = PadCtlRead (Private, XUSB_PADCTL_OC_DET_0);
      /* Supply VBUS and enable OC Handling again for the Port */
      EnablePortVbusOc (i, Private);
    }
  }
}

/* If Enable, Change the VBUS EN Pin from GPIO to SFIO when Over Current Handling is enabled.
 * This will enable XHCI HW to automatically disable Vbus when Over Current happens.
 *
 * Else Put the VBUS EN Pin in Default State
 */
STATIC
VOID
SelectVbusEnableTriState (
  USBPADCTL_DXE_PRIVATE  *Private,
  UINT32                 Pin,
  BOOLEAN                Enable
  )
{
 #if 0
  UINT32                  RegVal;
  NVIDIA_PINMUX_PROTOCOL  *mPmux = Private->mPmux;
  mPmux->ReadReg (mPmux, PADCTL_UART_USB_VBUS_EN (Pin), &RegVal);

  if (Enable) {
    RegVal &= ~UART_USB_PM_MASK;
    RegVal |= UART_USB_PM_USB | UART_USB_E_IO_HV_ENABLE | UART_USB_E_INPUT_ENABLE
              | UART_USB_VBUS_EN_TRISTATE | UART_USB_SF_SEL_HSIO;
    mPmux->WriteReg (mPmux, PADCTL_UART_USB_VBUS_EN (Pin), RegVal);
    mPmux->ReadReg (mPmux, PADCTL_UART_USB_VBUS_EN (Pin), &RegVal);
  } else {
    /* Put in Default State */
    RegVal &= ~(UART_USB_PM_MASK | UART_USB_E_INPUT_ENABLE | UART_USB_VBUS_EN_TRISTATE| UART_USB_SF_SEL_HSIO);
    RegVal |= UART_USB_PM_RSVD1;
    mPmux->WriteReg (mPmux, PADCTL_UART_USB_VBUS_EN (Pin), RegVal);
  }

 #endif
}

STATIC
VOID
DisableVbus (
  USBPADCTL_DXE_PRIVATE  *Private
  )
{
  UINT32                     i;
  PADCTL_PLAT_CONFIG         *PlatConfig = &(Private->PlatConfig);
  PORT_INFO                  *Usb2Ports  = PlatConfig->Usb2Ports;
  NVIDIA_REGULATOR_PROTOCOL  *mRegulator = Private->mRegulator;

  /* Stop the Over Current Timer Event if Enabled */
  if (Private->HandleOverCurrent == TRUE) {
    gBS->CloseEvent (Private->TimerEvent);
    Private->HandleOverCurrent = FALSE;
  }

  for (i = 0; i < PlatConfig->NumHsPhys; i++) {
    /* Dont do anything for Device Mode and Disabled Ports */
    if (Usb2Ports[i].PortEnabled == FALSE) {
      continue;
    }

    if (Usb2Ports[i].OcEnabled == TRUE) {
      /* Disable VBUS */
      DisablePortVbusOc (i, Private);
      /* Put the Vbus Enable Pin in Default State */
      SelectVbusEnableTriState (Private, Usb2Ports[i].OcPin, FALSE);
    } else {
      /* Disable VBUS Regulator through GPIO */
      if (EFI_ERROR (mRegulator->Enable (mRegulator, Usb2Ports[i].VbusSupply, FALSE))) {
        DEBUG ((
          EFI_D_ERROR,
          "%a: Couldn't Disable Regulator: %d for USB Port: %u\n",
          __FUNCTION__,
          Usb2Ports[i].VbusSupply,
          i
          ));
        /* Printing the Error and continuing here to do maximum clean up */
        continue;
      }
    }
  }

  return;
}

STATIC
EFI_STATUS
EnableVbus (
  USBPADCTL_DXE_PRIVATE  *Private
  )
{
  UINT32                     i;
  PADCTL_PLAT_CONFIG         *PlatConfig = &(Private->PlatConfig);
  PORT_INFO                  *Usb2Ports  = PlatConfig->Usb2Ports;
  NVIDIA_REGULATOR_PROTOCOL  *mRegulator = Private->mRegulator;
  EFI_STATUS                 Status;

  /* Over Current Handling is disabled by default unless enabled in DT*/
  Private->HandleOverCurrent = FALSE;
  for (i = 0; i < PlatConfig->NumHsPhys; i++) {
    /* Dont do anything for Device Mode and Disabled Ports */
    if (Usb2Ports[i].PortEnabled == FALSE) {
      continue;
    }

    if (Usb2Ports[i].OcEnabled == TRUE) {
      SelectVbusEnableTriState (Private, Usb2Ports[i].OcPin, TRUE);
      EnablePortVbusOc (i, Private);
      Private->HandleOverCurrent = TRUE;
    } else {
      /* Enable VBUS Regulator through GPIO */
      if (EFI_ERROR (mRegulator->Enable (mRegulator, Usb2Ports[i].VbusSupply, TRUE))) {
        DEBUG ((
          EFI_D_ERROR,
          "Couldn't Enable Regulator: %d for USB Port: %u\n",
          Usb2Ports[i].VbusSupply,
          i
          ));

        /* Printing the Error and continuing here so that other Ports will
         * still keep working and USB is not disabled completely
         */
        Usb2Ports[i].PortEnabled = FALSE;
        continue;
      }
    }
  }

  /* If atleast one port has OC Enabled, then create Timer Handler that
   * periodically checks for any OC Conditions and handle accordingly
   */
  if (Private->HandleOverCurrent == TRUE) {
    Status = gBS->CreateEvent (EVT_TIMER | EVT_NOTIFY_SIGNAL, TPL_NOTIFY, OverCurrentHandler, Private, &Private->TimerEvent);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a: Unable to create OverCurrent Timer\n", __FUNCTION__));
      return Status;
    }

    /* Using 2 Seconds so that we dont load the System with frequent Polling */
    Status = gBS->SetTimer (Private->TimerEvent, TimerPeriodic, 20000000);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "Error in Setting OverCurrent Timer\n"));
      return Status;
    }
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
FindUsb2PadClocks (
  PADCTL_PLAT_CONFIG                      *PlatConfig,
  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode
  )
{
  INT32         NodeOffset = -1;
  CONST UINT32  *ClockIds = NULL;
  INT32         ClocksLength, Index;
  UINT32        BpmpPhandle;

  NodeOffset = fdt_subnode_offset (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset, "pads");
  if (NodeOffset < 0) {
    DEBUG ((EFI_D_ERROR, "%a: Couldn't find pads subnode in DT\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  NodeOffset = fdt_subnode_offset (DeviceTreeNode->DeviceTreeBase, NodeOffset, "usb2");
  if (NodeOffset < 0) {
    DEBUG ((EFI_D_ERROR, "%a: Couldn't find pads->usb2 subnode in DT\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  ClockIds = fdt_getprop (DeviceTreeNode->DeviceTreeBase, NodeOffset, "clocks", &ClocksLength);
  if ((ClockIds == 0) || (ClocksLength == 0)) {
    PlatConfig->NumUsb2Clocks = 0;
    DEBUG ((EFI_D_ERROR, "%a: Couldn't find usb2 pad's clocks property in DT\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  } else {
    if ((ClocksLength % (sizeof (UINT32) * 2)) != 0) {
      DEBUG ((EFI_D_ERROR, "%a, Clock length(%d) unexpected\n", __FUNCTION__, ClocksLength));
      return EFI_UNSUPPORTED;
    }

    PlatConfig->NumUsb2Clocks = ClocksLength / (sizeof (UINT32) * 2);
  }

  BpmpPhandle = SwapBytes32 (ClockIds[0]);
  ASSERT (BpmpPhandle <= MAX_UINT16);
  PlatConfig->Usb2ClockIds = AllocateZeroPool (sizeof (UINT32) * PlatConfig->NumUsb2Clocks);
  for (Index = 0; Index < PlatConfig->NumUsb2Clocks; Index++) {
    PlatConfig->Usb2ClockIds[Index] = ((BpmpPhandle << 16) | SwapBytes32 (ClockIds[2 * Index + 1]));
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
InitPlatInfo (
  USBPADCTL_DXE_PRIVATE  *Private
  )
{
  PORT_INFO                               *Ports = NULL, *Usb2Ports, *Usb3Ports;
  UINT32                                  i;
  INT32                                   NodeOffset = -1;
  INT32                                   PortsOffset, PropertySize;
  CONST VOID                              *Property  = NULL;
  BOOLEAN                                 PortsFound = FALSE;
  CHAR8                                   Name[7];
  UINTN                                   CharCount;
  PADCTL_PLAT_CONFIG                      *PlatConfig     = &(Private->PlatConfig);
  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode = Private->DeviceTreeNode;

  Ports = AllocateZeroPool ((PlatConfig->NumHsPhys + PlatConfig->NumSsPhys) * sizeof (PORT_INFO));
  if (NULL == Ports) {
    DEBUG ((EFI_D_ERROR, "%a: Failed to allocate Port Memory\r\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  PlatConfig->Usb2Ports = Ports;
  PlatConfig->Usb3Ports = PlatConfig->Usb2Ports + PlatConfig->NumHsPhys;

  Usb2Ports = PlatConfig->Usb2Ports;
  Usb3Ports = PlatConfig->Usb3Ports;

  if (FindUsb2PadClocks (PlatConfig, DeviceTreeNode) != EFI_SUCCESS) {
    DEBUG ((EFI_D_ERROR, "Couldn't find USB2 Clocks Info in Device Tree\n"));
    FreePool (Ports);
    return EFI_UNSUPPORTED;
  }

  /* Finding the USB2 Ports that are enabled on the Platform */
  PortsOffset = fdt_subnode_offset (DeviceTreeNode->DeviceTreeBase, DeviceTreeNode->NodeOffset, "ports");
  if (PortsOffset < 0) {
    DEBUG ((EFI_D_ERROR, "Couldn't find USB Ports\n"));
    FreePool (Ports);
    return EFI_UNSUPPORTED;
  }

  /* Configuring USB2 Ports Information */
  for (i = 0; i < PlatConfig->NumHsPhys; i++) {
    /* The Port is disabled by default */
    Usb2Ports[i].PortEnabled = FALSE;

    CharCount  = AsciiSPrint (Name, sizeof (Name), "usb2-%u", i);
    NodeOffset = fdt_subnode_offset (DeviceTreeNode->DeviceTreeBase, PortsOffset, Name);
    if (NodeOffset < 0) {
      continue;
    }

    Property = fdt_getprop (DeviceTreeNode->DeviceTreeBase, NodeOffset, "status", &PropertySize);
    if (Property == NULL) {
      DEBUG ((EFI_D_ERROR, "Couldnt Find the USB Port Status\n"));
      continue;
    }

    /* The Port is disabled */
    if (AsciiStrCmp (Property, "okay") != 0) {
      continue;
    }

    Property = fdt_getprop (DeviceTreeNode->DeviceTreeBase, NodeOffset, "mode", &PropertySize);
    if (Property == NULL) {
      DEBUG ((EFI_D_ERROR, "%a: Couldn't Find the %a Port Mode\n", __FUNCTION__, Name));
      continue;
    }

    /* This Port is neither Host Mode nor Otg Mode. Dont touch Device Mode Ports */
    if ((AsciiStrCmp (Property, "otg") != 0) && (AsciiStrCmp (Property, "host") != 0)) {
      continue;
    }

    /* Now get the VBUS Supply of the Port */
    Property = fdt_getprop (DeviceTreeNode->DeviceTreeBase, NodeOffset, "vbus-supply", &PropertySize);
    if ((Property != NULL) && (PropertySize == sizeof (UINT32))) {
      Usb2Ports[i].VbusSupply = SwapBytes32 (*(UINT32 *)Property);
    } else {
      DEBUG ((EFI_D_ERROR, "Couldn't find Vbus Supply for Port: %a\n", Name));
      continue;
    }

    /* Check if OverCurrent Handling is Enabled on Port */
    Property = fdt_getprop (DeviceTreeNode->DeviceTreeBase, NodeOffset, "nvidia,oc-pin", &PropertySize);
    if ((Property != NULL) && (PropertySize == sizeof (UINT32))) {
      Usb2Ports[i].OcPin     = SwapBytes32 (*(UINT32 *)Property);
      Usb2Ports[i].OcEnabled = TRUE;
    } else {
      Usb2Ports[i].OcEnabled = FALSE;
    }

    /* Enabling this Port as we found necessary Port Info */
    Usb2Ports[i].PortEnabled = TRUE;
    PortsFound               = TRUE;
  }

  /* Configuring USB3 Ports Information */
  for (i = 0; i < PlatConfig->NumSsPhys; i++) {
    /* The Port is disabled by default */
    Usb3Ports[i].PortEnabled = FALSE;

    CharCount  = AsciiSPrint (Name, sizeof (Name), "usb3-%u", i);
    NodeOffset = fdt_subnode_offset (DeviceTreeNode->DeviceTreeBase, PortsOffset, Name);
    if (NodeOffset < 0) {
      continue;
    }

    Property = fdt_getprop (DeviceTreeNode->DeviceTreeBase, NodeOffset, "status", &PropertySize);
    if (Property == NULL) {
      DEBUG ((EFI_D_ERROR, "%a: Couldnt Find the %a Port Status\n", __FUNCTION__, Name));
      continue;
    }

    /* The Port is not enabled */
    if (AsciiStrCmp (Property, "okay") != 0) {
      continue;
    }

    /* Get the USB2 Companion Port Information. If we cant find USB2 Companion Port then dont enable
     * the USB3 Port. We could have enabled USB3 only port but we always had USB3 and USB2 provided
     * together on same port and Vbus-supply for the port is provided through the USB2 Companion
     * Port's DT Entry
     */
    Property = fdt_getprop (DeviceTreeNode->DeviceTreeBase, NodeOffset, "nvidia,usb2-companion", &PropertySize);
    if ((Property != NULL) && (PropertySize == sizeof (UINT32))) {
      Usb3Ports[i].CompanionPort = SwapBytes32 (*(UINT32 *)Property);

      /* Invalid Usb2 Companion Port provided in DT. Dont Enable this port */
      if (Usb3Ports[i].CompanionPort >= PlatConfig->NumHsPhys) {
        continue;
      }
    } else {
      DEBUG ((EFI_D_ERROR, "%a: Cant find USB2 Companion Port for %a\n", __FUNCTION__, Name));
      continue;
    }

    /* Return if the USB2 Companion Port is not enabled correctly in DT.
     * Returning because Vbus Supply for Port is currently provided only
     * in USB2 DT Entry and vbus wont be enabled unless USB2 port is enabled
     */
    if (Usb2Ports[Usb3Ports[i].CompanionPort].PortEnabled == FALSE) {
      DEBUG ((EFI_D_ERROR, "%a:USB2 Companion Port for %a is not enabled in DT\n", __FUNCTION__, Name));
      continue;
    }

    /* Now get the OC Pin information from USB2 Companion Port */
    if (Usb2Ports[Usb3Ports[i].CompanionPort].OcEnabled == TRUE) {
      Usb3Ports[i].OcEnabled = TRUE;
      Usb3Ports[i].OcPin     = Usb2Ports[Usb3Ports[i].CompanionPort].OcPin;
    }

    /* Enable the USB3 Port as we got all the necessary Information */
    Usb3Ports[i].PortEnabled = TRUE;
    DEBUG ((EFI_D_INFO, "Usb SS Port: %u Enabled\n", i));
  }

  /* If atleast one port is enabled, return Success */
  if (PortsFound == TRUE) {
    return EFI_SUCCESS;
  } else {
    return EFI_UNSUPPORTED;
  }
}

STATIC
EFI_STATUS
ReadFuseCalibration (
  USBPADCTL_DXE_PRIVATE  *Private
  )
{
  UINT32                 RegVal, i;
  PADCTL_PLAT_CONFIG     *PlatConfig = &(Private->PlatConfig);
  NVIDIA_EFUSE_PROTOCOL  *mEfuse     = Private->mEfuse;

  mEfuse->ReadReg (mEfuse, FUSE_USB_CALIB_0, &RegVal);

  /* Read Platform Specific Squelch Level and TermRangeAdj Values */
  PlatConfig->FuseHsSquelchLevel = (RegVal & FUSE_USB_CALIB_SQUELCHLEVEL_MASK) >> FUSE_USB_CALIB_SQUELCHLEVEL_SHIFT;
  PlatConfig->FuseHsTermRangeAdj = (RegVal & FUSE_USB_CALIB_TERMRANGEADJ_MASK) >> FUSE_USB_CALIB_TERMRANGEADJ_SHIFT;

  /* Read the PAD Specific HS CURR LEVEL Value from Fuse */
  for (i = 0; i < PlatConfig->NumHsPhys; i++) {
    PlatConfig->Usb2Ports[i].FuseHsCurrLevel = (RegVal >> HS_CURR_LEVEL_PADX_SHIFT (i)) & HS_CURR_LEVEL_PAD_MASK;
  }

  /* Read Platform Specific RpdCtrl Value */
  mEfuse->ReadReg (mEfuse, FUSE_USB_CALIB_EXT_0, &RegVal);
  PlatConfig->FuseRpdCtrl = RegVal & FUSE_USB_CALIB_EXT_RPD_CTRL_MASK;
  return EFI_SUCCESS;
}

STATIC
VOID
Usb2PhyInit (
  USBPADCTL_DXE_PRIVATE  *Private
  )
{
  UINT32              RegData, i;
  PADCTL_PLAT_CONFIG  *PlatConfig = &(Private->PlatConfig);
  PORT_INFO           *Usb2Ports  = PlatConfig->Usb2Ports;

  for (i = 0; i < PlatConfig->NumHsPhys; i++) {
    /* Enable PADS only for Ports that are enabled in DT */
    if (Usb2Ports[i].PortEnabled == FALSE) {
      continue;
    }

    /* reset VBUS&ID OVERRIDE */
    RegData  = PadCtlRead (Private, XUSB_PADCTL_USB2_VBUS_ID);
    RegData &= ~VBUS_OVERRIDE;
    RegData &= ~ID_OVERRIDE(~0);
    RegData |= ID_OVERRIDE_FLOATING;
    PadCtlWrite (Private, XUSB_PADCTL_USB2_VBUS_ID, RegData);

    /* ENABLE VBUS */
    RegData  = PadCtlRead (Private, XUSB_PADCTL_VBUS_OC_MAP_0);
    RegData |= VBUS_ENABLE (i);
    PadCtlWrite (Private, XUSB_PADCTL_VBUS_OC_MAP_0, RegData);
  }
}

STATIC
VOID
Usb2BiasPadPowerOn (
  USBPADCTL_DXE_PRIVATE  *Private
  )
{
  UINT32  RegVal;

  /* Program hs_squelch_level and power up the BIAS Pad */
  RegVal  = PadCtlRead (Private, XUSB_PADCTL_USB2_BIAS_PAD_CTL0);
  RegVal &= ~BIAS_PAD_PD;
  RegVal &= ~HS_SQUELCH_LEVEL(~0);
  RegVal &= ~HS_DISCON_LEVEL(~0);
  PadCtlWrite (Private, XUSB_PADCTL_USB2_BIAS_PAD_CTL0, RegVal);

  /* Start Bias PAD Tracking. usb2_trk clock should arleady be enabled
   * by now when this driver is loaded by DeviceDiscovery Library */
  RegVal  = PadCtlRead (Private, XUSB_PADCTL_USB2_BIAS_PAD_CTL1);
  RegVal &= ~USB2_TRK_START_TIMER(~0);
  RegVal |= USB2_TRK_START_TIMER (0x1E);
  RegVal &= ~USB2_TRK_DONE_RESET_TIMER(~0);
  RegVal |= USB2_TRK_DONE_RESET_TIMER (0xA);
  PadCtlWrite (Private, XUSB_PADCTL_USB2_BIAS_PAD_CTL1, RegVal);

  gBS->Stall (1);

  RegVal  = PadCtlRead (Private, XUSB_PADCTL_USB2_BIAS_PAD_CTL1);
  RegVal &= ~USB2_PD_TRK;
  PadCtlWrite (Private, XUSB_PADCTL_USB2_BIAS_PAD_CTL1, RegVal);
}

STATIC
VOID
Usb2PadPowerOn (
  USBPADCTL_DXE_PRIVATE  *Private,
  UINT32                 Index
  )
{
  UINT32  RegData;

  Usb2BiasPadPowerOn (Private);

  gBS->Stall (2);

  /* Clear Each PAD's PD and PD_DR Bits */
  RegData  = PadCtlRead (Private, USB2_OTG_PADX_CTL_0 (Index));
  RegData &= ~USB2_OTG_PD;
  PadCtlWrite (Private, USB2_OTG_PADX_CTL_0 (Index), RegData);

  RegData  = PadCtlRead (Private, USB2_OTG_PADX_CTL_1 (Index));
  RegData &=  ~USB2_OTG_PD_DR;
  PadCtlWrite (Private, USB2_OTG_PADX_CTL_1 (Index), RegData);
}

STATIC
VOID
Usb2PhyPowerOn (
  USBPADCTL_DXE_PRIVATE  *Private
  )
{
  UINT32              RegData, i;
  PADCTL_PLAT_CONFIG  *PlatConfig = &(Private->PlatConfig);
  PORT_INFO           *Usb2Ports  = PlatConfig->Usb2Ports;

  for (i = 0; i < PlatConfig->NumHsPhys; i++) {
    /* Enable PADS only for Ports that are enabled in DT */
    if (Usb2Ports[i].PortEnabled == FALSE) {
      continue;
    }

    /* Assign Each PADS to USB instead of UART */
    RegData  = PadCtlRead (Private, XUSB_PADCTL_USB2_PAD_MUX_0);
    RegData &= ~(USB2_PAD_MUX_PORT_MASK << USB2_PAD_MUX_PORT_SHIFT (i));
    RegData |= (PAD_MUX_PORT_XUSB << USB2_PAD_MUX_PORT_SHIFT (i));
    PadCtlWrite (Private, XUSB_PADCTL_USB2_PAD_MUX_0, RegData);

    /* Assign port capabilities */
    RegData  = PadCtlRead (Private, XUSB_PADCTL_USB2_PORT_CAP_0);
    RegData &= ~(USB2_PORT_CAP_MASK << USB2_PORTX_CAP_SHIFT (i));
    RegData |= (PORT_CAP_HOST << USB2_PORTX_CAP_SHIFT (i));
    PadCtlWrite (Private, XUSB_PADCTL_USB2_PORT_CAP_0, RegData);

    gBS->Stall (1);

    Usb2PadPowerOn (Private, i);
  }
}

/**
  This function Initializes USB HW.

  @param[in]     This                The instance of NVIDIA_USBPADCTL_PROTOCOL.

  @return EFI_SUCCESS                Successfully programmed PAD Registers.
  @return EFI_DEVICE_ERROR           Other error occured.
**/
EFI_STATUS
InitUsbHw234 (
  IN  NVIDIA_USBPADCTL_PROTOCOL  *This
  )
{
  USBPADCTL_DXE_PRIVATE  *Private;
  EFI_STATUS             Status;
  TEGRA_PLATFORM_TYPE    PlatformType;

  Status = EFI_SUCCESS;

  if (NULL == This) {
    return EFI_INVALID_PARAMETER;
  }

  PlatformType = TegraGetPlatform ();
  Private      = PADCTL_PRIVATE_DATA_FROM_THIS (This);

  /* XUSB PADCTL Block's clocks are enabled and corresponding RESETs are
   * deasserted by the DeviceDiscovery Lib Driver when PadctlDriver is loaded
   */

  /* Initialize Platform specific USB Ports information from DT */
  Status = InitPlatInfo (Private);
  if (Status != EFI_SUCCESS) {
    return Status;
  }

  if (PlatformType == TEGRA_PLATFORM_SILICON) {
    /* Store the USB Calibration Values read from Fuse Registers */
    if (ENABLE_FUSE) {
      ReadFuseCalibration (Private);
    }

    /* Initialize bias pad and perform Tracking */
    InitBiasPad (Private);

    /* Iniitalize Individial USB2 pads */
    InitUsb2PadX (Private);

    /* PinMux Programming is taken care outside this driver. If USB behavior is not
     * as expected, the PinMux Register Values for USB should be double checked
     */

    /* Local override for VBUS and ID status reporting.
     * Clear false reporting of VBUS and ID status changes.
     */
    VbusIdOverride (Private);

    /* UPHY programming is currently done in BPMP to support Super Speed
     * In Later chips if BPMP is not present then UPHY Programming should
     * be done in this driver
     */

    InitUsb3PadX (Private);

    /* Assign over current signal mapping for usb 2.0 and SS ports
     * Clear false reporting of over current events
     * Enable VBUS for the host ports
     */
    Status = EnableVbus (Private);
  } else {
    Usb2PhyInit (Private);

    Usb2PhyPowerOn (Private);
  }

  return Status;
}

/**
  This function DeInitializes USB HW. Specifically it cleans up Over Current
  Handling, disable Vbus and power down USB Pads. Otherwise when kernel is
  booted HW might encounter some spurious Over Current Events

  @param[in]     This              The instance of NVIDIA_USBPADCTL_PROTOCOL.
**/
VOID
DeInitUsbHw234 (
  IN  NVIDIA_USBPADCTL_PROTOCOL  *This
  )
{
  USBPADCTL_DXE_PRIVATE  *Private;
  UINT32                 i, RegData;
  PADCTL_PLAT_CONFIG     *PlatConfig;
  PORT_INFO              *Usb2Ports;

  if (NULL == This) {
    return;
  }

  Private    = PADCTL_PRIVATE_DATA_FROM_THIS (This);
  PlatConfig = &(Private->PlatConfig);
  Usb2Ports  = PlatConfig->Usb2Ports;

  /* Disable Over Current Handling and VBUS */
  DisableVbus (Private);

  /* Power Down Individual USB2 Pads */
  for (i = 0; i < PlatConfig->NumHsPhys; i++) {
    if (Usb2Ports[i].PortEnabled == FALSE) {
      continue;
    }

    /* Clear Each PAD's PD and PD_DR Bits */
    RegData  = PadCtlRead (Private, USB2_OTG_PADX_CTL_0 (i));
    RegData |= USB2_OTG_PD;
    PadCtlWrite (Private, USB2_OTG_PADX_CTL_0 (i), RegData);

    RegData  = PadCtlRead (Private, USB2_OTG_PADX_CTL_1 (i));
    RegData |= USB2_OTG_PD_DR;
    PadCtlWrite (Private, USB2_OTG_PADX_CTL_1 (i), RegData);
  }

  /* Power down BIAS Pad */
  RegData  = PadCtlRead (Private, XUSB_PADCTL_USB2_BIAS_PAD_CTL0);
  RegData |= BIAS_PAD_PD;
  PadCtlWrite (Private, XUSB_PADCTL_USB2_BIAS_PAD_CTL0, RegData);

  RegData  = PadCtlRead (Private, XUSB_PADCTL_USB2_BIAS_PAD_CTL1);
  RegData |= USB2_PD_TRK;
  PadCtlWrite (Private, XUSB_PADCTL_USB2_BIAS_PAD_CTL1, RegData);

  return;
}
