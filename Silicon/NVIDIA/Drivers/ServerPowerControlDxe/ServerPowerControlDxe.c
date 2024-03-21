/** @file
 *  Server Power Control Dxe
 *
 *  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 *
 *  SPDX-License-Identifier: BSD-2-Clause-Patent
 *
 **/

#include <PiPei.h>
#include <PiDxe.h>

#include <Protocol/ServerPowerControl.h>
#include <Protocol/BpmpIpc.h>
#include <Library/DebugLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/DxeServicesTableLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/MemoryAllocationLib.h>
#include <libfdt.h>

#define BYPASS_EXIT   0
#define BYPASS_ENTER  1

#pragma pack (1)
typedef struct {
  UINT32    Command;
  UINT32    SubCommand;
} MRQ_PWR_CNTRL_ABI_PACKET;

typedef struct {
  UINT32    Command;
  UINT32    ControlId;
  UINT32    BypassStatus;
} MRQ_PWR_CNTRL_COMMAND_PACKET;
#pragma pack ()

typedef enum {
  CmdPwrCntrlQueryAbi,
  CmdPwrCntrlBypassSet,
  CmdPwrCntrlBypassGet,
  CmdPwrCntrlMax
} MRQ_PWR_CNTRL_COMMANDS;

typedef enum {
  PwrCntrlIdInpEdpc,
  PwrCntrlIdInpEdpp,
  PwrCntrlIdCpuOutEdpc,
  PwrCntrlIdInpEdpcEx1,
  PwrCntrlIdInpEdpcEx2,
  PwrCntrlIdMax
} MRQ_PWR_CNTRL_ID;

NVIDIA_SERVER_POWER_CONTROL_PROTOCOL  mServerPowerControlProtocol;

EFI_STATUS
EFIAPI
BpmpProcessPowerControlCommand (
  IN NVIDIA_BPMP_IPC_PROTOCOL  *BpmpIpc,
  IN UINT32                    BpmpPhandle,
  IN VOID                      *Request,
  IN UINTN                     RequestSize
  )
{
  EFI_STATUS  Status;

  if ((BpmpIpc == NULL) ||
      (Request == NULL) ||
      (RequestSize == 0) ||
      (BpmpPhandle == MAX_UINT32))
  {
    return EFI_INVALID_PARAMETER;
  }

  Status = BpmpIpc->Communicate (
                      BpmpIpc,
                      NULL,
                      BpmpPhandle,
                      MRQ_PWR_CNTRL,
                      Request,
                      RequestSize,
                      NULL,
                      0,
                      NULL
                      );
  if (Status == EFI_UNSUPPORTED) {
    Status = EFI_SUCCESS;
  } else if (EFI_ERROR (Status)) {
    Status = EFI_DEVICE_ERROR;
  }

  return Status;
}

EFI_STATUS
EFIAPI
ConfigurePowerControl (
  NVIDIA_SERVER_POWER_CONTROL_PROTOCOL  *This,
  NVIDIA_SERVER_POWER_CONTROL_SETTING   PowerControlSetting
  )
{
  EFI_STATUS                    Status;
  NVIDIA_BPMP_IPC_PROTOCOL      *BpmpIpc;
  UINT32                        NumPowerControlInstances;
  UINT32                        *PowerControlHandles;
  UINT32                        Count;
  VOID                          *Dtb;
  INT32                         NodeOffset;
  CONST VOID                    *Property;
  CONST UINT32                  *Data;
  INT32                         DataLen;
  UINT32                        BpmpPhandle;
  MRQ_PWR_CNTRL_COMMAND_PACKET  Request;

  if ((This == NULL) ||
      (PowerControlSetting >= ServerPowerControlInputPowerCappingMax))
  {
    return EFI_INVALID_PARAMETER;
  }

  BpmpIpc = NULL;
  Status  = gBS->LocateProtocol (
                   &gNVIDIABpmpIpcProtocolGuid,
                   NULL,
                   (VOID **)&BpmpIpc
                   );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  NumPowerControlInstances = 0;
  PowerControlHandles      = NULL;
  Status                   = GetMatchingEnabledDeviceTreeNodes ("nvidia,th500-powercontrol", NULL, &NumPowerControlInstances);
  if (Status == EFI_NOT_FOUND) {
    goto ErrorExit;
  } else if (Status == EFI_BUFFER_TOO_SMALL) {
    PowerControlHandles = (UINT32 *)AllocateZeroPool (sizeof (UINT32) * NumPowerControlInstances);
    if (PowerControlHandles == NULL) {
      DEBUG ((DEBUG_ERROR, "ERROR: Failed to allocate buffer for bpmp handles.\n"));
      Status = EFI_OUT_OF_RESOURCES;
      goto ErrorExit;
    }

    Status = GetMatchingEnabledDeviceTreeNodes ("nvidia,th500-powercontrol", PowerControlHandles, &NumPowerControlInstances);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "ERROR: Failed to get bpmp dtb node handles. Status = %r\n", Status));
      goto ErrorExit;
    }
  }

  for (Count = 0; Count < NumPowerControlInstances; Count++) {
    Status = GetDeviceTreeNode (PowerControlHandles[Count], &Dtb, &NodeOffset);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get power control dtb node information.\n", __func__));
      goto ErrorExit;
    }

    Property = NULL;
    Property = fdt_getprop (Dtb, NodeOffset, "bpmp", &DataLen);
    if ((Property == NULL) ||
        (DataLen != sizeof (UINT32)))
    {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get bpmp information from power control dtb node.\n", __func__));
      goto ErrorExit;
    }

    Data        = (CONST UINT32 *)Property;
    BpmpPhandle = SwapBytes32 (*Data);
    if (PowerControlSetting == ServerPowerControlInputPowerCapping50ms) {
      Request.Command      = CmdPwrCntrlBypassSet;
      Request.ControlId    = PwrCntrlIdInpEdpcEx1;
      Request.BypassStatus = BYPASS_ENTER;
      Status               = BpmpProcessPowerControlCommand (BpmpIpc, BpmpPhandle, &Request, sizeof (MRQ_PWR_CNTRL_COMMAND_PACKET));
      if (EFI_ERROR (Status)) {
        goto ErrorExit;
      }

      Request.Command      = CmdPwrCntrlBypassSet;
      Request.ControlId    = PwrCntrlIdInpEdpcEx2;
      Request.BypassStatus = BYPASS_ENTER;
      Status               = BpmpProcessPowerControlCommand (BpmpIpc, BpmpPhandle, &Request, sizeof (MRQ_PWR_CNTRL_COMMAND_PACKET));
      if (EFI_ERROR (Status)) {
        goto ErrorExit;
      }

      DEBUG ((DEBUG_ERROR, "%a: Input Power Capping Using 50ms Timescale.\r\n", __FUNCTION__));
    } else if (PowerControlSetting == ServerPowerControlInputPowerCapping1s) {
      Request.Command      = CmdPwrCntrlBypassSet;
      Request.ControlId    = PwrCntrlIdInpEdpcEx2;
      Request.BypassStatus = BYPASS_EXIT;
      Status               = BpmpProcessPowerControlCommand (BpmpIpc, BpmpPhandle, &Request, sizeof (MRQ_PWR_CNTRL_COMMAND_PACKET));
      if (EFI_ERROR (Status)) {
        goto ErrorExit;
      }

      DEBUG ((DEBUG_ERROR, "%a: Input Power Capping Using 1s Timescale.\r\n", __FUNCTION__));
    } else if (PowerControlSetting == ServerPowerControlInputPowerCapping5s) {
      Request.Command      = CmdPwrCntrlBypassSet;
      Request.ControlId    = PwrCntrlIdInpEdpcEx1;
      Request.BypassStatus = BYPASS_EXIT;
      Status               = BpmpProcessPowerControlCommand (BpmpIpc, BpmpPhandle, &Request, sizeof (MRQ_PWR_CNTRL_COMMAND_PACKET));
      if (EFI_ERROR (Status)) {
        goto ErrorExit;
      }

      DEBUG ((DEBUG_ERROR, "%a: Input Power Capping Using 5s Timescale.\r\n", __FUNCTION__));
    }
  }

ErrorExit:
  if (PowerControlHandles != NULL) {
    FreePool (PowerControlHandles);
  }

  return Status;
}

EFI_STATUS
EFIAPI
ServerPowerControlDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                Status;
  NVIDIA_BPMP_IPC_PROTOCOL  *BpmpIpc;
  UINT32                    NumPowerControlInstances;
  UINT32                    *PowerControlHandles;
  UINT32                    Count;
  VOID                      *Dtb;
  INT32                     NodeOffset;
  CONST VOID                *Property;
  CONST UINT32              *Data;
  INT32                     DataLen;
  UINT32                    BpmpPhandle;
  MRQ_PWR_CNTRL_ABI_PACKET  Request;

  BpmpIpc = NULL;
  Status  = gBS->LocateProtocol (
                   &gNVIDIABpmpIpcProtocolGuid,
                   NULL,
                   (VOID **)&BpmpIpc
                   );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  NumPowerControlInstances = 0;
  PowerControlHandles      = NULL;
  Status                   = GetMatchingEnabledDeviceTreeNodes ("nvidia,th500-powercontrol", NULL, &NumPowerControlInstances);
  if (Status == EFI_NOT_FOUND) {
    goto ErrorExit;
  } else if (Status == EFI_BUFFER_TOO_SMALL) {
    PowerControlHandles = (UINT32 *)AllocateZeroPool (sizeof (UINT32) * NumPowerControlInstances);
    if (PowerControlHandles == NULL) {
      DEBUG ((DEBUG_ERROR, "ERROR: Failed to allocate buffer for bpmp handles.\n"));
      Status = EFI_OUT_OF_RESOURCES;
      goto ErrorExit;
    }

    Status = GetMatchingEnabledDeviceTreeNodes ("nvidia,th500-powercontrol", PowerControlHandles, &NumPowerControlInstances);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "ERROR: Failed to get bpmp dtb node handles. Status = %r\n", Status));
      goto ErrorExit;
    }
  }

  for (Count = 0; Count < NumPowerControlInstances; Count++) {
    Status = GetDeviceTreeNode (PowerControlHandles[Count], &Dtb, &NodeOffset);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get power control dtb node information.\n", __func__));
      goto ErrorExit;
    }

    Property = NULL;
    Property = fdt_getprop (Dtb, NodeOffset, "bpmp", &DataLen);
    if ((Property == NULL) ||
        (DataLen != sizeof (UINT32)))
    {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get bpmp information from power control dtb node.\n", __func__));
      goto ErrorExit;
    }

    Data               = (CONST UINT32 *)Property;
    BpmpPhandle        = SwapBytes32 (*Data);
    Request.Command    = CmdPwrCntrlQueryAbi;
    Request.SubCommand = CmdPwrCntrlBypassSet;
    Status             = BpmpProcessPowerControlCommand (BpmpIpc, BpmpPhandle, &Request, sizeof (MRQ_PWR_CNTRL_ABI_PACKET));
    if (EFI_ERROR (Status)) {
      goto ErrorExit;
    }
  }

  mServerPowerControlProtocol.ConfigurePowerControl = ConfigurePowerControl;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gServerPowerControlProtocolGuid,
                  &mServerPowerControlProtocol,
                  NULL
                  );

ErrorExit:
  if (PowerControlHandles != NULL) {
    FreePool (PowerControlHandles);
  }

  return Status;
}
