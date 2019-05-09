/** @file

  Usb Pad Control Driver

  Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Protocol/Regulator.h>
#include <Protocol/EFuse.h>
#include "UsbPadCtlPrivate.h"

NVIDIA_REGULATOR_PROTOCOL    *mRegulator = NULL;
NVIDIA_EFUSE_PROTOCOL        *mEfuse = NULL;

NVIDIA_COMPATIBILITY_MAPPING gDeviceCompatibilityMap[] = {
    { "nvidia,tegra186-xusb-padctl", &gNVIDIANonDiscoverableT186UsbPadDeviceGuid },
    { NULL, NULL }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG gDeviceDiscoverDriverConfig = {
    L"NVIDIA USB Pad controller driver",   //DriverName
    TRUE,                                  //UseDriverBinding
    TRUE,                                  //AutoEnableClocks
    TRUE,                                  //AutoSetParents
    TRUE,                                  //AutoDeassertReset
    FALSE,                                 //AutoDeassertPg
    TRUE                                   //SkipEdkiiNondiscoverableInstall
};

EFI_STATUS
XhciInitUsb2PadX (
  VOID
  )
{
  EFI_STATUS e = EFI_SUCCESS;
  UINT32 RegData, UsbCalib, UsbCalibExt, HsCurrLevel, TermRangeAdj;
  UINT32 RpdCtrl;

  /* Get hs_curr_level, term_range_adj, rpd_ctrl PAD Parameter values
   * from FUSE Config Registers
   */
  mEfuse->ReadReg(mEfuse, FUSE_USB_CALIB_0, &UsbCalib);
  HsCurrLevel = UsbCalib & FUSE_USB_CALIB_HS_CURR_LEVEL_MASK;
  TermRangeAdj = UsbCalib & FUSE_USB_CALIB_TERMRANGEADJ_MASK;
  TermRangeAdj = TermRangeAdj >> FUSE_USB_CALIB_TERMRANGEADJ_SHIFT;
  mEfuse->ReadReg(mEfuse, FUSE_USB_CALIB_EXT_0, &UsbCalibExt);
  RpdCtrl = UsbCalibExt & FUSE_USB_CALIB_EXT_RPD_CTRL_MASK;

  /* Clear Each PAD's PD and PD_DR Bits */
  /* PAD 0 */
  NV_XUSB_PADCTL_READ(USB2_OTG_PAD0_CTL_0, RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD0_CTL_0, PD, SW_DEFAULT,
                                                                       RegData);
  NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD0_CTL_0, RegData);
  NV_XUSB_PADCTL_READ(USB2_OTG_PAD0_CTL_1, RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD0_CTL_1, PD_DR,
                                                  SW_DEFAULT, RegData);
  NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD0_CTL_1, RegData);
  /* PAD 1 */
  NV_XUSB_PADCTL_READ(USB2_OTG_PAD1_CTL_0, RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD1_CTL_0, PD, SW_DEFAULT,
                                                                       RegData);
  NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD1_CTL_0, RegData);
  NV_XUSB_PADCTL_READ(USB2_OTG_PAD1_CTL_1, RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD1_CTL_1, PD_DR,
                                                  SW_DEFAULT, RegData);
  NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD1_CTL_1, RegData);
  /* PAD 2 */
  NV_XUSB_PADCTL_READ(USB2_OTG_PAD2_CTL_0, RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD2_CTL_0, PD,
                                               SW_DEFAULT, RegData);
  NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD2_CTL_0, RegData);
  NV_XUSB_PADCTL_READ(USB2_OTG_PAD2_CTL_1, RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD2_CTL_1, PD_DR,
                                                   SW_DEFAULT, RegData);
  NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD2_CTL_1, RegData);
  /* PAD 3 */
  NV_XUSB_PADCTL_READ(USB2_OTG_PAD3_CTL_0, RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD3_CTL_0, PD,
                                               SW_DEFAULT, RegData);
  NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD3_CTL_0, RegData);
  NV_XUSB_PADCTL_READ(USB2_OTG_PAD3_CTL_1, RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD3_CTL_1, PD_DR,
                                                  SW_DEFAULT, RegData);
  NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD3_CTL_1, RegData);

  /* Assign Each PADS to USB instead of UART */
  NV_XUSB_PADCTL_READ(USB2_PAD_MUX, RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PAD_MUX, USB2_OTG_PAD_PORT0,
                                                              XUSB, RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PAD_MUX, USB2_OTG_PAD_PORT1,
                                                             XUSB, RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PAD_MUX, USB2_OTG_PAD_PORT2,
                                                              XUSB, RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PAD_MUX, USB2_OTG_PAD_PORT3,
                                                              XUSB, RegData);
  NV_XUSB_PADCTL_WRITE(USB2_PAD_MUX, RegData);

  /* Assign port capabilities for 2.0 and superspeed ports */
  NV_XUSB_PADCTL_READ(USB2_PORT_CAP, RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PORT_CAP, PORT0_CAP, HOST_ONLY,
                                                                       RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PORT_CAP, PORT1_CAP, HOST_ONLY,
                                                                       RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PORT_CAP, PORT2_CAP, HOST_ONLY,
                                                                       RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_PORT_CAP, PORT3_CAP, HOST_ONLY,
                                                                       RegData);
  NV_XUSB_PADCTL_WRITE(USB2_PORT_CAP, RegData);

  /* Program PD_ZI, TERM_SEL, HsCurrLevel, RpdCtrl and term_range
   * for all PAD's
   */
  /* PAD 0 */
  NV_XUSB_PADCTL_READ(USB2_OTG_PAD0_CTL_0, RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD0_CTL_0, PD_ZI,
                                                           SW_DEFAULT, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD0_CTL_0, TERM_SEL, 1,
                                                                       RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD0_CTL_0, HS_CURR_LEVEL,
                                                          HsCurrLevel, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD0_CTL_0, LS_FSLEW, 6, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD0_CTL_0, LS_RSLEW, 6, RegData);
  NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD0_CTL_0, RegData);
  NV_XUSB_PADCTL_READ(USB2_OTG_PAD0_CTL_1, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD0_CTL_1, TERM_RANGE_ADJ,
                                                         TermRangeAdj, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD0_CTL_1, RPD_CTRL,
                                                        RpdCtrl, RegData);
  NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD0_CTL_1, RegData);
  NV_XUSB_PADCTL_READ(USB2_OTG_PAD0_CTL_1, RegData);
  /* PAD 1 */
  NV_XUSB_PADCTL_READ(USB2_OTG_PAD1_CTL_0, RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD1_CTL_0, PD_ZI,
                                                  SW_DEFAULT, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD1_CTL_0, TERM_SEL, 1,
                                                                    RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD1_CTL_0, HS_CURR_LEVEL,
                                                         HsCurrLevel, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD1_CTL_0, LS_FSLEW, 6, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD1_CTL_0, LS_RSLEW, 6, RegData);
  NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD1_CTL_0, RegData);
  NV_XUSB_PADCTL_READ(USB2_OTG_PAD1_CTL_1, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD1_CTL_1, TERM_RANGE_ADJ,
                                                         TermRangeAdj, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD1_CTL_1, RPD_CTRL,
                                                        RpdCtrl, RegData);
  NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD1_CTL_1, RegData);
  NV_XUSB_PADCTL_READ(USB2_OTG_PAD1_CTL_1, RegData);
  /* PAD 2 */
  NV_XUSB_PADCTL_READ(USB2_OTG_PAD2_CTL_0, RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD2_CTL_0, PD_ZI,
                                                  SW_DEFAULT, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD2_CTL_0, TERM_SEL, 1,
                                                                    RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD2_CTL_0, HS_CURR_LEVEL,
                                                         HsCurrLevel, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD2_CTL_0, LS_FSLEW, 6, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD2_CTL_0, LS_RSLEW, 6, RegData);
  NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD2_CTL_0, RegData);
  NV_XUSB_PADCTL_READ(USB2_OTG_PAD2_CTL_1, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD2_CTL_1, TERM_RANGE_ADJ,
                                                         TermRangeAdj, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD2_CTL_1, RPD_CTRL,
                                                        RpdCtrl, RegData);
  NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD2_CTL_1, RegData);
  NV_XUSB_PADCTL_READ(USB2_OTG_PAD2_CTL_1, RegData);
  /* PAD 3 */
  NV_XUSB_PADCTL_READ(USB2_OTG_PAD3_CTL_0, RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OTG_PAD3_CTL_0, PD_ZI,
                                                  SW_DEFAULT, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD3_CTL_0, TERM_SEL, 1,
                                                                    RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD3_CTL_0, HS_CURR_LEVEL,
                                                         HsCurrLevel, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD3_CTL_0, LS_FSLEW, 6, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD3_CTL_0, LS_RSLEW, 6, RegData);
  NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD3_CTL_0, RegData);
  NV_XUSB_PADCTL_READ(USB2_OTG_PAD3_CTL_1, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD3_CTL_1, TERM_RANGE_ADJ,
                                                         TermRangeAdj, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_OTG_PAD3_CTL_1, RPD_CTRL,
                                                        RpdCtrl, RegData);
  NV_XUSB_PADCTL_WRITE(USB2_OTG_PAD3_CTL_1, RegData);
  NV_XUSB_PADCTL_READ(USB2_OTG_PAD3_CTL_1, RegData);

  /* USB Pad protection circuit activation for all PADS. Programmed
   * VREG_DIR = HOST(2) instead of Device(1) for all PADS as we dont
   * support Device Mode in UEFI currently. If Later Chips need Device
   * Mode then program the corresponding Device PADS VREG_DIR = Device(1)
   * for protection against sinking more current
   */
  /* PAD 0 */
  NV_XUSB_PADCTL_READ(USB2_BATTERY_CHRG_OTGPAD0_CTL1, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL,
    USB2_BATTERY_CHRG_OTGPAD0_CTL1, PD_VREG, 0x0, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL,
    USB2_BATTERY_CHRG_OTGPAD0_CTL1, VREG_DIR, VREG_DIR_OUT, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL,
    USB2_BATTERY_CHRG_OTGPAD0_CTL1, VREG_LEV, VREG_LEVEL_2A, RegData);
  NV_XUSB_PADCTL_WRITE(USB2_BATTERY_CHRG_OTGPAD0_CTL1, RegData);
  /* PAD 1 */
  NV_XUSB_PADCTL_READ(USB2_BATTERY_CHRG_OTGPAD1_CTL1, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL,
    USB2_BATTERY_CHRG_OTGPAD1_CTL1, PD_VREG, 0x0, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL,
    USB2_BATTERY_CHRG_OTGPAD1_CTL1, VREG_DIR, VREG_DIR_OUT, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL,
    USB2_BATTERY_CHRG_OTGPAD1_CTL1, VREG_LEV, VREG_LEVEL_2A, RegData);
  NV_XUSB_PADCTL_WRITE(USB2_BATTERY_CHRG_OTGPAD1_CTL1, RegData);
  /* PAD 2 */
  NV_XUSB_PADCTL_READ(USB2_BATTERY_CHRG_OTGPAD2_CTL1, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL,
    USB2_BATTERY_CHRG_OTGPAD2_CTL1, PD_VREG, 0x0, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL,
    USB2_BATTERY_CHRG_OTGPAD2_CTL1, VREG_DIR, VREG_DIR_OUT, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL,
    USB2_BATTERY_CHRG_OTGPAD2_CTL1, VREG_LEV, VREG_LEVEL_2A, RegData);
  NV_XUSB_PADCTL_WRITE(USB2_BATTERY_CHRG_OTGPAD2_CTL1, RegData);
  /* PAD 3 */
  NV_XUSB_PADCTL_READ(USB2_BATTERY_CHRG_OTGPAD3_CTL1, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL,
    USB2_BATTERY_CHRG_OTGPAD3_CTL1, PD_VREG, 0x0, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL,
    USB2_BATTERY_CHRG_OTGPAD3_CTL1, VREG_DIR, VREG_DIR_OUT, RegData);
  RegData = NV_FLD_SET_DRF_NUM(XUSB_PADCTL,
    USB2_BATTERY_CHRG_OTGPAD3_CTL1, VREG_LEV, VREG_LEVEL_2A, RegData);
  NV_XUSB_PADCTL_WRITE(USB2_BATTERY_CHRG_OTGPAD3_CTL1, RegData);

  /* Disable over current signal mapping for XUSB and 2.0 ports
   * Need to do this before enabling VBUS
   */
  NV_XUSB_PADCTL_READ(USB2_OC_MAP, RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OC_MAP, PORT0_OC_PIN,
                                      OC_DETECTION_DISABLED, RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OC_MAP, PORT1_OC_PIN,
                                      OC_DETECTION_DISABLED, RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OC_MAP, PORT2_OC_PIN,
                                      OC_DETECTION_DISABLED, RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OC_MAP, PORT3_OC_PIN,
                                      OC_DETECTION_DISABLED, RegData);
  NV_XUSB_PADCTL_WRITE(USB2_OC_MAP, RegData);
  NV_XUSB_PADCTL_READ(VBUS_OC_MAP, RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, VBUS_OC_MAP, VBUS_ENABLE1_OC_MAP,
                                             OC_DETECTION_DISABLED, RegData);
  RegData = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, VBUS_OC_MAP, VBUS_ENABLE0_OC_MAP,
                                             OC_DETECTION_DISABLED, RegData);
  NV_XUSB_PADCTL_WRITE(USB2_OC_MAP, RegData);

  return e;
}

EFI_STATUS
XhciInitBiasPad (
  VOID
  )
{
  UINT32 RegVal, hs_squelch_level;
  EFI_STATUS e = EFI_SUCCESS;

  /* Program hs_squelch_level and power up the BIAS Pad */
  RegVal = MmioRead32(NV_ADDRESS_MAP_FUSE_BASE + FUSE_USB_CALIB_0);
  hs_squelch_level = NV_DRF_VAL(FUSE, USB_CALIB, HS_SQUELCH, RegVal);
  NV_XUSB_PADCTL_READ(USB2_BIAS_PAD_CTL_0, RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_BIAS_PAD_CTL_0,
                                       PD, SW_DEFAULT, RegVal);
  RegVal = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_BIAS_PAD_CTL_0,
                   HS_SQUELCH_LEVEL, hs_squelch_level, RegVal);
  RegVal = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_BIAS_PAD_CTL_0,
                   HS_DISCON_LEVEL, 0x7, RegVal);
  NV_XUSB_PADCTL_WRITE(USB2_BIAS_PAD_CTL_0, RegVal);
  NV_XUSB_PADCTL_READ(USB2_BIAS_PAD_CTL_0, RegVal);

  /* Start Bias PAD Tracking */
  NV_XUSB_PADCTL_READ(USB2_BIAS_PAD_CTL_1, RegVal);
  RegVal |= NV_DRF_NUM(XUSB_PADCTL, USB2_BIAS_PAD_CTL_1,
                                 TRK_START_TIMER, 0x1E);
  RegVal |= NV_DRF_NUM(XUSB_PADCTL, USB2_BIAS_PAD_CTL_1,
                             TRK_DONE_RESET_TIMER, 0xA);
  NV_XUSB_PADCTL_WRITE(USB2_BIAS_PAD_CTL_1, RegVal);
  NV_XUSB_PADCTL_READ(USB2_BIAS_PAD_CTL_1, RegVal);
  /* Not Enabling/Tracking HSIC PAD as we are not using HSIC Port */
  gBS->Stall(1);
  NV_XUSB_PADCTL_READ(USB2_BIAS_PAD_CTL_1, RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_BIAS_PAD_CTL_1, PD_TRK,
                                                   SW_DEFAULT, RegVal);
  NV_XUSB_PADCTL_WRITE(USB2_BIAS_PAD_CTL_1, RegVal);
  NV_XUSB_PADCTL_READ(USB2_BIAS_PAD_CTL_1, RegVal);

  return e;
}

EFI_STATUS XhciVbusOverride(void)
{
  UINT32 RegVal;
  /* Local override for VBUS and ID status reporting. */
  NV_XUSB_PADCTL_READ(USB2_VBUS_ID, RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_VBUS_ID, ID_SOURCE_SELECT,
                                                     ID_OVERRIDE, RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_VBUS_ID, VBUS_SOURCE_SELECT,
                                                     VBUS_OVERRIDE, RegVal);
  RegVal = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_VBUS_ID, ID_OVERRIDE, 0x0,
                                                                  RegVal);
  NV_XUSB_PADCTL_WRITE(USB2_VBUS_ID, RegVal);

  /* Clear false reporting of VBUS and ID status changes. */
  NV_XUSB_PADCTL_READ(USB2_VBUS_ID, RegVal);
  RegVal = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_VBUS_ID, IDDIG_ST_CHNG, 0x1,
                                                                    RegVal);
  RegVal = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_VBUS_ID, VBUS_VALID_ST_CHNG,
                                                               0x1, RegVal);
  NV_XUSB_PADCTL_WRITE(USB2_VBUS_ID, RegVal);

  return EFI_SUCCESS;
}


void XhciReleaseSsWakestateLatch(void)
{
  UINT32 RegVal;

  NV_XUSB_PADCTL_READ(ELPG_PROGRAM_1, RegVal);
  RegVal = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1,
                         SSP2_ELPG_CLAMP_EN, 0x0, RegVal);
  RegVal = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1,
                   SSP2_ELPG_CLAMP_EN_EARLY, 0x0, RegVal);
  RegVal = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1,
                   SSP2_ELPG_VCORE_DOWN, 0x0, RegVal);
  RegVal = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1,
                         SSP1_ELPG_CLAMP_EN, 0x0, RegVal);
  RegVal = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1,
                   SSP1_ELPG_CLAMP_EN_EARLY, 0x0, RegVal);
  RegVal = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1,
                       SSP1_ELPG_VCORE_DOWN, 0x0, RegVal);
  RegVal = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1,
                         SSP0_ELPG_CLAMP_EN, 0x0, RegVal);
  RegVal = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1,
                   SSP0_ELPG_CLAMP_EN_EARLY, 0x0, RegVal);
  RegVal = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, ELPG_PROGRAM_1,
                       SSP0_ELPG_VCORE_DOWN, 0x0, RegVal);
  NV_XUSB_PADCTL_WRITE(ELPG_PROGRAM_1, RegVal);
}

/* The Regulators are for T186. These might have to be changed for
 * later chips
 */
void XhciInitRegulators(void)
{
  EFI_STATUS Status;

  Status = mRegulator-> Enable(mRegulator, REG_VDD_USB0_5V, TRUE);
  if EFI_ERROR(Status) {
    DEBUG ((EFI_D_ERROR, "Couldn't enable Regulator: vdd-usb0-5v\n"));
  }

  Status = mRegulator-> Enable(mRegulator, REG_VDD_USB1_5V, TRUE);
  if EFI_ERROR(Status) {
    DEBUG ((EFI_D_ERROR, "Couldn't enable Regulator: vdd-usb1-5v\n"));
  }
}

void XhciEnableVbus(void)
{
  UINT32 RegVal;

  /* Assign over current signal mapping for usb 2.0 and SS ports */
  NV_XUSB_PADCTL_READ(USB2_OC_MAP, RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OC_MAP, PORT3_OC_PIN,
                                               OC_DETECTED3, RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OC_MAP, PORT2_OC_PIN,
                                               OC_DETECTED2, RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OC_MAP, PORT1_OC_PIN,
                                               OC_DETECTED1, RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, USB2_OC_MAP, PORT0_OC_PIN,
                                               OC_DETECTED0, RegVal);
  NV_XUSB_PADCTL_WRITE(USB2_OC_MAP, RegVal);

  NV_XUSB_PADCTL_READ(SS_OC_MAP, RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, SS_OC_MAP, PORT3_OC_PIN,
                                             OC_DETECTED3, RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, SS_OC_MAP, PORT2_OC_PIN,
                                             OC_DETECTED2, RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, SS_OC_MAP, PORT1_OC_PIN,
                                             OC_DETECTED1, RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, SS_OC_MAP, PORT0_OC_PIN,
                                             OC_DETECTED0, RegVal);
  NV_XUSB_PADCTL_WRITE(SS_OC_MAP, RegVal);

  NV_XUSB_PADCTL_READ(VBUS_OC_MAP, RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, VBUS_OC_MAP, VBUS_ENABLE1_OC_MAP,
                                                      OC_DETECTED1, RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, VBUS_OC_MAP, VBUS_ENABLE0_OC_MAP,
                                                      OC_DETECTED0, RegVal);
  NV_XUSB_PADCTL_WRITE(VBUS_OC_MAP, RegVal);

  /* clear false reporting of over current events */
  NV_XUSB_PADCTL_READ(OC_DET, RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, OC_DET, OC_DETECTED3, YES, RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, OC_DET, OC_DETECTED2, YES, RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, OC_DET, OC_DETECTED1, YES, RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, OC_DET, OC_DETECTED0, YES, RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, OC_DET, OC_DETECTED_VBUS_PAD3, YES,
                                                                      RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, OC_DET, OC_DETECTED_VBUS_PAD2, YES,
                                                                      RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, OC_DET, OC_DETECTED_VBUS_PAD1, YES,
                                                                      RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, OC_DET, OC_DETECTED_VBUS_PAD0, YES,
                                                                      RegVal);
  NV_XUSB_PADCTL_WRITE(OC_DET, RegVal);

  gBS->Stall(1);

  /* Enable VBUS for the host ports */
  NV_XUSB_PADCTL_READ(VBUS_OC_MAP, RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, VBUS_OC_MAP, VBUS_ENABLE3, YES,
                                                                  RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, VBUS_OC_MAP, VBUS_ENABLE2, YES,
                                                                  RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, VBUS_OC_MAP, VBUS_ENABLE1, YES,
                                                                  RegVal);
  RegVal = NV_FLD_SET_DRF_DEF(XUSB_PADCTL, VBUS_OC_MAP, VBUS_ENABLE0, YES,
                                                                  RegVal);
  NV_XUSB_PADCTL_WRITE(VBUS_OC_MAP, RegVal);
}

/**
  This function Initializes USB HW.

  @param[in]     This                The instance of NVIDIA_USBPADCTL_PROTOCOL.

  @return EFI_SUCCESS                Successfully programmed PAD Registers.
  @return EFI_DEVICE_ERROR           Other error occured.
**/
STATIC
EFI_STATUS
UsbPadCtlInitializeHw (
  IN  NVIDIA_USBPADCTL_PROTOCOL  *This
  )
{
  UINT32 RegVal;

  /* XUSB PADCTL Block's clocks are enabled and corresponding RESETs are
   * deasserted by the DeviceDiscovery Lib Driver when PadctlDriver is loaded
   */

  /* Initialize Regulators */
  XhciInitRegulators();

  /* Initialize bias pad and perform Tracking */
  XhciInitBiasPad();

  /* Iniitalize Individial USB pad registers */
  XhciInitUsb2PadX();

  /* No PinMux Programming done for T186. This needs to be done in later
   * chips if required by Spec
   */

  /* Local override for VBUS and ID status reporting.
   * Clear false reporting of VBUS and ID status changes.
   */
  XhciVbusOverride();

  /* UPHY programming is currently done in BPMP to support Super Speed
   * In Later chips if BPMP is not present then UPHY Programming should
   * be done in this driver
   */

  /* Assign port capabilities(Host Only) for Super speed ports */
  NV_XUSB_PADCTL_READ(SS_PORT_CAP, RegVal);
  RegVal = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, SS_PORT_CAP, PORT0_CAP, 0x1, RegVal);
  RegVal = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, SS_PORT_CAP, PORT1_CAP, 0x1, RegVal);
  RegVal = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, SS_PORT_CAP, PORT2_CAP, 0x1, RegVal);
  RegVal = NV_FLD_SET_DRF_NUM(XUSB_PADCTL, USB2_VBUS_ID, VBUS_VALID_ST_CHNG,
                                                               0x1, RegVal);
  NV_XUSB_PADCTL_WRITE(SS_PORT_CAP, RegVal);

  /* Release XUSB SS wake logic latching */
  XhciReleaseSsWakestateLatch();

  /* Assign over current signal mapping for usb 2.0 and SS ports
   * Clear false reporting of over current events
   * Enable VBUS for the host ports
   */
  XhciEnableVbus();

  return EFI_SUCCESS;
}

/**
  Callback that will be invoked at various phases of the driver initialization

  This function allows for modification of system behavior at various points in
  the driver binding process.

  @param[in] Phase                    Current phase of the driver initialization
  @param[in] DriverHandle             Handle of the driver.
  @param[in] ControllerHandle         Handle of the controller.
  @param[in] DeviceTreeNode           Pointer to device tree node protocol is available.

  @retval EFI_SUCCESS              Operation successful.
  @retval EFI_SUCCESS              Driver does not handle this phase
  @retval others                   Error occurred
**/
EFI_STATUS
DeviceDiscoveryNotify (
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES         Phase,
  IN  EFI_HANDLE                             DriverHandle,
  IN  EFI_HANDLE                             ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL *DeviceTreeNode OPTIONAL
  )
{
  EFI_STATUS Status = EFI_SUCCESS;
  USBPADCTL_DXE_PRIVATE *Private = NULL;
  DEBUG ((EFI_D_ERROR, "%a\r\n",__FUNCTION__));

  switch (Phase) {
  case DeviceDiscoveryDriverBindingStart:
    Status = gBS->LocateProtocol (&gNVIDIARegulatorProtocolGuid, NULL,
                                                (VOID **)&mRegulator);
    if (EFI_ERROR (Status) || mRegulator == NULL) {
      DEBUG ((EFI_D_ERROR,
      "%a: Couldn't get gNVIDIARegulatorProtocolGuid Handle: %r\n",
      __FUNCTION__, Status));
      goto ErrorExit;
    }

    Status = gBS->LocateProtocol (&gNVIDIAEFuseProtocolGuid, NULL,
                                                (VOID **)&mEfuse);
    if (EFI_ERROR (Status) || mEfuse == NULL) {
      DEBUG ((EFI_D_ERROR,
      "%a: Couldn't get gNVIDIAEFuseProtocolGuid Handle: %r\n",
      __FUNCTION__, Status));
      goto ErrorExit;
    }

    Private = AllocatePool (sizeof (USBPADCTL_DXE_PRIVATE));
    if (NULL == Private) {
      DEBUG ((EFI_D_ERROR, "%a: Failed to allocate private data stucture\r\n",
                                                               __FUNCTION__));
      Status = EFI_OUT_OF_RESOURCES;
      goto ErrorExit;
    }

    Private->ImageHandle = DriverHandle;
    Private->UsbPadCtlProtocol.InitHw = UsbPadCtlInitializeHw;

    Status = gBS->InstallMultipleProtocolInterfaces (
                  &DriverHandle,
                  &gNVIDIAUsbPadCtlProtocolGuid,
                  &Private->UsbPadCtlProtocol,
                  NULL
                  );
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "%a, Failed to install protocols: %r\r\n",
                                              __FUNCTION__, Status));
      FreePool (Private);
      goto ErrorExit;
    }
    break;
  default:
    break;
  }
ErrorExit:
  return Status;
}
