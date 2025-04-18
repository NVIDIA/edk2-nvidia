/** @file
  UEFI Shell utility for NVIDIA PCIe controller information and diagnostics

  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/BaseMemoryLib.h>
#include <Library/UefiLib.h>
#include <Library/ShellLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiHiiServicesLib.h>
#include <Library/HiiLib.h>
#include <Library/PciLib.h>
#include <Protocol/PciIo.h>
#include <Protocol/DevicePath.h>
#include <Library/PciSegmentLib.h>
#include <Library/DevicePathLib.h>

#include <Protocol/PciPlatform.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>
#include <ConfigurationManagerObject.h>

#include "PcieOscDefinitions.h"

// Shell parameter list
SHELL_PARAM_ITEM  mPcieUtilParamList[] = {
  { L"--list", TypeFlag },
  { L"-?",     TypeFlag },
  { NULL,      TypeMax  },
};

EFI_HII_HANDLE  mHiiHandle;
CHAR16          mAppName[] = L"PcieUtil";

/**
  Display command usage and help
**/
VOID
DisplayHelp (
  VOID
  )
{
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_HELP), mHiiHandle);
}

/**
  Display NVIDIA-specific controller information

  @param[in]  ConfigIo     Pointer to NVIDIA configuration IO protocol
**/
VOID
DisplayNvidiaControllerInfo (
  IN NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL  *ConfigIo
  )
{
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_NVIDIA_INFO_HEADER), mHiiHandle);

  // Display BPMP Phandle information
  if (ConfigIo->BpmpPhandle != 0) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_BPMP_PHANDLE), mHiiHandle, ConfigIo->BpmpPhandle);
  } else {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_NO_BPMP), mHiiHandle);
  }

  // Display HBM memory range information for C2C connectivity
  if ((ConfigIo->HbmRangeStart != 0) && (ConfigIo->HbmRangeSize != 0)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_C2C_PRESENT), mHiiHandle);
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_PCIE_UTIL_HBM_RANGE),
      mHiiHandle,
      ConfigIo->HbmRangeStart,
      ConfigIo->HbmRangeStart + ConfigIo->HbmRangeSize - 1
      );
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_PCIE_UTIL_HBM_SIZE),
      mHiiHandle,
      ConfigIo->HbmRangeSize,
      ConfigIo->HbmRangeSize / (1024 * 1024)
      );

    // Display proximity domain information
    if (ConfigIo->NumProximityDomains > 0) {
      ShellPrintHiiEx (
        -1,
        -1,
        NULL,
        STRING_TOKEN (STR_PCIE_UTIL_PROXIMITY_DOMAIN),
        mHiiHandle,
        ConfigIo->ProximityDomainStart,
        ConfigIo->ProximityDomainStart + ConfigIo->NumProximityDomains - 1,
        ConfigIo->NumProximityDomains
        );
    }
  } else {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_NO_C2C), mHiiHandle);
  }

  // Display OS control information
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_OSC_CTRL), mHiiHandle, ConfigIo->OSCCtrl);

  // Display port type information
  if (ConfigIo->IsExternalFacingPort) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_PORT_TYPE_EXTERNAL), mHiiHandle);
  } else {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_PORT_TYPE_INTERNAL), mHiiHandle);
  }

  // Display controller physical location information if available
  if ((ConfigIo->SocketID < 8) && (ConfigIo->ControllerID < 16)) {
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_PCIE_UTIL_PHYSICAL_LOCATION),
      mHiiHandle,
      ConfigIo->SocketID,
      ConfigIo->ControllerID
      );
  }
}

/**
  Display decoded OSCCtrl field bits

  @param[in]  OSCCtrl     The OSCCtrl field value
**/
VOID
DisplayOscControlBits (
  IN UINT32  OSCCtrl
  )
{
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_OSC_HEADER), mHiiHandle);

  // Define all known OSC Control bits using NVIDIA's definitions
  struct {
    UINT32    Bit;
    CHAR16    *Name;
    CHAR16    *Description;
  } OscBits[] = {
    { PCIE_FW_OSC_CTRL_PCIE_NATIVE_HP,     L"NATIVE_HP",     L"Native PCIe Hot-Plug"         },
    { PCIE_FW_OSC_CTRL_SHPC_NATIVE_HP,     L"SHPC_HP",       L"Standard Hot-Plug Controller" },
    { PCIE_FW_OSC_CTRL_PCIE_NATIVE_PME,    L"NATIVE_PME",    L"Native PCIe PME"              },
    { PCIE_FW_OSC_CTRL_PCIE_AER,           L"AER",           L"Advanced Error Reporting"     },
    { PCIE_FW_OSC_CTRL_PCIE_CAP_STRUCTURE, L"CAP_STRUCTURE", L"PCIe Capability Structure"    },
    { PCIE_FW_OSC_CTRL_LTR,                L"LTR",           L"Latency Tolerance Reporting"  },
    { PCIE_FW_OSC_CTRL_RSVD,               L"RESERVED",      L"Reserved"                     },
    { PCIE_FW_OSC_CTRL_PCIE_DPC,           L"DPC",           L"Downstream Port Containment"  },
    { PCIE_FW_OSC_CTRL_PCIE_CMPL_TO,       L"CMPL_TO",       L"Completion Timeout Control"   },
    { PCIE_FW_OSC_CTRL_PCIE_SFI,           L"SFI",           L"System Firmware Intermediary" },
    { 0,                                   NULL,             NULL                            } // Terminator
  };

  // Print each bit status
  UINT32  Index;

  for (Index = 0; OscBits[Index].Name != NULL; Index++) {
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_PCIE_UTIL_OSC_BIT_STATUS),
      mHiiHandle,
      OscBits[Index].Name,
      OscBits[Index].Description,
      (OSCCtrl & OscBits[Index].Bit) ? L"Enabled" : L"Disabled"
      );
  }

  // Print unknown bits if any are set
  UINT32  KnownBits = 0;

  for (Index = 0; OscBits[Index].Name != NULL; Index++) {
    KnownBits |= OscBits[Index].Bit;
  }

  UINT32  UnknownBits = OSCCtrl & ~KnownBits;

  if (UnknownBits != 0) {
    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_PCIE_UTIL_OSC_UNKNOWN_BITS),
      mHiiHandle,
      UnknownBits
      );
  }
}

/**
  Print information about a specific PCIe controller

  @param[in]  Handle                   Controller handle
  @param[in]  ConfigIo                 Pointer to configuration IO protocol
  @param[in]  ConfigData               Pointer to configuration data protocol

  @retval EFI_SUCCESS                  Information displayed successfully
**/
EFI_STATUS
PrintControllerInfo (
  IN EFI_HANDLE                                        Handle,
  IN NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL  *ConfigIo,
  IN CM_ARCH_COMMON_PCI_CONFIG_SPACE_INFO              *ConfigData
  )
{
  EFI_STATUS                Status;
  EFI_DEVICE_PATH_PROTOCOL  *DevicePath     = NULL;
  CHAR16                    *DevicePathText = NULL;
  UINT16                    LinkStatus;
  UINT8                     LinkSpeed;
  UINT8                     LinkWidth;
  UINT32                    DeviceCapabilities;
  UINT16                    PCIeCapOff = 0;

  Status = gBS->HandleProtocol (
                  Handle,
                  &gEfiDevicePathProtocolGuid,
                  (VOID **)&DevicePath
                  );
  if (!EFI_ERROR (Status) && (DevicePath != NULL)) {
    DevicePathText = ConvertDevicePathToText (DevicePath, FALSE, FALSE);
  }

  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_SEPARATOR), mHiiHandle);
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_CONTROLLER_INFO), mHiiHandle);
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_SOCKET_ID), mHiiHandle, ConfigIo->SocketID);
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_CONTROLLER_ID), mHiiHandle, ConfigIo->ControllerID);
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_SEGMENT), mHiiHandle, ConfigIo->SegmentNumber);
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_BUS_RANGE), mHiiHandle, ConfigIo->MinBusNumber, ConfigIo->MaxBusNumber);
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_ECAM_BASE), mHiiHandle, ConfigIo->EcamBase);

  if (DevicePathText != NULL) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_DEVICE_PATH), mHiiHandle, DevicePathText);
    FreePool (DevicePathText);
  }

  // Find the PCIe capability offset
  UINT8  CapOffset = 0x34; // Start of capabilities pointer in PCI header
  UINT8  CapId;
  UINT8  NextCapOffset;

  // Read capabilities pointer
  NextCapOffset = PciSegmentRead8 (
                    PCI_SEGMENT_LIB_ADDRESS (
                      ConfigIo->SegmentNumber,
                      0,                      // Primary bus
                      0,                      // Device
                      0,                      // Function
                      CapOffset               // Capabilities Pointer offset in header
                      )
                    );

  // Walk through capabilities list to find PCIe capability (ID 0x10)
  while (NextCapOffset != 0) {
    CapId = PciSegmentRead8 (
              PCI_SEGMENT_LIB_ADDRESS (
                ConfigIo->SegmentNumber,
                0,                           // Primary bus
                0,                           // Device
                0,                           // Function
                NextCapOffset                // Current capability offset
                )
              );

    if (CapId == 0x10) {
      // PCIe capability ID
      PCIeCapOff = NextCapOffset;
      break;
    }

    // Get pointer to next capability
    NextCapOffset = PciSegmentRead8 (
                      PCI_SEGMENT_LIB_ADDRESS (
                        ConfigIo->SegmentNumber,
                        0,                            // Primary bus
                        0,                            // Device
                        0,                            // Function
                        NextCapOffset + 1             // Next capability pointer is at offset+1
                        )
                      );
  }

  // Read link status - locate PCIe capability offset first
  if (PCIeCapOff != 0) {
    // Read link status from PCIe capability structure
    LinkStatus = PciSegmentRead16 (
                   PCI_SEGMENT_LIB_ADDRESS (
                     ConfigIo->SegmentNumber,
                     0,                              // Primary bus 0
                     0,                              // Device 0 (Root port)
                     0,                              // Function 0
                     PCIeCapOff + 0x12               // Link Status offset
                     )
                   );

    // Read link capabilities
    DeviceCapabilities = PciSegmentRead32 (
                           PCI_SEGMENT_LIB_ADDRESS (
                             ConfigIo->SegmentNumber,
                             0,                             // Primary bus 0
                             0,                             // Device 0 (Root port)
                             0,                             // Function 0
                             PCIeCapOff + 0xC               // Link Capability offset
                             )
                           );

    LinkSpeed = (LinkStatus >> 0) & 0xF;  // Current Link Speed is bits [3:0]
    LinkWidth = (LinkStatus >> 4) & 0x3F; // Negotiated Link Width is bits [9:4]

    ShellPrintHiiEx (
      -1,
      -1,
      NULL,
      STRING_TOKEN (STR_PCIE_UTIL_LINK_STATUS),
      mHiiHandle,
      ((LinkStatus & BIT13) != 0) ? L"UP" : L"DOWN"
      );

    if ((LinkStatus & BIT13) != 0) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_LINK_SPEED), mHiiHandle, LinkSpeed);
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_LINK_WIDTH), mHiiHandle, LinkWidth);

      ShellPrintHiiEx (
        -1,
        -1,
        NULL,
        STRING_TOKEN (STR_PCIE_UTIL_MAX_CAPABILITY),
        mHiiHandle,
        DeviceCapabilities & 0xF,                    // Max Link Speed is bits [3:0]
        (DeviceCapabilities >> 4) & 0x3F             // Max Link Width is bits [9:4]
        );
    }
  } else {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_LINK_UNKNOWN), mHiiHandle);
  }

  if (ConfigIo->IsExternalFacingPort) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_PORT_TYPE), mHiiHandle);
  }

  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_SEPARATOR), mHiiHandle);
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_SEPARATOR), mHiiHandle);

  DisplayNvidiaControllerInfo (ConfigIo);

  DisplayOscControlBits (ConfigIo->OSCCtrl);

  return EFI_SUCCESS;
}

/**
  List all NVIDIA PCIe controllers in the system

  @retval EFI_SUCCESS     The command completed successfully
  @retval Others          An error occurred
**/
EFI_STATUS
ListPcieControllers (
  VOID
  )
{
  EFI_STATUS                                        Status;
  EFI_HANDLE                                        *HandleBuffer = NULL;
  UINTN                                             HandleCount   = 0;
  UINTN                                             Index;
  NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL  *ConfigIo       = NULL;
  CM_ARCH_COMMON_PCI_CONFIG_SPACE_INFO              *ConfigData     = NULL;
  UINTN                                             ControllerCount = 0;

  // Find all handles with the NVIDIA PCIe configuration protocol
  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gNVIDIAPciRootBridgeConfigurationIoProtocolGuid,
                  NULL,
                  &HandleCount,
                  &HandleBuffer
                  );

  if (EFI_ERROR (Status) || (HandleCount == 0)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_NO_PCIE), mHiiHandle, mAppName);
    return EFI_NOT_FOUND;
  }

  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_PROTOCOL_FOUND), mHiiHandle, mAppName);
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_CONTROLLERS_FOUND), mHiiHandle, mAppName, HandleCount);

  // Process each controller
  for (Index = 0; Index < HandleCount; Index++) {
    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gNVIDIAPciRootBridgeConfigurationIoProtocolGuid,
                    (VOID **)&ConfigIo
                    );

    if (EFI_ERROR (Status) || (ConfigIo == NULL)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_CONFIG_IO_FAILED), mHiiHandle, mAppName, Status);
      continue;
    }

    Status = gBS->HandleProtocol (
                    HandleBuffer[Index],
                    &gNVIDIAPciConfigurationDataProtocolGuid,
                    (VOID **)&ConfigData
                    );

    if (EFI_ERROR (Status) || (ConfigData == NULL)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_CONFIG_DATA_FAILED), mHiiHandle, mAppName, Status);
    }

    ControllerCount++;
    PrintControllerInfo (HandleBuffer[Index], ConfigIo, ConfigData);
  }

  if (HandleBuffer != NULL) {
    FreePool (HandleBuffer);
  }

  if (ControllerCount == 0) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_NO_VALID_CONTROLLERS), mHiiHandle, mAppName);
    return EFI_NOT_FOUND;
  }

  return EFI_SUCCESS;
}

/**
  UEFI application entry point.

  @param[in]  ImageHandle    The image handle of this application.
  @param[in]  SystemTable    The pointer to the EFI System Table.

  @retval EFI_SUCCESS        The operation completed successfully.
  @retval Others             An error occurred.
**/
EFI_STATUS
EFIAPI
InitializePcieUtil (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS                   Status;
  LIST_ENTRY                   *ParamPackage;
  CHAR16                       *ProblemParam;
  EFI_HII_PACKAGE_LIST_HEADER  *PackageList;

  //
  // Retrieve HII package list from ImageHandle
  //
  Status = gBS->OpenProtocol (
                  ImageHandle,
                  &gEfiHiiPackageListProtocolGuid,
                  (VOID **)&PackageList,
                  ImageHandle,
                  NULL,
                  EFI_OPEN_PROTOCOL_GET_PROTOCOL
                  );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // Publish HII package list to HII Database.
  //
  Status = gHiiDatabase->NewPackageList (
                           gHiiDatabase,
                           PackageList,
                           NULL,
                           &mHiiHandle
                           );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  ASSERT (mHiiHandle != NULL);

  Status = ShellCommandLineParseEx (mPcieUtilParamList, &ParamPackage, &ProblemParam, TRUE, FALSE);
  if (EFI_ERROR (Status)) {
    ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_INVALID_PARAM), mHiiHandle, mAppName);
    goto Done;
  }

  if (ShellCommandLineGetFlag (ParamPackage, L"-?")) {
    DisplayHelp ();
    goto Done;
  }

  if (ShellCommandLineGetFlag (ParamPackage, L"--list")) {
    Status = ListPcieControllers ();
    if (EFI_ERROR (Status)) {
      ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_NO_PCIE), mHiiHandle, mAppName);
    }

    goto Done;
  }

  // Unknown command - display help
  ShellPrintHiiEx (-1, -1, NULL, STRING_TOKEN (STR_PCIE_UTIL_INVALID_PARAM), mHiiHandle, mAppName);
  DisplayHelp ();

Done:
  ShellCommandLineFreeVarList (ParamPackage);
  HiiRemovePackages (mHiiHandle);

  return EFI_SUCCESS;
}
