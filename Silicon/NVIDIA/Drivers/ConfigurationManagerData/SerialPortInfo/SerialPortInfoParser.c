/** @file
  Serial port info parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "SerialPortInfoParser.h"
#include "../ConfigurationManagerDataRepoLib.h"

#include <Library/DeviceTreeHelperLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Library/PlatformResourceLib.h>
#include <Library/NVIDIADebugLib.h>
#include <NVIDIAConfiguration.h>

#include <IndustryStandard/DebugPort2Table.h>
#include <IndustryStandard/SerialPortConsoleRedirectionTable.h>

STATIC
CONST CHAR8  *TegraSerialPortCompatibility[] = {
  "nvidia,tegra20-uart",
  "nvidia,tegra186-hsuart",
  "nvidia,tegra194-hsuart",
  NULL
};

STATIC
CONST CHAR8  *ArmSerialPortCompatibility[] = {
  "arm,sbsa-uart",
  "arm,pl011",
  NULL
};

/** Serial port info parser function.

  The following structures are populated:
  - EArchCommonObjSerialDebugPortInfo
  OR
  - EArchCommonObjConsolePortInfo

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
SerialPortInfoParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                         Status;
  UINT32                             NumberOfSerialPorts;
  CM_ARCH_COMMON_SERIAL_PORT_INFO    *SpcrSerialPort;
  NVIDIA_DEVICE_TREE_REGISTER_DATA   RegisterData;
  NVIDIA_DEVICE_TREE_INTERRUPT_DATA  InterruptData;
  UINT32                             Index;
  UINT32                             Size;
  UINT8                              SerialPortConfig;
  UINT8                              SerialTypeConfig;
  CM_STD_OBJ_ACPI_TABLE_INFO         AcpiTableHeader;
  CONST CHAR8                        **Map;
  CM_OBJ_DESCRIPTOR                  Desc;
  UINTN                              ChipID;
  INT32                              NodeOffset;

  SpcrSerialPort = NULL;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  SerialPortConfig = PcdGet8 (PcdSerialPortConfig);
  if (SerialPortConfig == NVIDIA_SERIAL_PORT_DISABLED) {
    return EFI_SUCCESS;
  }

  SerialTypeConfig = PcdGet8 (PcdSerialTypeConfig);
  ChipID           = TegraGetChipID ();

  switch (ChipID) {
    case T194_CHIP_ID:
      if (SerialTypeConfig == NVIDIA_SERIAL_PORT_TYPE_16550) {
        Map = TegraSerialPortCompatibility;
      } else {
        return EFI_SUCCESS;
      }

      break;

    case T234_CHIP_ID: // JDS TODO - confirm that this is correct
      if (SerialTypeConfig == NVIDIA_SERIAL_PORT_TYPE_16550) {
        Map = TegraSerialPortCompatibility;
      } else {
        Map = ArmSerialPortCompatibility;
      }

      break;

    case T264_CHIP_ID:
      if (SerialTypeConfig == NVIDIA_SERIAL_PORT_TYPE_SBSA) {
        Map = ArmSerialPortCompatibility;
      } else {
        return EFI_SUCCESS;
      }

      break;

    case TH500_CHIP_ID:
      if (SerialTypeConfig == NVIDIA_SERIAL_PORT_TYPE_SBSA) {
        Map = ArmSerialPortCompatibility;
      } else {
        return EFI_SUCCESS;
      }

      break;

    default:
      DEBUG ((DEBUG_ERROR, "%a: Unable to determine how to handle SerialPort for ChipID 0x%x\n", __FUNCTION__, ChipID));
      return EFI_UNSUPPORTED;
  }

  Status = DeviceTreeGetCompatibleNodeCount (Map, &NumberOfSerialPorts);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get Serial Port info\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  SpcrSerialPort = (CM_ARCH_COMMON_SERIAL_PORT_INFO *)AllocateZeroPool (sizeof (CM_ARCH_COMMON_SERIAL_PORT_INFO) * NumberOfSerialPorts);
  if (SpcrSerialPort == NULL) {
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  NodeOffset = -1;
  Index      = 0;
  while (EFI_SUCCESS == DeviceTreeGetNextCompatibleNode (Map, &NodeOffset)) {
    // Only one register space is expected
    Size   = 1;
    Status = DeviceTreeGetRegisters (NodeOffset, &RegisterData, &Size);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to get registers - %r\r\n", __func__, Status));
      goto CleanupAndReturn;
    }

    // Only one interrupt is expected
    Size   = 1;
    Status = DeviceTreeGetInterrupts (NodeOffset, &InterruptData, &Size);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Unable to get interrupts - %r\r\n", __func__, Status));
      goto CleanupAndReturn;
    }

    SpcrSerialPort[Index].BaseAddress       = RegisterData.BaseAddress;
    SpcrSerialPort[Index].BaseAddressLength = RegisterData.Size;
    SpcrSerialPort[Index].Interrupt         = DEVICETREE_TO_ACPI_INTERRUPT_NUM (InterruptData);
    SpcrSerialPort[Index].BaudRate          = FixedPcdGet64 (PcdUartDefaultBaudRate);
    SpcrSerialPort[Index].Clock             = FixedPcdGet32 (PL011UartClkInHz);
    if (SerialTypeConfig == NVIDIA_SERIAL_PORT_TYPE_SBSA) {
      SpcrSerialPort[Index].PortSubtype = EFI_ACPI_DBG2_PORT_SUBTYPE_SERIAL_ARM_SBSA_GENERIC_UART;
    } else {
      if (SerialPortConfig == NVIDIA_SERIAL_PORT_SPCR_FULL_16550) {
        SpcrSerialPort[Index].PortSubtype = EFI_ACPI_DBG2_PORT_SUBTYPE_SERIAL_FULL_16550;
      } else {
        SpcrSerialPort[Index].PortSubtype = EFI_ACPI_DBG2_PORT_SUBTYPE_SERIAL_NVIDIA_16550_UART;
      }
    }

    Index++;
  }

  // Extend ACPI table list with the new table header
  if ((SerialPortConfig == NVIDIA_SERIAL_PORT_DBG2_SBSA) ||
      (SerialPortConfig == NVIDIA_SERIAL_PORT_DBG2_NVIDIA_16550))
  {
    AcpiTableHeader.AcpiTableSignature = EFI_ACPI_6_4_DEBUG_PORT_2_TABLE_SIGNATURE;
    AcpiTableHeader.AcpiTableRevision  = EFI_ACPI_DEBUG_PORT_2_TABLE_REVISION;
    AcpiTableHeader.TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdDbg2);
  } else {
    AcpiTableHeader.AcpiTableSignature = EFI_ACPI_6_4_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_SIGNATURE;
    AcpiTableHeader.AcpiTableRevision  = EFI_ACPI_SERIAL_PORT_CONSOLE_REDIRECTION_TABLE_REVISION;
    AcpiTableHeader.TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSpcr);
  }

  AcpiTableHeader.AcpiTableData = NULL;
  AcpiTableHeader.OemTableId    = PcdGet64 (PcdAcpiTegraUartOemTableId);
  AcpiTableHeader.OemRevision   = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
  AcpiTableHeader.MinorRevision = 0;

  Status = NvAddAcpiTableGenerator (ParserHandle, &AcpiTableHeader);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add SerialPort table generator\n", __FUNCTION__, Status));
    goto CleanupAndReturn;
  }

  // Create the new Serial entries
  if ((SerialPortConfig == NVIDIA_SERIAL_PORT_DBG2_SBSA) ||
      (SerialPortConfig == NVIDIA_SERIAL_PORT_DBG2_NVIDIA_16550))
  {
    Desc.ObjectId = CREATE_CM_ARCH_COMMON_OBJECT_ID (EArchCommonObjSerialDebugPortInfo);
  } else {
    Desc.ObjectId = CREATE_CM_ARCH_COMMON_OBJECT_ID (EArchCommonObjConsolePortInfo);
  }

  Desc.Size  = sizeof (CM_ARCH_COMMON_SERIAL_PORT_INFO) * NumberOfSerialPorts;
  Desc.Count = NumberOfSerialPorts;
  Desc.Data  = SpcrSerialPort;

  Status = NvAddMultipleCmObjGetTokens (ParserHandle, &Desc, NULL, NULL);
  if (EFI_ERROR (Status)) {
    goto CleanupAndReturn;
  }

CleanupAndReturn:
  FREE_NON_NULL (SpcrSerialPort);
  return Status;
}

REGISTER_PARSER_FUNCTION (SerialPortInfoParser, NULL)
