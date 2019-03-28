/** @file

  SE RNG Controller Driver

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
#include <Library/TimerLib.h>
#include <libfdt.h>
#include <Library/CacheMaintenanceLib.h>
#include <Protocol/NonDiscoverableDevice.h>

#include "SeRngPrivate.h"

NVIDIA_COMPATIBILITY_MAPPING gDeviceCompatibilityMap[] = {
    { "nvidia,tegra234-se", &gNVIDIANonDiscoverableT234SeDeviceGuid },
    { "nvidia,tegra194-se-elp", &gNVIDIANonDiscoverableT194SeDeviceGuid },
    { NULL, NULL }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG gDeviceDiscoverDriverConfig = {
    .DriverName = L"NVIDIA SE RNG controller driver",
    .UseDriverBinding = TRUE,
    .AutoEnableClocks = TRUE,
    .AutoDeassertReset = TRUE,
    .SkipEdkiiNondiscoverableInstall = TRUE
};

STATIC
EFI_STATUS
ExecuteRng1ControlCommand(
    IN SE_RNG_PRIVATE_DATA *Private,
    IN UINT32              Command
    )
{
  UINT32  MaxPollCount = RNG1_TIMEOUT;
  BOOLEAN SecureMode;
  UINT32  Data32;
  UINT32  ExpectedStatus;

  MmioWrite32 (Private->BaseAddress + TEGRA_SE_RNG1_INT_EN_OFFSET, MAX_UINT32);
  MmioWrite32 (Private->BaseAddress + TEGRA_SE_RNG1_IE_OFFSET, MAX_UINT32);

  Data32 = MmioRead32 (Private->BaseAddress + TEGRA_SE_RNG1_STATUS_OFFSET);
  if ((Data32 & TEGRA_SE_RNG1_STATUS_SECURE) != 0) {
    SecureMode = TRUE;
  } else {
    SecureMode = FALSE;
  }

  switch (Command) {
    case RNG1_CMD_GEN_NONCE:
    case RNG1_CMD_CREATE_STATE:
    case RNG1_CMD_RENEW_STATE:
    case RNG1_CMD_REFRESH_ADDIN:
    case RNG1_CMD_GEN_RANDOM:
    case RNG1_CMD_ADVANCE_STATE:
      ExpectedStatus = TEGRA_SE_RNG1_ISTATUS_DONE;
      break;
    case RNG1_CMD_GEN_NOISE:
      if (SecureMode) {
        ExpectedStatus = TEGRA_SE_RNG1_ISTATUS_DONE;
      } else {
        ExpectedStatus = TEGRA_SE_RNG1_ISTATUS_DONE | TEGRA_SE_RNG1_ISTATUS_NOISE_RDY;
      }
      break;
    case RNG1_CMD_KAT:
      ExpectedStatus = TEGRA_SE_RNG1_ISTATUS_KAT_COMPLETED;
      break;
    case RNG1_CMD_ZEROIZE:
      ExpectedStatus = TEGRA_SE_RNG1_ISTATUS_ZEROIZED;
      break;
    case RNG1_CMD_NOP:
    default:
      DEBUG ((DEBUG_ERROR, "Cmd %d has nothing to do (or) invalid\r\n", Command));
      return EFI_DEVICE_ERROR;
  }
  MmioWrite32 (Private->BaseAddress + TEGRA_SE_RNG1_CTRL_OFFSET, Command);

  MaxPollCount = RNG1_TIMEOUT;
  do {
    if (MaxPollCount == 0) {
      DEBUG ((DEBUG_ERROR, "RNG1 ISTAT poll timed out\r\n"));
      DEBUG ((DEBUG_ERROR, "Command %d\r\n", Command));
      return EFI_DEVICE_ERROR;
    }
    MicroSecondDelay(1);
    Data32 = MmioRead32 (Private->BaseAddress + TEGRA_SE_RNG1_ISTATUS_OFFSET);
    MaxPollCount--;
  } while (Data32 != ExpectedStatus);

  Data32 = MmioRead32 (Private->BaseAddress + TEGRA_SE_RNG1_IE_OFFSET);
  Data32 = MmioRead32 (Private->BaseAddress + TEGRA_SE_RNG1_INT_EN_OFFSET);

  MaxPollCount = RNG1_TIMEOUT;
  do {
    if (MaxPollCount == 0) {
      break;
    }
    MicroSecondDelay(1);
    Data32 = MmioRead32 (Private->BaseAddress + TEGRA_SE_RNG1_INT_STATUS_OFFSET);
    MaxPollCount--;
  } while ((Data32 & TEGRA_SE_RNG1_INT_STATUS_EIP0) != 0);

  MmioWrite32 (Private->BaseAddress + TEGRA_SE_RNG1_ISTATUS_OFFSET, ExpectedStatus);

  Data32 = MmioRead32 (Private->BaseAddress + TEGRA_SE_RNG1_INT_STATUS_OFFSET);
  if ((Data32 & TEGRA_SE_RNG1_INT_STATUS_EIP0) != 0) {
    DEBUG ((DEBUG_ERROR, "RNG1 interupt not cleared (0x%x) after cmd %d execution\r\n",
      Data32, Command));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Gets 128-bits of random data from SE RNG1.

  @param[in]     This                The instance of the NVIDIA_SE_RNG_PROTOCOL.
  @param[in]     Buffer              Buffer to place data into

  @return EFI_SUCCESS               The data was returned.
  @return EFI_INVALID_PARAMETER     Buffer is NULL.
  @return EFI_DEVICE_ERROR          Failed to get random data.
**/
STATIC
EFI_STATUS
SeRngRng1GetRandom128 (
  IN  NVIDIA_SE_RNG_PROTOCOL   *This,
  IN  UINT64                   *Buffer
  )
{
  SE_RNG_PRIVATE_DATA *Private;
  EFI_STATUS          Status;
  UINT32              Data32;
  UINT32              MaxPollCount = RNG1_TIMEOUT;
  UINT32              *Buffer32 = (UINT32 *)Buffer;
  UINTN               Index;

  if ((This == NULL) ||
      (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = SE_RNG_PRIVATE_DATA_FROM_THIS (This);

  /*Wait until RNG is Idle */
  do {
     if (MaxPollCount == 0) {
      DEBUG ((DEBUG_ERROR, "RNG1 Idle timed out\r\n"));
      return EFI_DEVICE_ERROR;
    }
    MicroSecondDelay(1);
    Data32 = MmioRead32 (Private->BaseAddress + TEGRA_SE_RNG1_STATUS_OFFSET);
    MaxPollCount--;
  } while ((Data32 & TEGRA_SE_RNG1_STATUS_BUSY) != 0);

  Data32 = MmioRead32 (Private->BaseAddress + TEGRA_SE_RNG1_ISTATUS_OFFSET);
  MmioWrite32 (Private->BaseAddress + TEGRA_SE_RNG1_ISTATUS_OFFSET, Data32);

  Data32 = MmioRead32 (Private->BaseAddress + TEGRA_SE_RNG1_ISTATUS_OFFSET);
  if (Data32 != 0) {
    DEBUG ((DEBUG_ERROR, "RNG1_ISTATUS Reg is not cleared\r\n"));
    return EFI_DEVICE_ERROR;
  }

  /* need to write twice, switch secure/promiscuous
   * mode would reset other bits
   */
  MmioWrite32 (Private->BaseAddress + TEGRA_SE_RNG1_SE_SMODE_OFFSET, TEGRA_SE_RNG1_SE_SMODE_SECURE);
  MmioWrite32 (Private->BaseAddress + TEGRA_SE_RNG1_SE_SMODE_OFFSET, TEGRA_SE_RNG1_SE_SMODE_SECURE);
  MmioWrite32 (Private->BaseAddress + TEGRA_SE_RNG1_SE_MODE_OFFSET, RNG1_MODE_SEC_ALG);

  /* Generate Noise */
  Status = ExecuteRng1ControlCommand(Private, RNG1_CMD_GEN_NOISE);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }
  Status = ExecuteRng1ControlCommand(Private, RNG1_CMD_CREATE_STATE);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  Status = ExecuteRng1ControlCommand(Private, RNG1_CMD_GEN_RANDOM);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  for (Index = 0; Index < 4; Index++) {
    Buffer32[Index] = MmioRead32 (Private->BaseAddress + TEGRA_SE_RNG1_RAND0_OFFSET + Index * sizeof(UINT32));
    if (Buffer32[Index] == 0) {
      DEBUG ((DEBUG_ERROR, "No random data from RAND\r\n"));
      return EFI_DEVICE_ERROR;
    }
  }

  Status = ExecuteRng1ControlCommand(Private, RNG1_CMD_ADVANCE_STATE);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }

  //Check RNG1 alarm
  Data32 = MmioRead32 (Private->BaseAddress + TEGRA_SE_RNG1_ALARMS_OFFSET);
  if (Data32 != 0) {
    DEBUG ((DEBUG_ERROR, "RNG1 Alarms not cleared (0x%x)\r\n", Data32));
    return EFI_DEVICE_ERROR;
  }

  Status = ExecuteRng1ControlCommand(Private, RNG1_CMD_ZEROIZE);
  if (EFI_ERROR (Status)) {
    return EFI_DEVICE_ERROR;
  }
  return EFI_SUCCESS;
}

/**
  Gets 128-bits of random data from SE.

  @param[in]     This                The instance of the NVIDIA_SE_RNG_PROTOCOL.
  @param[in]     Buffer              Buffer to place data into

  @return EFI_SUCCESS               The data was returned.
  @return EFI_INVALID_PARAMETER     Buffer is NULL.
  @return EFI_DEVICE_ERROR          Failed to get random data.
**/
STATIC
EFI_STATUS
SeRngGetRandom128 (
  IN  NVIDIA_SE_RNG_PROTOCOL   *This,
  IN  UINT64                   *Buffer
  )
{
  SE_RNG_PRIVATE_DATA *Private;
  EFI_STATUS          Status;
  UINT32              UpperAddress;
  UINT32              MaxPollCount = SE_MAX_POLL_COUNT;
  UINT32              AesStatus;

  if ((This == NULL) ||
      (Buffer == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  Private = SE_RNG_PRIVATE_DATA_FROM_THIS (This);

  MmioWrite32 (Private->BaseAddress + SE0_AES0_CONFIG_0,
               SE0_AES0_CONFIG_0_DST_MEMORY |
               SE0_AES0_CONFIG_0_DEC_ALG_NOP |
               SE0_AES0_CONFIG_0_ENC_ALG_RNG |
               SE0_AES0_CONFIG_0_ENC_MODE__KEY256
              );

  MmioWrite32 (Private->BaseAddress + SE0_AES0_CRYPTO_CONFIG_0,
               SE0_AES0_CRYPTO_CONFIG_0_XOR_POS_BYPASS |
               SE0_AES0_CRYPTO_CONFIG_0_INPUT_SEL_RANDOM |
               SE0_AES0_CRYPTO_CONFIG_0_CORE_SEL_ENCRYPT |
               SE0_AES0_CRYPTO_CONFIG_0_HASH_ENB_DISABLE
              );
/*
  MmioWrite32 (Private->BaseAddress + SE0_AES0_RNG_CONFIG_0,
               SE0_AES0_RNG_CONFIG_0_SRC_ENTROPY |
               SE0_AES0_RNG_CONFIG_0_MODE_FORCE_RESEED
              );

  MmioWrite32 (Private->BaseAddress + SE0_AES0_RNG_RESEED_INTERVAL_0, RANDOM_BYTES);

*/
  WriteBackDataCacheRange (Buffer, RANDOM_BYTES);

  MmioWrite32 (Private->BaseAddress + SE0_AES0_OUT_ADDR_0, (UINT32)(UINTN)Buffer);

  UpperAddress = (((UINTN)(VOID *)Buffer >> 32) << SE0_AES0_OUT_ADDR_HI_0_MSB_SHIFT) & SE0_AES0_OUT_ADDR_HI_0_MSB_MASK;
  UpperAddress |= (RANDOM_BYTES << SE0_AES0_OUT_ADDR_HI_0_SZ_SHIFT) & SE0_AES0_OUT_ADDR_HI_0_SZ_MASK;
  MmioWrite32 (Private->BaseAddress + SE0_AES0_OUT_ADDR_HI_0, UpperAddress);

  //Always support 1 block
  MmioWrite32 (Private->BaseAddress + SE0_AES0_CRYPTO_LAST_BLOCK_0, 0);

  MmioWrite32 (Private->BaseAddress + SE0_AES0_OPERATION_0,
               SE0_AES0_OPERATION_0_LASTBUF_FIELD |
               SE_UNIT_OPERATION_PKT_OP_START );

  do {
    AesStatus = MmioRead32 (Private->BaseAddress + SE0_AES0_STATUS_0);
    MaxPollCount--;
  } while ((MaxPollCount > 0) && (AesStatus != 0));

  if (AesStatus != 0) {
    DEBUG ((EFI_D_ERROR, "%a, Timeout waiting for random\r\n", __FUNCTION__));
    Status = EFI_DEVICE_ERROR;
    goto ErrorExit;
  }

  InvalidateDataCacheRange (Buffer, RANDOM_BYTES);

  Status = EFI_SUCCESS;

ErrorExit:

  return Status;
}

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
  SE_RNG_PRIVATE_DATA     *Private;
  EFI_STATUS              Status;
  UINTN                   RegionSize;
  NON_DISCOVERABLE_DEVICE *Device;

  switch (Phase) {
  case DeviceDiscoveryDriverBindingStart:
    Status = gBS->HandleProtocol (
                    ControllerHandle,
                    &gNVIDIANonDiscoverableDeviceProtocolGuid,
                    (VOID **)&Device
                    );
    if (EFI_ERROR (Status)) {
      return Status;
    }

    Private = (SE_RNG_PRIVATE_DATA *)AllocateZeroPool (sizeof (SE_RNG_PRIVATE_DATA));
    if (Private == NULL) {
      Status = EFI_OUT_OF_RESOURCES;
      DEBUG ((EFI_D_ERROR, "SeRngDxe: Failed to allocate private data structure\r\n"));
      break;
    }

    Private->Signature = SE_RNG_SIGNATURE;
    if (CompareGuid (Device->Type, &gNVIDIANonDiscoverableT234SeDeviceGuid)) {
      Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &Private->BaseAddress, &RegionSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "SeRngDxe: Failed to get region location (%r)\r\n", Status));
        FreePool (Private);
        break;
      }

      Private->SeRngProtocol.GetRandom128 = SeRngGetRandom128;
    } else {
      Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 1, &Private->BaseAddress, &RegionSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((EFI_D_ERROR, "SeRngDxe: Failed to get region location (%r)\r\n", Status));
        FreePool (Private);
        break;
      }

      Private->SeRngProtocol.GetRandom128 = SeRngRng1GetRandom128;
    }

    Status = gBS->InstallMultipleProtocolInterfaces (&ControllerHandle,
                                                     &gEfiCallerIdGuid,
                                                     Private,
                                                     &gNVIDIASeRngProtocolGuid,
                                                     &Private->SeRngProtocol,
                                                     NULL
                                                     );
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "SeRngDxe: Failed to install protocol (%r)\r\n", Status));
      FreePool (Private);
      break;
    }
    break;

  case DeviceDiscoveryDriverBindingStop:
    Status = gBS->HandleProtocol (ControllerHandle, &gEfiCallerIdGuid, (VOID **)&Private);
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "SeRng: Failed to get private data (%r)\r\n", Status));
      break;
    }

    Status = gBS->UninstallMultipleProtocolInterfaces (ControllerHandle,
                                                       &gEfiCallerIdGuid,
                                                       Private,
                                                       &gNVIDIASeRngProtocolGuid,
                                                       &Private->SeRngProtocol,
                                                       NULL
                                                       );
    if (EFI_ERROR (Status)) {
      DEBUG ((EFI_D_ERROR, "SeRng: Failed to uninstall procotol (%r)\r\n", Status));
      break;
    }

    FreePool (Private);
    Status = EFI_SUCCESS;
    break;

  default:
    Status = EFI_SUCCESS;
    break;
  }
  return Status;
}
