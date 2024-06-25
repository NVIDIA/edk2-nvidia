/** @file
  Usb info parser.

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "UsbInfoParser.h"
#include "../ConfigurationManagerDataRepoLib.h"
#include "../ResourceTokenUtility.h"

#include <Library/BaseMemoryLib.h>
#include <Library/ConfigurationManagerDataLib.h>
#include <Library/PrintLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/NvCmObjectDescUtility.h>

STATIC CONST CHAR8  *UsbCompatibleIds[] = {
  "nvidia,tegra186-xhci",
  "nvidia,tegra186-xusb",
  "nvidia,tegra194-xhci",
  "nvidia,tegra194-xusb",
  "nvidia,tegra234-xhci",
  "nvidia,tegra234-xusb",
  "nvidia,tegra264-xhci",
  "nvidia,tegra264-xusb",
  NULL
};

#define USB_HID          "NVDA0214"
#define USB_CID          "PNP0D10"
#define USB_NAME_FORMAT  "USB%x"
#define USB_MAX_DEVICES  16

/** Usb info parser function.

  Adds Usb information to the SSDT ACPI table being generated

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
UsbInfoParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                  Status;
  INT32                       NodeOffset;
  CM_ARM_GENERIC_DEVICE_INFO  DeviceInfo;
  CM_OBJ_DESCRIPTOR           *CmObjDesc;
  CM_STD_OBJ_ACPI_TABLE_INFO  AcpiTableHeader;

  NodeOffset = -1;

  CopyMem (DeviceInfo.Hid, USB_HID, sizeof (DeviceInfo.Hid));
  DeviceInfo.CidValid = TRUE;
  CopyMem (DeviceInfo.Cid, USB_CID, sizeof (DeviceInfo.Cid));
  DeviceInfo.Uid      = 0;
  DeviceInfo.HrvValid = FALSE;
  DeviceInfo.Cca      = TRUE;

  Status = NvCreateCmObjDesc (
             CREATE_CM_ARM_OBJECT_ID (EArmObjGenericDeviceInfo),
             1,
             &DeviceInfo,
             sizeof (DeviceInfo),
             &CmObjDesc
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  do {
    Status = DeviceTreeGetNextCompatibleNode (UsbCompatibleIds, &NodeOffset);
    if (EFI_ERROR (Status)) {
      break;
    }

    if (DeviceInfo.Uid >= USB_MAX_DEVICES) {
      break;
    }

    AsciiSPrint (DeviceInfo.Name, sizeof (DeviceInfo.Name), USB_NAME_FORMAT, DeviceInfo.Uid);

    Status = CreateMemoryRangesObject (
               ParserHandle,
               NodeOffset,
               1,
               NULL,
               NULL,
               &DeviceInfo.AddressResourceToken
               );
    if (EFI_ERROR (Status)) {
      break;
    }

    Status = CreateInterruptsObject (
               ParserHandle,
               NodeOffset,
               1,
               NULL,
               NULL,
               &DeviceInfo.InterruptResourceToken
               );
    if (EFI_ERROR (Status)) {
      break;
    }

    Status = NvExtendCmObj (ParserHandle, CmObjDesc, CM_NULL_TOKEN, NULL);
    if (Status == EFI_NOT_FOUND) {
      Status = NvAddMultipleCmObjGetTokens (ParserHandle, CmObjDesc, NULL, NULL);
    }

    if (EFI_ERROR (Status)) {
      break;
    }

    DeviceInfo.Uid++;
  } while (!EFI_ERROR (Status));

  if (DeviceInfo.Uid != 0) {
    AcpiTableHeader.AcpiTableSignature = EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE;
    AcpiTableHeader.AcpiTableRevision  = EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_REVISION;
    AcpiTableHeader.TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdtGenericDevice);
    AcpiTableHeader.AcpiTableData      = NULL;
    AcpiTableHeader.OemTableId         = 0;
    AcpiTableHeader.OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
    AcpiTableHeader.MinorRevision      = 0;

    Status = NvAddAcpiTableGenerator (ParserHandle, &AcpiTableHeader);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  return Status;
}

REGISTER_PARSER_FUNCTION (UsbInfoParser, NULL)
