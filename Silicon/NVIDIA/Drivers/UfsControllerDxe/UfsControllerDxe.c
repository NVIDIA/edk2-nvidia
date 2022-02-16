/** @file

  UFS Controller Driver

  Copyright (c) 2021, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/IoLib.h>
#include <Library/TimerLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <libfdt.h>

#include <Protocol/UfsHostControllerPlatform.h>

NVIDIA_COMPATIBILITY_MAPPING gDeviceCompatibilityMap[] = {
  { "tegra,ufs_variant", &gEdkiiNonDiscoverableUfsDeviceGuid },
  { NULL, NULL }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG gDeviceDiscoverDriverConfig = {
  .DriverName = L"NVIDIA Ufs controller driver",
  .UseDriverBinding = TRUE,
  .AutoEnableClocks = TRUE,
  .AutoDeassertReset = TRUE,
  .SkipEdkiiNondiscoverableInstall = FALSE,
  .SkipAutoDeinitControllerOnExitBootServices = TRUE
};

//
// UIC command opcodes
//
typedef enum {
  UfsUicDmeGet            = 0x01,
  UfsUicDmeSet            = 0x02,
  UfsUicDmePeerGet        = 0x03,
  UfsUicDmePeerSet        = 0x04,
  UfsUicDmePwrOn          = 0x10,
  UfsUicDmePwrOff         = 0x11,
  UfsUicDmeEnable         = 0x12,
  UfsUicDmeReset          = 0x14,
  UfsUicDmeEndpointReset  = 0x15,
  UfsUicDmeLinkStartup    = 0x16,
  UfsUicDmeHibernateEnter = 0x17,
  UfsUicDmeHibernateExit  = 0x18,
  UfsUicDmeTestMode       = 0x1A
} UFS_UIC_OPCODE;

/** UIC MIB Attributes */
#define PA_AVAIL_TX_DATA_LANES                0x1520U
#define PA_AVAIL_RX_DATA_LANES                0x1540U
#define PA_ACTIVE_TX_DATA_LANES               0x1560U
#define PA_CONNECTED_TX_DATA_LANES            0x1561U
#define PA_TX_GEAR                            0x1568U
#define PA_TX_TERMINATION                     0x1569U
#define PA_HS_SERIES                          0x156AU
#define PA_ACTIVE_RX_DATA_LANES               0x1580U
#define PA_CONNECTED_RX_DATA_LANES            0x1581U
#define PA_RX_GEAR                            0x1583U
#define PA_RX_TERMINATION                     0x1584U
#define PA_TX_HS_G1_PERPARE_LENGTH            0x1553U
#define PA_TX_HS_G2_PERPARE_LENGTH            0x1555U
#define PA_TX_HS_G3_PERPARE_LENGTH            0x1557U

#define PA_MAXRXHSGEAR                        0x1587U

#define PA_TX_HS_G1_SYNC_LENGTH               0x1552U
#define PA_TX_HS_G2_SYNC_LENGTH               0x1554U
#define PA_TX_HS_G3_SYNC_LENGTH               0x1556U

#define PA_LOCAL_TX_LCC_ENABLE                0x155EU
#define PA_PEER_TX_LCC_ENABLE                 0x155FU
#define PA_TX_TRAILING_CLOCKS                 0x1564U
#define PA_PWR_MODE                           0x1571U
#define PA_SLEEP_NO_CONFIG_TIME               0x15A2U
#define PA_STALL_NO_CONFIG_TIME               0x15A3U
#define PA_SAVE_CONFIG_TIME                   0x15A4U

#define PA_HIBERN8TIME                        0x15A7U
#define PA_TACTIVATE                          0x15A8U
#define PA_GRANULARITY                        0x15AAU

#define PWR_MODE_USER_DATA0                   0x15B0U
#define PWR_MODE_USER_DATA1                   0x15B1U
#define PWR_MODE_USER_DATA2                   0x15B2U

#define T_CPORTFLAGS                          0x4025U
#define T_CONNECTIONSTATE                     0x4020U

#define DME_LAYERENABLE                       0xD000U
#define VS_TXBURSTCLOSUREDELAY                0xD084U

#define DME_FC0PROTECTIONTIMEOUTVAL           0xD041U
#define DME_TC0REPLAYTIMEOUTVAL               0xD042U
#define DME_AFC0REQTIMEOUTVAL                 0xD043U

#define VS_DEBUGSAVECONFIGTIME                0xD0A0U
#define VS_DEBUGSAVECONFIGTIME_TREF           0x6U
#define VS_DEBUGSAVECONFIGTIME_ST_SCT         0x3U

#define SET_TREF(x)                           (((x) & 0x7UL) << 2)
#define SET_ST_SCT(x)                         (((x) & 0x3UL) << 0)

STATIC UINT32 TxBurstClosureDelay             = 0;

/** Unipro powerchange mode.
 * SLOW : PWM
 * SLOW_AUTO : PWM (but does auto burst closure for power saving)
 */
#define PWRMODE_SLOW_MODE                     0x2U
#define PWRMODE_FAST_MODE                     0x1U
#define PWRMODE_FASTAUTO_MODE                 0x4U
#define PWRMODE_SLOWAUTO_MODE                 0x5U

/**
 * @addtogroup UFS_HS_RATE
 *
 * @brief UFS HS rate
 * @{
 */
#define UFS_HS_RATE_A                         1
#define UFS_HS_RATE_B                         2
/** @} */

/*
 * UFS AUX Registers
 */

#define UFSHC_AUX_UFSHC_SW_EN_CLK_SLCG_OFFSET 0x8
#define UFSHC_CLK_OVR_ON                      BIT0
#define UFSHC_HCLK_OVR_ON                     BIT1
#define UFSHC_LP_CLK_T_CLK_OVR_ON             BIT2
#define UFSHC_CLK_T_CLK_OVR_ON                BIT3
#define UFSHC_CG_SYS_CLK_OVR_ON               BIT4
#define UFSHC_TX_SYMBOL_CLK_OVR_ON            BIT5
#define UFSHC_RX_SYMBOLCLKSELECTED_CLK_OVR_ON BIT6
#define UFSHC_PCLK_OVR_ON                     BIT7
#define UFSHC_AUX_UFSHC_STATUS_OFFSET         0x10
#define UFSHC_HIBERNATE_STATUS                BIT0
#define UFSHC_AUX_UFSHC_DEV_CTRL_OFFSET       0x14
#define UFSHC_DEV_CLK_EN                      BIT0
#define UFSHC_DEV_RESET                       BIT1

/* vendor specific pre-defined parameters */

/*
 * HCLKFrequency in MHz.
 * HCLKDIV is used to generate 1usec tick signal used by Unipro.
 */
#define UFS_VNDR_HCLKDIV_1US_TICK_OFFSET      0xCC
#define REG_UFS_VNDR_HCLKDIV                  0xFC

STATIC
EFI_STATUS
UfsDmeCmd (
  IN  EDKII_UFS_HC_DRIVER_INTERFACE *DriverInterface,
  IN  UFS_UIC_OPCODE                 OpCode,
  IN  UINT32                         Attribute,
  IN  UINT32                         InValue,
  OUT UINT32                        *OutValue OPTIONAL
) {
  EDKII_UIC_COMMAND Command;
  EFI_STATUS        Status;

  if (DriverInterface == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Command.Opcode = OpCode;
  Command.Arg1 = Attribute << 16;
  Command.Arg2 = 0;
  Command.Arg3 = InValue;

  Status = DriverInterface->UfsExecUicCommand (DriverInterface, &Command);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a DmeCmd failed - %x %x %x - %r\r\n", __FUNCTION__, OpCode, Attribute, InValue, Status));
    return Status;
  }

  if (OutValue != NULL) {
    *OutValue = Command.Arg3;
  }
  return EFI_SUCCESS;
}

/**
  Callback function for platform driver.

  @param[in]      ControllerHandle  Handle of the UFS controller.
  @param[in]      CallbackPhase     Specifies when the platform protocol is called
  @param[in, out] CallbackData      Data specific to the callback phase.
                                    For PreHce and PostHce - EDKII_UFS_HC_DRIVER_INTERFACE.
                                    For PreLinkStartup and PostLinkStartup - EDKII_UFS_HC_DRIVER_INTERFACE.

  @retval EFI_SUCCESS            Override function completed successfully.
  @retval EFI_INVALID_PARAMETER  CallbackPhase is invalid or CallbackData is NULL when phase expects valid data.
  @retval Others                 Function failed to complete.
**/
EFI_STATUS
UfsCallback (
  IN      EFI_HANDLE                           ControllerHandle,
  IN      EDKII_UFS_HC_PLATFORM_CALLBACK_PHASE CallbackPhase,
  IN OUT  VOID                                 *CallbackData
  )
{
  EFI_PHYSICAL_ADDRESS            BaseAddress;
  EFI_PHYSICAL_ADDRESS            BaseAddressAux;
  UINTN                           Size;
  EFI_STATUS                      Status;
  EDKII_UFS_HC_DRIVER_INTERFACE   *DriverInterface;
  UINT32                          Value;

  Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress, &Size);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to locate address range\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 1, &BaseAddressAux, &Size);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to locate aux address range\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  DriverInterface = (EDKII_UFS_HC_DRIVER_INTERFACE *)CallbackData;

  switch (CallbackPhase) {
  case EdkiiUfsHcPreHce:
    DeviceDiscoveryEnableClock (ControllerHandle, "mphy_force_ls_mode", TRUE);
    MicroSecondDelay (500);
    MmioAnd32 (BaseAddressAux + UFSHC_AUX_UFSHC_DEV_CTRL_OFFSET, ~UFSHC_DEV_RESET);
    break;

  case EdkiiUfsHcPostHce:
    MmioAnd32 (BaseAddressAux + UFSHC_AUX_UFSHC_SW_EN_CLK_SLCG_OFFSET, ~UFSHC_CG_SYS_CLK_OVR_ON);
    MmioAnd32 (BaseAddressAux + UFSHC_AUX_UFSHC_DEV_CTRL_OFFSET, ~UFSHC_DEV_CLK_EN);
    MmioAnd32 (BaseAddressAux + UFSHC_AUX_UFSHC_DEV_CTRL_OFFSET, ~UFSHC_DEV_RESET);
    MmioOr32 (BaseAddressAux + UFSHC_AUX_UFSHC_DEV_CTRL_OFFSET, UFSHC_DEV_CLK_EN);
    MmioOr32 (BaseAddressAux + UFSHC_AUX_UFSHC_DEV_CTRL_OFFSET, UFSHC_DEV_RESET);
    MmioWrite32 (BaseAddress + UFS_VNDR_HCLKDIV_1US_TICK_OFFSET, REG_UFS_VNDR_HCLKDIV);
    DeviceDiscoveryEnableClock (ControllerHandle, "mphy_force_ls_mode", FALSE);
    break;

  case EdkiiUfsHcPreLinkStartup:
    UfsDmeCmd (DriverInterface, UfsUicDmeSet, PA_LOCAL_TX_LCC_ENABLE, 0, NULL);
    UfsDmeCmd (DriverInterface, UfsUicDmeGet, VS_TXBURSTCLOSUREDELAY, 0, &TxBurstClosureDelay);
    UfsDmeCmd (DriverInterface, UfsUicDmeSet, VS_TXBURSTCLOSUREDELAY, 0, NULL);
    break;

  case EdkiiUfsHcPostLinkStartup:
    UfsDmeCmd (DriverInterface, UfsUicDmeSet, T_CONNECTIONSTATE, 1, NULL);
    UfsDmeCmd (DriverInterface, UfsUicDmeSet, PA_TX_HS_G1_SYNC_LENGTH, 0x4f, NULL);
    UfsDmeCmd (DriverInterface, UfsUicDmeSet, PA_TX_HS_G2_SYNC_LENGTH, 0x4f, NULL);
    UfsDmeCmd (DriverInterface, UfsUicDmeSet, PA_TX_HS_G3_SYNC_LENGTH, 0x4f, NULL);

    UfsDmeCmd(DriverInterface, UfsUicDmeSet, DME_FC0PROTECTIONTIMEOUTVAL, 0x1fff, NULL);
    UfsDmeCmd(DriverInterface, UfsUicDmeSet, DME_TC0REPLAYTIMEOUTVAL, 0xffff, NULL);
    UfsDmeCmd(DriverInterface, UfsUicDmeSet, DME_AFC0REQTIMEOUTVAL, 0x7fff, NULL);

    UfsDmeCmd(DriverInterface, UfsUicDmeSet, PWR_MODE_USER_DATA0, 0x1fff, NULL);
    UfsDmeCmd(DriverInterface, UfsUicDmeSet, PWR_MODE_USER_DATA1, 0xffff, NULL);
    UfsDmeCmd(DriverInterface, UfsUicDmeSet, PWR_MODE_USER_DATA2, 0x7fff, NULL);

    UfsDmeCmd (DriverInterface, UfsUicDmeSet, VS_TXBURSTCLOSUREDELAY, TxBurstClosureDelay, NULL);

    Status = UfsDmeCmd (DriverInterface, UfsUicDmeGet, PA_CONNECTED_TX_DATA_LANES, 0, &Value);
    if (!EFI_ERROR (Status)) {
      UfsDmeCmd (DriverInterface, UfsUicDmeSet, PA_ACTIVE_TX_DATA_LANES, Value, NULL);
    }

    Status = UfsDmeCmd (DriverInterface, UfsUicDmeGet, PA_CONNECTED_RX_DATA_LANES, 0, &Value);
    if (!EFI_ERROR (Status)) {
      UfsDmeCmd (DriverInterface, UfsUicDmeSet, PA_ACTIVE_RX_DATA_LANES, Value, NULL);
    }

    UfsDmeCmd (DriverInterface, UfsUicDmeGet, VS_DEBUGSAVECONFIGTIME, 0, &Value);
    Value &= ~(SET_TREF(~0UL));
    Value |= SET_TREF(VS_DEBUGSAVECONFIGTIME_TREF);
    Value &= ~(SET_ST_SCT(~0UL));
    Value |= SET_TREF(VS_DEBUGSAVECONFIGTIME_ST_SCT);
    UfsDmeCmd (DriverInterface, UfsUicDmeSet, VS_DEBUGSAVECONFIGTIME, Value, NULL);

    Status = UfsDmeCmd (DriverInterface, UfsUicDmeGet, PA_MAXRXHSGEAR, 0, &Value);
    if (!EFI_ERROR (Status)) {
      UfsDmeCmd (DriverInterface, UfsUicDmeSet, PA_RX_GEAR, Value, NULL);
    }

    Status = UfsDmeCmd (DriverInterface, UfsUicDmePeerGet, PA_MAXRXHSGEAR, 0, &Value);
    if (!EFI_ERROR (Status)) {
      UfsDmeCmd (DriverInterface, UfsUicDmeSet, PA_TX_GEAR, Value, NULL);
    } else {
      Status = UfsDmeCmd (DriverInterface, UfsUicDmeGet, PA_MAXRXHSGEAR, 0, &Value);
      if (!EFI_ERROR (Status)) {
        UfsDmeCmd (DriverInterface, UfsUicDmeSet, PA_TX_GEAR, Value, NULL);
      }
    }

    UfsDmeCmd (DriverInterface, UfsUicDmeSet, PA_RX_TERMINATION, 1, NULL);
    UfsDmeCmd (DriverInterface, UfsUicDmeSet, PA_TX_TERMINATION, 1, NULL);

    UfsDmeCmd (DriverInterface, UfsUicDmeGet, PA_HS_SERIES, 0, &Value);
    Value = UFS_HS_RATE_A;
    UfsDmeCmd (DriverInterface, UfsUicDmeSet, PA_HS_SERIES, Value, NULL);

    UfsDmeCmd (DriverInterface, UfsUicDmeSet, PA_PWR_MODE, ((PWRMODE_FAST_MODE << 4) | PWRMODE_FAST_MODE), NULL);
    break;

  default:
    return EFI_SUCCESS;
  }

  return EFI_SUCCESS;
}

EDKII_UFS_HC_PLATFORM_PROTOCOL gUfsOverride = {
  EDKII_UFS_HC_PLATFORM_PROTOCOL_VERSION,
  NULL,
  UfsCallback
};

/**
  Callback that will be invoked at various phases of the driver initialization

  This function allows for modification of system behavior at various points in
  the driver binding process.

  @param[in] Phase                    Current phase of the driver initialization
  @param[in] DriverHandle             Handle of the driver.
  @param[in] ControllerHandle         Handle of the controller.
  @param[in] DeviceTreeNode           Pointer to the device tree node protocol is available.

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
  EFI_STATUS              Status;
  UINTN                   RegionCount;
  NON_DISCOVERABLE_DEVICE *Device;
  EFI_PHYSICAL_ADDRESS    BaseAddress;
  EFI_PHYSICAL_ADDRESS    BaseAddressAux;
  UINTN                   Size;

  switch (Phase) {
  case DeviceDiscoveryDriverStart:
    return gBS->InstallMultipleProtocolInterfaces (
                  &DriverHandle,
                  &gEdkiiUfsHcPlatformProtocolGuid,
                  &gUfsOverride,
                  NULL
                  );
  case DeviceDiscoveryDriverBindingSupported:
    Status = DeviceDiscoveryGetMmioRegionCount (ControllerHandle, &RegionCount);
    if (EFI_ERROR (Status) || (RegionCount < 2)) {
      return EFI_UNSUPPORTED;
    }
    break;

  case DeviceDiscoveryDriverBindingStart:
    Status = gBS->HandleProtocol(ControllerHandle,
                                 &gNVIDIANonDiscoverableDeviceProtocolGuid,
                                 (VOID **)&Device);
    if (EFI_ERROR(Status)) {
      DEBUG((DEBUG_ERROR, "%a: Unable to locate non discoverable device\n", __FUNCTION__));
      return Status;
    }

    Device->DmaType = NonDiscoverableDeviceDmaTypeNonCoherent;

    Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress, &Size);
    if (EFI_ERROR (Status)) {
      DEBUG((DEBUG_ERROR, "%a: Base region not correct\n", __FUNCTION__));
      return EFI_UNSUPPORTED;
    }

    Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 1, &BaseAddressAux, &Size);
    if (EFI_ERROR (Status)) {
      DEBUG((DEBUG_ERROR, "%a: Aux region not correct\n", __FUNCTION__));
      return EFI_UNSUPPORTED;
    }

    Status = DeviceDiscoveryEnableClock (ControllerHandle, "mphy_force_ls_mode", TRUE);
    if (!EFI_ERROR (Status)) {
      MicroSecondDelay (1000);
      DeviceDiscoveryEnableClock (ControllerHandle, "mphy_force_ls_mode", FALSE);
    }

    MmioAnd32 (BaseAddressAux + UFSHC_AUX_UFSHC_DEV_CTRL_OFFSET, ~UFSHC_DEV_CLK_EN);
    MmioAnd32 (BaseAddressAux + UFSHC_AUX_UFSHC_DEV_CTRL_OFFSET, ~UFSHC_DEV_RESET);
    MmioOr32 (BaseAddressAux + UFSHC_AUX_UFSHC_DEV_CTRL_OFFSET, UFSHC_DEV_CLK_EN);
    MmioOr32 (BaseAddressAux + UFSHC_AUX_UFSHC_DEV_CTRL_OFFSET, UFSHC_DEV_RESET);
    MmioWrite32 (BaseAddress + UFS_VNDR_HCLKDIV_1US_TICK_OFFSET, REG_UFS_VNDR_HCLKDIV);
    break;

  default:
    return EFI_SUCCESS;
  }

  return EFI_SUCCESS;
}
