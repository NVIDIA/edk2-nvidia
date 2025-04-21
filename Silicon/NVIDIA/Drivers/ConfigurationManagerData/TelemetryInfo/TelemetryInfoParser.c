/** @file
  Patches the DSDT with Telemetry info

  SPDX-FileCopyrightText: Copyright (c) 2019-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2017 - 2018, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "TelemetryInfoParser.h"
#include "../ConfigurationManagerDataRepoLib.h"

#include <Library/ConfigurationManagerDataLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/FloorSweepingLib.h>
#include <Library/HobLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/UefiBootServicesTableLib.h>

#include <Protocol/BpmpIpc.h>
#include <Protocol/GpuDsdAmlGenerationProtocol.h>

#include <TH500/TH500Definitions.h>

// ACPI Timer enable
#define ACPI_TIMER_INSTRUCTION_ENABLE  (PcdGet8 (PcdAcpiTimerEnabled))

/** MRQ_PWR_LIMIT get sub-command (CMD_PWR_LIMIT_GET) packet
*/
#pragma pack (1)
typedef struct {
  UINT32    Command;
  UINT32    LimitId;
  UINT32    LimitSrc;
  UINT32    LimitType;
} MRQ_PWR_LIMIT_COMMAND_PACKET;
#pragma pack ()

/** patch _STA to enable/disable power meter device

  @param[in]     SocketId                Socket Id
  @param[in]     TelemetryDataBuffAddr   Telemetry Data Buff Addr

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
UpdatePowerMeterStaInfo (
  IN  NVIDIA_AML_PATCH_PROTOCOL  *PatchProtocol,
  IN  UINT32                     SocketId,
  IN  UINT64                     TelemetryDataBuffAddr
  )
{
  EFI_STATUS            Status;
  NVIDIA_AML_NODE_INFO  AcpiNodeInfo;
  UINT32                Index;
  UINT32                TelLayoutValidFlags0;
  UINT32                TelLayoutValidFlags1;
  UINT32                TelLayoutValidFlags2;
  UINT32                PwrMeterIndex;
  UINT8                 PwrMeterStatus;
  UINT32                *TelemetryData;
  BOOLEAN               GpuPresent;
  VOID                  *NVIDIAGpuDSDAMLGenerationProtocol;

  STATIC CHAR8 *CONST  AcpiPwrMeterStaPatchName[] = {
    "_SB_.PM00._STA",
    "_SB_.PM01._STA",
    "_SB_.PM02._STA",
    "_SB_.PM03._STA",
    "_SB_.PM10._STA",
    "_SB_.PM11._STA",
    "_SB_.PM12._STA",
    "_SB_.PM13._STA",
    "_SB_.PM20._STA",
    "_SB_.PM21._STA",
    "_SB_.PM22._STA",
    "_SB_.PM23._STA",
    "_SB_.PM30._STA",
    "_SB_.PM31._STA",
    "_SB_.PM32._STA",
    "_SB_.PM33._STA",
  };

  Status               = EFI_SUCCESS;
  TelemetryData        = NULL;
  TelemetryData        = (UINT32 *)TelemetryDataBuffAddr;
  TelLayoutValidFlags0 = TelemetryData[TH500_TEL_LAYOUT_VALID_FLAGS0_IDX];
  TelLayoutValidFlags1 = TelemetryData[TH500_TEL_LAYOUT_VALID_FLAGS1_IDX];
  TelLayoutValidFlags2 = TelemetryData[TH500_TEL_LAYOUT_VALID_FLAGS2_IDX];
  GpuPresent           = FALSE;

  if (SocketId >= ((ARRAY_SIZE (AcpiPwrMeterStaPatchName)) / TH500_MAX_PWR_METER)) {
    DEBUG ((DEBUG_ERROR, "%a: Index %u exceeding AcpiPwrMeterStaPatchName size\r\n", __FUNCTION__, SocketId));
    Status = EFI_INVALID_PARAMETER;
    goto ErrorExit;
  }

  Status = gBS->LocateProtocol (&gEfiNVIDIAGpuDSDAMLGenerationProtocolGuid, NULL, &NVIDIAGpuDSDAMLGenerationProtocol);
  if (EFI_ERROR (Status)) {
    GpuPresent = FALSE;
  } else {
    GpuPresent = TRUE;
  }

  if (GpuPresent) {
    TelLayoutValidFlags0 = TelLayoutValidFlags0 & ~TH500_MODULE_PWR_IDX_VALID_FLAG;
    TelLayoutValidFlags2 = TelLayoutValidFlags2 & ~TH500_MODULE_PWR_1SEC_IDX_VALID_FLAG;
  }

  for (Index = 0; Index < TH500_MAX_PWR_METER; Index++) {
    if ((TelLayoutValidFlags0 & (TH500_MODULE_PWR_IDX_VALID_FLAG << Index)) ||
        (TelLayoutValidFlags2 & (TH500_MODULE_PWR_1SEC_IDX_VALID_FLAG << Index)))
    {
      PwrMeterIndex = (SocketId * TH500_MAX_PWR_METER) + Index;
      Status        = PatchProtocol->FindNode (PatchProtocol, AcpiPwrMeterStaPatchName[PwrMeterIndex], &AcpiNodeInfo);
      if (EFI_ERROR (Status)) {
        DEBUG ((
          DEBUG_ERROR,
          "%a: Acpi pwr meter sta node is not found for patching %a - %r\r\n",
          __FUNCTION__,
          AcpiPwrMeterStaPatchName[PwrMeterIndex],
          Status
          ));
        Status = EFI_SUCCESS;
        goto ErrorExit;
      }

      PwrMeterStatus = 0xF;
      Status         = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &PwrMeterStatus, sizeof (PwrMeterStatus));
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\r\n", __FUNCTION__, AcpiPwrMeterStaPatchName[PwrMeterIndex], Status));
        Status = EFI_SUCCESS;
        goto ErrorExit;
      }
    }
  }

ErrorExit:
  return Status;
}

/** patch ACPI Timer operator enable/disable status from Nvidia boot configuration in DSDT.

  @param[in]     SocketId            Socket Id

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
UpdateAcpiTimerOprInfo (
  IN  NVIDIA_AML_PATCH_PROTOCOL  *PatchProtocol,
  IN  UINT32                     SocketId
  )
{
  EFI_STATUS            Status;
  NVIDIA_AML_NODE_INFO  AcpiNodeInfo;
  UINT8                 AcpiTimerEnableFlag;

  STATIC CHAR8 *CONST  AcpiTimerInstructionEnableVarName[] = {
    "_SB_.BPM0.TIME",
    "_SB_.BPM1.TIME",
    "_SB_.BPM2.TIME",
    "_SB_.BPM3.TIME",
  };

  AcpiTimerEnableFlag = ACPI_TIMER_INSTRUCTION_ENABLE;

  if (SocketId >= ARRAY_SIZE (AcpiTimerInstructionEnableVarName)) {
    DEBUG ((DEBUG_ERROR, "%a: Index %u exceeding AcpiTimerInstructionEnableVarName size\r\n", __FUNCTION__, SocketId));
    Status = EFI_INVALID_PARAMETER;
    goto ErrorExit;
  }

  Status = PatchProtocol->FindNode (PatchProtocol, AcpiTimerInstructionEnableVarName[SocketId], &AcpiNodeInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Acpi timer enable node is not found for patching %a - %r\r\n", __FUNCTION__, AcpiTimerInstructionEnableVarName[SocketId], Status));
    Status = EFI_SUCCESS;
    goto ErrorExit;
  }

  Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &AcpiTimerEnableFlag, sizeof (AcpiTimerEnableFlag));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\r\n", __FUNCTION__, AcpiTimerInstructionEnableVarName[SocketId], Status));
    Status = EFI_SUCCESS;
    goto ErrorExit;
  }

ErrorExit:
  return Status;
}

/** patch MRQ_PWR_LIMIT data in DSDT.

  @param[in]     BpmpIpcProtocol     The instance of the NVIDIA_BPMP_IPC_PROTOCOL
  @param[in]     BpmpHandle          Bpmp handle
  @param[in]     SocketId            Socket Id

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
UpdatePowerLimitInfo (
  IN  NVIDIA_AML_PATCH_PROTOCOL  *PatchProtocol,
  IN  NVIDIA_BPMP_IPC_PROTOCOL   *BpmpIpcProtocol,
  IN  UINT32                     BpmpHandle,
  IN  UINT32                     SocketId
  )
{
  EFI_STATUS                    Status;
  NVIDIA_AML_NODE_INFO          AcpiNodeInfo;
  MRQ_PWR_LIMIT_COMMAND_PACKET  Request;
  UINT32                        PwrLimit;

  STATIC CHAR8 *CONST  AcpiMrqPwrLimitMinPatchName[] = {
    "_SB_.PM01.MINP",
    "_SB_.PM11.MINP",
    "_SB_.PM21.MINP",
    "_SB_.PM31.MINP",
  };

  STATIC CHAR8 *CONST  AcpiMrqPwrLimitMaxPatchName[] = {
    "_SB_.PM01.MAXP",
    "_SB_.PM11.MAXP",
    "_SB_.PM21.MAXP",
    "_SB_.PM31.MAXP",
  };

  // Get power meter limits
  Request.Command   = TH500_PWR_LIMIT_GET;
  Request.LimitId   = TH500_PWR_LIMIT_ID_TH500_INP_EDPC_MW;
  Request.LimitSrc  = TH500_PWR_LIMIT_SRC_INB;
  Request.LimitType = TH500_PWR_LIMIT_TYPE_BOUND_MAX;

  Status = BpmpIpcProtocol->Communicate (
                              BpmpIpcProtocol,
                              NULL,
                              BpmpHandle,
                              MRQ_PWR_LIMIT,
                              (VOID *)&Request,
                              sizeof (MRQ_PWR_LIMIT_COMMAND_PACKET),
                              (VOID *)&PwrLimit,
                              sizeof (UINT32),
                              NULL
                              );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error in BPMP communication for max pwr limit: %r\r\n", __FUNCTION__, Status));
    Status = EFI_SUCCESS;
    goto ErrorExit;
  }

  if (PwrLimit == 0) {
    PwrLimit = 0xFFFFFFFF;
  }

  if (SocketId >= ARRAY_SIZE (AcpiMrqPwrLimitMaxPatchName)) {
    DEBUG ((DEBUG_ERROR, "%a: SocketId %u exceeding AcpiMrqPwrLimitMaxPatchName size\r\n", __FUNCTION__, SocketId));
    goto ErrorExit;
  }

  Status = PatchProtocol->FindNode (PatchProtocol, AcpiMrqPwrLimitMaxPatchName[SocketId], &AcpiNodeInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Max power limit node is not found for patching %a - %r\r\n",
      __FUNCTION__,
      AcpiMrqPwrLimitMaxPatchName[SocketId],
      Status
      ));
    Status = EFI_SUCCESS;
    goto ErrorExit;
  }

  Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &PwrLimit, sizeof (PwrLimit));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\r\n", __FUNCTION__, AcpiMrqPwrLimitMaxPatchName[SocketId], Status));
    Status = EFI_SUCCESS;
    goto ErrorExit;
  }

  Request.LimitType = TH500_PWR_LIMIT_TYPE_BOUND_MIN;

  Status = BpmpIpcProtocol->Communicate (
                              BpmpIpcProtocol,
                              NULL,
                              BpmpHandle,
                              MRQ_PWR_LIMIT,
                              (VOID *)&Request,
                              sizeof (MRQ_PWR_LIMIT_COMMAND_PACKET),
                              (VOID *)&PwrLimit,
                              sizeof (UINT32),
                              NULL
                              );

  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error in BPMP communication for min pwr limit: %r\r\n", __FUNCTION__, Status));
    Status = EFI_SUCCESS;
    goto ErrorExit;
  }

  if (SocketId >= ARRAY_SIZE (AcpiMrqPwrLimitMinPatchName)) {
    DEBUG ((DEBUG_ERROR, "%a: SocketId %u exceeding AcpiMrqPwrLimitMinPatchName size\r\n", __FUNCTION__, SocketId));
    goto ErrorExit;
  }

  Status = PatchProtocol->FindNode (PatchProtocol, AcpiMrqPwrLimitMinPatchName[SocketId], &AcpiNodeInfo);
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Min power limit node is not found for patching %a - %r\r\n",
      __FUNCTION__,
      AcpiMrqPwrLimitMinPatchName[SocketId],
      Status
      ));
    Status = EFI_SUCCESS;
    goto ErrorExit;
  }

  Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &PwrLimit, sizeof (PwrLimit));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\r\n", __FUNCTION__, AcpiMrqPwrLimitMinPatchName[SocketId], Status));
    Status = EFI_SUCCESS;
    goto ErrorExit;
  }

ErrorExit:
  return Status;
}

/** Get the Dram speed from the telemtry data and update the dram info in the
    PlatformResourceData Hob.

  @param[in]     SocketId                Socket Id
  @param[in]     TelemetryDataBuffAddr   Telemetry Data Buff Addr

  @retval EFI_SUCCESS   Success

**/
STATIC
EFI_STATUS
EFIAPI
UpdateDramSpeed (
  IN  UINT32  SocketId,
  IN  UINT64  TelemetryDataBuffAddr
  )
{
  TEGRA_DRAM_DEVICE_INFO  *DramInfo;
  VOID                    *Hob;
  UINT32                  *TelemetryData;

  TelemetryData = NULL;
  TelemetryData = (UINT32 *)TelemetryDataBuffAddr;
  Hob           = GetFirstGuidHob (&gNVIDIAPlatformResourceDataGuid);
  if ((Hob != NULL) &&
      (GET_GUID_HOB_DATA_SIZE (Hob) == sizeof (TEGRA_PLATFORM_RESOURCE_INFO)))
  {
    DramInfo                                           = ((TEGRA_PLATFORM_RESOURCE_INFO *)GET_GUID_HOB_DATA (Hob))->DramDeviceInfo;
    DramInfo[SocketId * MAX_DIMMS_PER_SOCKET].SpeedKhz = TelemetryData[TH500_TEL_LAYOUT_DRAM_RATE_IDX];
    DEBUG ((DEBUG_INFO, "Setting Dram Speed to %u for Socket %u\n", DramInfo[SocketId * MAX_DIMMS_PER_SOCKET].SpeedKhz, SocketId));
  } else {
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

STATIC CONST CHAR8  *TelemetryCompatibleInfo[] = {
  "nvidia,th500-mrqtelemetry",
  NULL
};

/** DSDT patcher for Telemetry info.

  The DSDT table is potentially patched with the following information:
    "_SB_.BPM0.TBUF",
    "_SB_.BPM1.TBUF",
    "_SB_.BPM2.TBUF",
    "_SB_.BPM3.TBUF",
    "_SB_.PM00._STA",
    "_SB_.PM01._STA",
    "_SB_.PM02._STA",
    "_SB_.PM03._STA",
    "_SB_.PM10._STA",
    "_SB_.PM11._STA",
    "_SB_.PM12._STA",
    "_SB_.PM13._STA",
    "_SB_.PM20._STA",
    "_SB_.PM21._STA",
    "_SB_.PM22._STA",
    "_SB_.PM23._STA",
    "_SB_.PM30._STA",
    "_SB_.PM31._STA",
    "_SB_.PM32._STA",
    "_SB_.PM33._STA",
    "_SB_.BPM0.TIME",
    "_SB_.BPM1.TIME",
    "_SB_.BPM2.TIME",
    "_SB_.BPM3.TIME",
    "_SB_.PM01.MINP",
    "_SB_.PM11.MINP",
    "_SB_.PM21.MINP",
    "_SB_.PM31.MINP",
    "_SB_.PM01.MAXP",
    "_SB_.PM11.MAXP",
    "_SB_.PM21.MAXP",
    "_SB_.PM31.MAXP",

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle    A handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
TelemetryInfoParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                 Status;
  NVIDIA_AML_PATCH_PROTOCOL  *PatchProtocol;
  NVIDIA_AML_NODE_INFO       AcpiNodeInfo;
  NVIDIA_BPMP_IPC_PROTOCOL   *BpmpIpcProtocol;
  UINT64                     TelemetryDataBuffAddr;
  UINTN                      ResponseSize;
  UINT32                     BpmpHandle;
  VOID                       *Dtb;
  INT32                      NodeOffset;
  CONST VOID                 *Property;
  INT32                      PropertySize;
  UINT32                     SocketId;

  STATIC CHAR8 *CONST  AcpiMrqTelemetryBufferPatchName[] = {
    "_SB_.BPM0.TBUF",
    "_SB_.BPM1.TBUF",
    "_SB_.BPM2.TBUF",
    "_SB_.BPM3.TBUF",
  };

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  BpmpIpcProtocol       = NULL;
  TelemetryDataBuffAddr = 0;
  ResponseSize          = sizeof (TelemetryDataBuffAddr);
  Dtb                   = NULL;
  Property              = NULL;
  PropertySize          = 0;
  SocketId              = 0;

  Status = NvGetCmPatchProtocol (ParserHandle, &PatchProtocol);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  NodeOffset = -1;
  Status     = DeviceTreeGetNextCompatibleNode (TelemetryCompatibleInfo, &NodeOffset);
  if (Status == EFI_NOT_FOUND) {
    DEBUG ((DEBUG_ERROR, "%a: %a nodes absent in device tree\r\n", __FUNCTION__, TelemetryCompatibleInfo[0]));
    Status = EFI_SUCCESS;
    goto ErrorExit;
  } else if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get Telemetry nodes\n", __FUNCTION__, Status));
    goto ErrorExit;
  }

  Status = gBS->LocateProtocol (&gNVIDIABpmpIpcProtocolGuid, NULL, (VOID **)&BpmpIpcProtocol);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to locate BpmpIpcProtocol\n", __FUNCTION__, Status));
    Status = EFI_NOT_READY;
    goto ErrorExit;
  }

  if (BpmpIpcProtocol == NULL) {
    Status = EFI_INVALID_PARAMETER;
    goto ErrorExit;
  }

  while (NodeOffset > 0) {
    BpmpHandle = 0;

    Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "nvidia,bpmp", &BpmpHandle);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get Bpmp node phandle (%r)\n", __FUNCTION__, Status));
      goto ErrorExit;
    }

    SocketId = -1;
    Status   = DeviceTreeGetNodePropertyValue32 (NodeOffset, "nvidia,hw-instance-id", &SocketId);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get Socket Id (%r)\n", __FUNCTION__, Status));
      goto ErrorExit;
    }

    if (SocketId >= PcdGet32 (PcdTegraMaxSockets)) {
      DEBUG ((DEBUG_ERROR, "%a: SocketId %u exceeds number of sockets\r\n", __FUNCTION__, SocketId));
      Status = EFI_SUCCESS;
      goto ErrorExit;
    }

    if (!IsSocketEnabled (SocketId)) {
      continue;
    }

    if (SocketId >= ARRAY_SIZE (AcpiMrqTelemetryBufferPatchName)) {
      DEBUG ((DEBUG_ERROR, "%a: SocketId %u exceeding AcpiMrqTelemetryBufferPatchName size\r\n", __FUNCTION__, SocketId));
      Status = EFI_SUCCESS;
      goto ErrorExit;
    }

    Status = BpmpIpcProtocol->Communicate (
                                BpmpIpcProtocol,
                                NULL,
                                BpmpHandle,
                                MRQ_TELEMETRY,
                                NULL,
                                0,
                                (VOID *)&TelemetryDataBuffAddr,
                                ResponseSize,
                                NULL
                                );

    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Error in BPMP communication: %r\r\n", __FUNCTION__, Status));
      Status = EFI_SUCCESS;
      goto ErrorExit;
    }

    TelemetryDataBuffAddr = TH500_AMAP_GET_ADD (TelemetryDataBuffAddr, SocketId);

    Status = PatchProtocol->FindNode (PatchProtocol, AcpiMrqTelemetryBufferPatchName[SocketId], &AcpiNodeInfo);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: MRQ_TELEMETRY node is not found for patching %a - %r\r\n", __FUNCTION__, AcpiMrqTelemetryBufferPatchName[SocketId], Status));
      Status = EFI_SUCCESS;
      goto ErrorExit;
    }

    Status = PatchProtocol->SetNodeData (PatchProtocol, &AcpiNodeInfo, &TelemetryDataBuffAddr, sizeof (TelemetryDataBuffAddr));
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Error updating %a - %r\r\n", __FUNCTION__, AcpiMrqTelemetryBufferPatchName[SocketId], Status));
      Status = EFI_SUCCESS;
      goto ErrorExit;
    }

    Status = UpdatePowerMeterStaInfo (PatchProtocol, SocketId, TelemetryDataBuffAddr);
    if (EFI_ERROR (Status)) {
      Status = EFI_SUCCESS;
      goto ErrorExit;
    }

    Status = UpdateAcpiTimerOprInfo (PatchProtocol, SocketId);
    if (EFI_ERROR (Status)) {
      Status = EFI_SUCCESS;
      goto ErrorExit;
    }

    Status = UpdatePowerLimitInfo (PatchProtocol, BpmpIpcProtocol, BpmpHandle, SocketId);
    if (EFI_ERROR (Status)) {
      Status = EFI_SUCCESS;
      goto ErrorExit;
    }

    Status = UpdateDramSpeed (SocketId, TelemetryDataBuffAddr);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to update Dram speed %r\n", __FUNCTION__, Status));
      Status = EFI_SUCCESS;
    }

    Status = DeviceTreeGetNextCompatibleNode (TelemetryCompatibleInfo, &NodeOffset);
    if (Status == EFI_NOT_FOUND) {
      Status = EFI_SUCCESS;
      break;
    } else if (EFI_ERROR (Status)) {
      goto ErrorExit;
    }
  }

ErrorExit:
  return Status;
}

REGISTER_PARSER_FUNCTION (TelemetryInfoParser, NULL)
