/** @file
  SSDT Pcie Table Generator.

  SPDX-FileCopyrightText: Copyright (c) 2022-2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2021 - 2022, Arm Limited. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Reference(s):
  - PCI Firmware Specification - Revision 3.0
  - ACPI 6.4 specification:
   - s6.2.13 "_PRT (PCI Routing Table)"
   - s6.1.1 "_ADR (Address)"
  - linux kernel code
  - Arm Base Boot Requirements v1.0
  - Arm Base System Architecture v1.0
**/

#include <Library/AcpiLib.h>
#include <Library/BaseLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/DebugLib.h>
#include <Library/DevicePathLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PrintLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/PciRootBridgeConfigurationIo.h>
#include <Protocol/AcpiTable.h>

// Module specific include files.
#include <AcpiTableGenerator.h>
#include <ConfigurationManagerObject.h>
#include <ConfigurationManagerHelper.h>
#include <Library/AcpiHelperLib.h>
#include <Library/TableHelperLib.h>
#include <Library/AmlLib/AmlLib.h>
#include <Library/SsdtPcieSupportLib.h>
#include <Protocol/ConfigurationManagerProtocol.h>
#include <Protocol/PciIo.h>
#include <Protocol/RasNsCommPcieDpcDataProtocol.h>
#include <Protocol/GpuDsdAmlGenerationProtocol.h>
#include <TH500/TH500Definitions.h>

extern CHAR8  ssdtpcietemplate_aml_code[];

#define DSD_EXTERNAL_FACING_PORT_GUID \
(GUID){0xEFCC06CC, 0x73AC, 0x4BC3, {0xBF, 0xF0, 0x76, 0x14, 0x38, 0x07, 0xC3, 0x89}}

#define NV_THERM_I2CS_SCRATCH  0x200bc

STATIC
EFI_STATUS
EFIAPI
GeneratePciDSDForExtPort (
  IN       CONST CM_ARM_PCI_CONFIG_SPACE_INFO  *PciInfo,
  IN  OUT        AML_OBJECT_NODE_HANDLE        RpNode,
  IN             UINT32                        Uid
  )
{
  EFI_STATUS              Status;
  AML_OBJECT_NODE_HANDLE  DsdNode;
  AML_OBJECT_NODE_HANDLE  DsdPkgNode;
  EFI_HANDLE              *Handles = NULL;
  UINTN                   NumberOfHandles;
  UINTN                   CurrentHandle;
  BOOLEAN                 IsExternalFacingPort;
  EFI_GUID                ExtPortGUID = DSD_EXTERNAL_FACING_PORT_GUID;

  Status = gBS->LocateHandleBuffer (
                  ByProtocol,
                  &gNVIDIAPciRootBridgeConfigurationIoProtocolGuid,
                  NULL,
                  &NumberOfHandles,
                  &Handles
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to locate host bridge protocols, %r.\r\n", __FUNCTION__, Status));
    goto exit_handler;
  }

  for (CurrentHandle = 0; CurrentHandle < NumberOfHandles; CurrentHandle++) {
    NVIDIA_PCI_ROOT_BRIDGE_CONFIGURATION_IO_PROTOCOL  *RootBridgeCfgIo = NULL;
    Status = gBS->HandleProtocol (
                    Handles[CurrentHandle],
                    &gNVIDIAPciRootBridgeConfigurationIoProtocolGuid,
                    (VOID **)&RootBridgeCfgIo
                    );
    if (EFI_ERROR (Status)) {
      DEBUG ((
        DEBUG_ERROR,
        "%a: Failed to get protocol for handle %p, %r.\r\n",
        __FUNCTION__,
        Handles[CurrentHandle],
        Status
        ));
      goto exit_handler;
    }

    if (PciInfo->PciSegmentGroupNumber != RootBridgeCfgIo->SegmentNumber) {
      continue;
    }

    IsExternalFacingPort = RootBridgeCfgIo->IsExternalFacingPort;
  }

  if (!IsExternalFacingPort) {
    return EFI_SUCCESS;
  }

  Status = AmlCodeGenNamePackage ("_DSD", NULL, &DsdNode);
  if (EFI_ERROR (Status)) {
    ASSERT (0);
    goto exit_handler;
  }

  Status = AmlAddDeviceDataDescriptorPackage (
             &ExtPortGUID,
             DsdNode,
             &DsdPkgNode
             );
  if (EFI_ERROR (Status)) {
    ASSERT (0);
    goto exit_handler;
  }

  Status = AmlAddNameIntegerPackage ("ExternalFacingPort", 1, DsdPkgNode);
  if (EFI_ERROR (Status)) {
    ASSERT (0);
    goto exit_handler;
  }

  Status = AmlAddNameIntegerPackage ("UID", Uid, DsdPkgNode);
  if (EFI_ERROR (Status)) {
    ASSERT (0);
    goto exit_handler;
  }

  Status = AmlAttachNode (RpNode, DsdNode);
  if (EFI_ERROR (Status)) {
    ASSERT_EFI_ERROR (Status);
    goto exit_handler;
  }

exit_handler:
  if (Handles != NULL) {
    FreePool (Handles);
  }

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
UpdateSharedNSMemAddr (
  IN       CONST CM_ARM_PCI_CONFIG_SPACE_INFO  *PciInfo,
  IN  OUT        AML_OBJECT_NODE_HANDLE        RpNode,
  IN             UINT32                        Uid
  )
{
  EFI_STATUS                   Status;
  RAS_PCIE_DPC_COMM_BUF_INFO   *DpcCommBuf = NULL;
  RAS_FW_PCIE_DPC_COMM_STRUCT  *DpcComm    = NULL;
  AML_OBJECT_NODE_HANDLE       AddrNode;
  UINT32                       Socket, Instance;

  Status = gBS->LocateProtocol (
                  &gNVIDIARasNsCommPcieDpcDataProtocolGuid,
                  NULL,
                  (VOID **)&DpcCommBuf
                  );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "%a: Couldn't get gNVIDIARasNsCommPcieDpcDataProtocolGuid protocol: %r\n",
      __FUNCTION__,
      Status
      ));
  }

  if (DpcCommBuf == NULL) {
    // Protocol installed NULL interface. Skip using it.
    return EFI_SUCCESS;
  }

  DpcComm  = (RAS_FW_PCIE_DPC_COMM_STRUCT *)DpcCommBuf->PcieBase;
  Socket   = Uid >> 4;
  Instance = Uid & 0xF;
  DEBUG ((DEBUG_VERBOSE, "%a: Socket = %u, Instance = %u\r\n", __FUNCTION__, Socket, Instance));

  Status = AmlFindNode (RpNode, "ADDR", &AddrNode);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = AmlNameOpUpdateInteger (AddrNode, (UINT64)(&(DpcComm->PcieDpcInfo[Socket][Instance])));
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
UpdateLICAddr (
  IN       CONST CM_ARM_PCI_CONFIG_SPACE_INFO  *PciInfo,
  IN  OUT        AML_OBJECT_NODE_HANDLE        Node,
  IN             UINT32                        Uid,
  IN             EFI_PHYSICAL_ADDRESS          Base
  )
{
  EFI_STATUS              Status;
  AML_OBJECT_NODE_HANDLE  LicaNode;
  UINT64                  Socket;
  EFI_PHYSICAL_ADDRESS    Address;

  Socket = Uid >> 4;

  Status = AmlFindNode (Node, "LICA", &LicaNode);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Address = Base | (Socket << TH500_SOCKET_SHFT);

  Status = AmlNameOpUpdateInteger (LicaNode, Address);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return Status;
}

STATIC
EFI_STATUS
EFIAPI
UpdateFSPBootAddr (
  IN       CONST CM_ARM_PCI_CONFIG_SPACE_INFO  *PciInfo,
  IN  OUT        AML_OBJECT_NODE_HANDLE        Node
  )
{
  EFI_STATUS              Status;
  AML_OBJECT_NODE_HANDLE  FspaNode;
  EFI_PHYSICAL_ADDRESS    Address;
  CHAR8                   NodeName[] = "FSPA";

  if (PciInfo->BaseAddress < TH500_VDM_SIZE) {
    return EFI_INVALID_PARAMETER;
  }

  Status = AmlFindNode (Node, NodeName, &FspaNode);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Address = PciInfo->BaseAddress - TH500_VDM_SIZE + NV_THERM_I2CS_SCRATCH;
  Status  = AmlNameOpUpdateInteger (FspaNode, Address);
  return Status;
}

/** Generate Pci slots devices.

  PCI Firmware Specification - Revision 3.3,
  s4.8 "Generic ACPI PCI Slot Description" requests to describe the PCI slot
  used. It should be possible to enumerate them, but this is additional
  information.

  @param [in]       PciInfo       Pci device information.
  @param [in]       MappingTable  The mapping table structure.
  @param [in]       Uid           Unique Id of the Pci device.
  @param [in, out]  PciNode       Pci node to amend.

  @retval EFI_SUCCESS            Success.
  @retval EFI_INVALID_PARAMETER  Invalid parameter.
  @retval EFI_OUT_OF_RESOURCES   Failed to allocate memory.
**/
EFI_STATUS
EFIAPI
GeneratePciSlots (
  IN      CONST CM_ARM_PCI_CONFIG_SPACE_INFO  *PciInfo,
  IN      CONST MAPPING_TABLE                 *MappingTable,
  IN            UINT32                        Uid,
  IN  OUT       AML_OBJECT_NODE_HANDLE        PciNode
  )
{
  EFI_STATUS                              Status;
  EFI_STATUS                              Status1;
  EFI_ACPI_DESCRIPTION_HEADER             *SsdtPcieTemplate;
  AML_ROOT_NODE_HANDLE                    TemplateRoot;
  AML_OBJECT_NODE_HANDLE                  GpuNode;
  AML_OBJECT_NODE_HANDLE                  DsdNode;
  AML_OBJECT_NODE_HANDLE                  RpNode;
  EFI_HANDLE                              *HandleBuffer;
  UINTN                                   NumberOfHandles;
  UINTN                                   HandleIndex;
  EFI_PCI_IO_PROTOCOL                     *PciIo;
  UINTN                                   SegmentNumber;
  UINTN                                   BusNumber;
  UINTN                                   DeviceNumber;
  UINTN                                   FunctionNumber;
  NVIDIA_GPU_DSD_AML_GENERATION_PROTOCOL  *GpuDsdGeneration;
  PCI_DEVICE_PATH                         *PciDevicePathNode;
  EFI_DEVICE_PATH_PROTOCOL                *DevicePath;
  AML_OBJECT_NODE_HANDLE                  CurrentNode;
  AML_OBJECT_NODE_HANDLE                  SwitchNode;
  CHAR8                                   SwitchName[5];
  UINT8                                   SwitchNumber;
  BOOLEAN                                 GpuNodeIsDetached;

  ASSERT (
    PciNode !=  NULL
    );

  // Parse the Ssdt Pci Osc Template.
  SsdtPcieTemplate = (EFI_ACPI_DESCRIPTION_HEADER *)
                     ssdtpcietemplate_aml_code;

  GpuNodeIsDetached = FALSE;
  GpuNode           = NULL;
  TemplateRoot      = NULL;
  Status            = AmlParseDefinitionBlock (
                        SsdtPcieTemplate,
                        &TemplateRoot
                        );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: SSDT-PCI: Failed to parse SSDT PCI Template."
      " Status = %r\n",
      Status
      ));
    return Status;
  }

  Status = AmlFindNode (TemplateRoot, "\\RP00", &RpNode);
  if (EFI_ERROR (Status)) {
    goto error_handler;
  }

  Status = AmlDetachNode (RpNode);
  if (EFI_ERROR (Status)) {
    goto error_handler;
  }

  Status = AmlAttachNode (PciNode, RpNode);
  if (EFI_ERROR (Status)) {
    // Free the detached node.
    AmlDeleteTree (RpNode);
    goto error_handler;
  }

  /*
   * Using the Socket-ID as the Proximity Domain ID.
   * Extract the Socket-ID from the Segment number.
   */
  Status = AmlCodeGenNameInteger (
             "_PXM",
             (PciInfo->PciSegmentGroupNumber >> 4) & 0xF,
             PciNode,
             NULL
             );
  if (EFI_ERROR (Status)) {
    ASSERT (0);
    return Status;
  }

  Status = GeneratePciDSDForExtPort (PciInfo, RpNode, Uid);
  if (EFI_ERROR (Status)) {
    goto error_handler;
  }

  Status = UpdateSharedNSMemAddr (PciInfo, RpNode, Uid);
  if (EFI_ERROR (Status)) {
    goto error_handler;
  }

  Status = UpdateLICAddr (PciInfo, RpNode, Uid, TH500_SW_IO4_BASE_SOCKET_0);
  if (EFI_ERROR (Status)) {
    goto error_handler;
  }

  Status1 = gBS->LocateHandleBuffer (
                   ByProtocol,
                   &gEfiNVIDIAGpuDSDAMLGenerationProtocolGuid,
                   NULL,
                   &NumberOfHandles,
                   &HandleBuffer
                   );
  DEBUG ((
    DEBUG_ERROR,
    "DEBUG: SSDT-PCI: GpuDSDAMLGeneration Proocol(s) [handles:%u handle buffer:%p]"      " Status = %r\n",
    NumberOfHandles,
    HandleBuffer,
    Status1
    ));

  if (!EFI_ERROR (Status1)) {
    for (HandleIndex = 0; HandleIndex < NumberOfHandles; HandleIndex++) {
      Status1 = gBS->HandleProtocol (
                       HandleBuffer[HandleIndex],
                       &gEfiPciIoProtocolGuid,
                       (VOID **)&PciIo
                       );
      if (EFI_ERROR (Status1)) {
        continue;
      }

      Status1 = PciIo->GetLocation (PciIo, &SegmentNumber, &BusNumber, &DeviceNumber, &FunctionNumber);
      if (EFI_ERROR (Status1)) {
        continue;
      }

      if (SegmentNumber != PciInfo->PciSegmentGroupNumber) {
        continue;
      }

      Status = gBS->HandleProtocol (
                      HandleBuffer[HandleIndex],
                      &gEfiNVIDIAGpuDSDAMLGenerationProtocolGuid,
                      (VOID **)&GpuDsdGeneration
                      );
      DEBUG ((
        DEBUG_ERROR,
        "DEBUG: SSDT-PCI: GpuDSDAMLGeneration Proocol(s) [HandleIndex:%u Protocol:%p]"      " Status = %r\n",
        HandleIndex,
        GpuDsdGeneration,
        Status1
        ));

      if (EFI_ERROR (Status)) {
        ASSERT_EFI_ERROR (Status);
        goto error_handler;
      }

      Status = AmlFindNode (TemplateRoot, "\\GPU0", &GpuNode);
      if (EFI_ERROR (Status)) {
        goto error_handler;
      }

      Status = AmlDetachNode (GpuNode);
      if (EFI_ERROR (Status)) {
        goto error_handler;
      }

      GpuNodeIsDetached = TRUE;
      Status            = GpuDsdGeneration->GetDsdNode (GpuDsdGeneration, &DsdNode);
      if (EFI_ERROR (Status)) {
        ASSERT_EFI_ERROR (Status);
      } else {
        Status = AmlAttachNode (GpuNode, DsdNode);
        if (EFI_ERROR (Status)) {
          goto error_handler;
        }
      }

      Status = gBS->HandleProtocol (
                      HandleBuffer[HandleIndex],
                      &gEfiDevicePathProtocolGuid,
                      (VOID **)&DevicePath
                      );
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: no GPU device path: %r\n", __FUNCTION__, Status));
        goto error_handler;
      }

      CurrentNode  = NULL;
      SwitchNumber = 0;
      while (!IsDevicePathEnd (DevicePath)) {
        DEBUG ((DEBUG_INFO, "%a: type=0x%x subtype=0x%x\n", __FUNCTION__, DevicePath->Type, DevicePath->SubType));

        if ((DevicePath->Type == HARDWARE_DEVICE_PATH) &&
            (DevicePath->SubType == HW_PCI_DP))
        {
          PciDevicePathNode = (PCI_DEVICE_PATH *)DevicePath;
          DEBUG ((DEBUG_INFO, "%a: Pci Dev=0x%x Func=0x%x \n", __FUNCTION__, PciDevicePathNode->Device, PciDevicePathNode->Function));

          // First PCI device in path is RP node
          // Last PCI device in path is the GPU node
          // Any additional PCI devices between first and last are switch nodes
          if (IsDevicePathEnd (NextDevicePathNode (DevicePath))) {
            DevicePath = NextDevicePathNode (DevicePath);
            continue;
          }

          if (CurrentNode == NULL) {
            CurrentNode = RpNode;
          } else {
            AsciiSPrint (SwitchName, sizeof (SwitchName), "SW%02x", SwitchNumber);
            Status = AmlCodeGenDevice (SwitchName, CurrentNode, &SwitchNode);
            if (EFI_ERROR (Status)) {
              goto error_handler;
            }

            CurrentNode = SwitchNode;
            Status      = AmlCodeGenNameInteger (
                            "_ADR",
                            (PciDevicePathNode->Device << 16) | PciDevicePathNode->Function,
                            SwitchNode,
                            NULL
                            );
            if (EFI_ERROR (Status)) {
              goto error_handler;
            }

            DEBUG ((DEBUG_INFO, "%a: inserted %a\n", __FUNCTION__, SwitchName));

            SwitchNumber++;
          }
        }

        DevicePath = NextDevicePathNode (DevicePath);
      }

      if (CurrentNode == NULL) {
        DEBUG ((DEBUG_ERROR, "%a: bad DP\n", __FUNCTION__));
        goto error_handler;
      }

      Status = AmlAttachNode (CurrentNode, GpuNode);
      if (EFI_ERROR (Status)) {
        goto error_handler;
      }

      GpuNodeIsDetached = FALSE;
      Status            = UpdateLICAddr (PciInfo, GpuNode, Uid, TH500_SW_IO1_BASE_SOCKET_0);
      if (EFI_ERROR (Status)) {
        goto error_handler;
      }

      Status = UpdateFSPBootAddr (PciInfo, GpuNode);
      if (EFI_ERROR (Status)) {
        goto error_handler;
      }

      break;
    }

    FreePool (HandleBuffer);
  }

error_handler:
  // Cleanup
  if (GpuNodeIsDetached) {
    AmlDeleteTree (GpuNode);
  }

  Status1 = AmlDeleteTree (TemplateRoot);
  if (EFI_ERROR (Status1)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: SSDT-PCI: Failed to cleanup AML tree."
      " Status = %r\n",
      Status1
      ));
    // If Status was success but we failed to delete the AML Tree
    // return Status1 else return the original error code, i.e. Status.
    if (!EFI_ERROR (Status)) {
      return Status1;
    }
  }

  return Status;
}

/** Add an _OSC template method to the PciNode.

  The _OSC method is provided as an AML blob. The blob is
  parsed and attached at the end of the PciNode list of variable elements.

  @param [in]       PciInfo     Pci device information.
  @param [in, out]  PciNode     Pci node to amend.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_OUT_OF_RESOURCES    Could not allocate memory.
**/
EFI_STATUS
EFIAPI
AddOscMethod (
  IN      CONST CM_ARM_PCI_CONFIG_SPACE_INFO  *PciInfo,
  IN  OUT   AML_OBJECT_NODE_HANDLE            PciNode
  )
{
  EFI_STATUS                   Status;
  EFI_STATUS                   Status1;
  EFI_ACPI_DESCRIPTION_HEADER  *SsdtPcieOscTemplate;
  AML_ROOT_NODE_HANDLE         OscTemplateRoot;
  AML_OBJECT_NODE_HANDLE       OscNode;

  ASSERT (PciNode != NULL);

  // Parse the Ssdt Pci Osc Template.
  SsdtPcieOscTemplate = (EFI_ACPI_DESCRIPTION_HEADER *)
                        ssdtpcietemplate_aml_code;

  OscNode         = NULL;
  OscTemplateRoot = NULL;
  Status          = AmlParseDefinitionBlock (
                      SsdtPcieOscTemplate,
                      &OscTemplateRoot
                      );
  if (EFI_ERROR (Status)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: SSDT-PCI-OSC: Failed to parse SSDT PCI OSC Template."
      " Status = %r\n",
      Status
      ));
    return Status;
  }

  Status = AmlFindNode (OscTemplateRoot, "\\_OSC", &OscNode);
  if (EFI_ERROR (Status)) {
    goto error_handler;
  }

  Status = AmlDetachNode (OscNode);
  if (EFI_ERROR (Status)) {
    goto error_handler;
  }

  Status = AmlAttachNode (PciNode, OscNode);
  if (EFI_ERROR (Status)) {
    // Free the detached node.
    AmlDeleteTree (OscNode);
    goto error_handler;
  }

error_handler:
  // Cleanup
  Status1 = AmlDeleteTree (OscTemplateRoot);
  if (EFI_ERROR (Status1)) {
    DEBUG ((
      DEBUG_ERROR,
      "ERROR: SSDT-PCI-OSC: Failed to cleanup AML tree."
      " Status = %r\n",
      Status1
      ));
    // If Status was success but we failed to delete the AML Tree
    // return Status1 else return the original error code, i.e. Status.
    if (!EFI_ERROR (Status)) {
      return Status1;
    }
  }

  return Status;
}
