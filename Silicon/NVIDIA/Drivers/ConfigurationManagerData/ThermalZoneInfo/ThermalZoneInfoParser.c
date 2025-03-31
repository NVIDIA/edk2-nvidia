/** @file
  Patches the SSDT with ThermalZoneInfo

  SPDX-FileCopyrightText: Copyright (c) 2019-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2017 - 2018, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include "ThermalZoneInfoParser.h"
#include "../ConfigurationManagerDataRepoLib.h"

#include <Library/AmlLib/AmlLib.h>
#include <Library/ConfigurationManagerDataLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/FloorSweepingLib.h>
#include <Library/MpCoreInfoLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/PcdLib.h>
#include <Library/PrintLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <TH500/TH500Definitions.h>

#define CELSIUS_TO_KELVIN(Temp)  (Temp + 2732)

#define MAX_DEVICES_PER_THERMAL_ZONE  10
#define MAX_UNICODE_STRING_LEN        128

typedef struct {
  UINT32          ZoneId;
  BOOLEAN         PassiveSupported;
  BOOLEAN         CriticalSupported;
  UINT32          *PassiveCpus;
  CONST CHAR16    *SocketFormatString;
} THERMAL_ZONE_DATA;

// #include "BpmpSsdtSocket0_TH500.hex"
extern unsigned char  bpmpssdtsocket0_th500_aml_code[];
// #include "BpmpSsdtSocket1_TH500.hex"
extern unsigned char  bpmpssdtsocket1_th500_aml_code[];
// #include "BpmpSsdtSocket2_TH500.hex"
extern unsigned char  bpmpssdtsocket2_th500_aml_code[];
// #include "BpmpSsdtSocket3_TH500.hex"
extern unsigned char                bpmpssdtsocket3_th500_aml_code[];
STATIC EFI_ACPI_DESCRIPTION_HEADER  *AcpiBpmpTableArray[] = {
  (EFI_ACPI_DESCRIPTION_HEADER *)bpmpssdtsocket0_th500_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)bpmpssdtsocket1_th500_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)bpmpssdtsocket2_th500_aml_code,
  (EFI_ACPI_DESCRIPTION_HEADER *)bpmpssdtsocket3_th500_aml_code
};

STATIC UINT32  ThermalZoneCpu0_List[]  = { 0x00, 0x02, 0x04, 0x0E, MAX_UINT32 };
STATIC UINT32  ThermalZoneCpu1_List[]  = { 0x06, 0x08, 0x0A, 0x0C, 0x1A, MAX_UINT32 };
STATIC UINT32  ThermalZoneCpu2_List[]  = { 0x05, 0x12, 0x13, 0x1C, 0x20, 0x21, 0x1D, 0x03, 0x10, 0x11, 0x1E, 0x1F, MAX_UINT32 };
STATIC UINT32  ThermalZoneCpu3_List[]  = { 0x07, 0x14, 0x15, 0x22, 0x23, 0x0B, 0x18, 0x19, 0x26, 0x27, 0x28, 0x29, 0x09, 0x16, 0x17, 0x24, 0x25, MAX_UINT32 };
STATIC UINT32  ThermalZoneSoc0_List[]  = { 0x2A, 0x2B, 0x2D, 0x2C, 0x3B, 0x3A, 0x49, 0x2F, 0x2E, 0x3D, 0x3C, 0x4B, MAX_UINT32 };
STATIC UINT32  ThermalZoneSoc1_List[]  = { 0x31, 0x30, 0x3F, 0x3E, 0x4D, 0x33, 0x32, 0x41, 0x40, 0x4F, 0x35, 0x34, 0x43, 0x42, 0x51, 0x36, 0x37, MAX_UINT32 };
STATIC UINT32  ThermalZoneSoc2_List[]  = { 0x48, 0x38, 0x46, 0x4A, MAX_UINT32 };
STATIC UINT32  ThermalZoneSoc3_List[]  = { 0x4C, 0x4E, 0x50, 0x44, 0x52, MAX_UINT32 };
STATIC UINT32  ThermalZoneSoc4_List[]  = { MAX_UINT32 };
STATIC UINT32  ThermalZoneTjMax_List[] = { 0x00, MAX_UINT32 };

STATIC CONST THERMAL_ZONE_DATA  ThermalZoneData[] = {
  { TH500_THERMAL_ZONE_CPU0,   !FixedPcdGetBool (PcdUseSinglePassiveThermalZone), TRUE, ThermalZoneCpu0_List,  L"Thermal Zone Skt%d CPU0"  },
  { TH500_THERMAL_ZONE_CPU1,   !FixedPcdGetBool (PcdUseSinglePassiveThermalZone), TRUE, ThermalZoneCpu1_List,  L"Thermal Zone Skt%d CPU1"  },
  { TH500_THERMAL_ZONE_CPU2,   !FixedPcdGetBool (PcdUseSinglePassiveThermalZone), TRUE, ThermalZoneCpu2_List,  L"Thermal Zone Skt%d CPU2"  },
  { TH500_THERMAL_ZONE_CPU3,   !FixedPcdGetBool (PcdUseSinglePassiveThermalZone), TRUE, ThermalZoneCpu3_List,  L"Thermal Zone Skt%d CPU3"  },
  { TH500_THERMAL_ZONE_SOC0,   !FixedPcdGetBool (PcdUseSinglePassiveThermalZone), TRUE, ThermalZoneSoc0_List,  L"Thermal Zone Skt%d SOC0"  },
  { TH500_THERMAL_ZONE_SOC1,   !FixedPcdGetBool (PcdUseSinglePassiveThermalZone), TRUE, ThermalZoneSoc1_List,  L"Thermal Zone Skt%d SOC1"  },
  { TH500_THERMAL_ZONE_SOC2,   !FixedPcdGetBool (PcdUseSinglePassiveThermalZone), TRUE, ThermalZoneSoc2_List,  L"Thermal Zone Skt%d SOC2"  },
  { TH500_THERMAL_ZONE_SOC3,   !FixedPcdGetBool (PcdUseSinglePassiveThermalZone), TRUE, ThermalZoneSoc3_List,  L"Thermal Zone Skt%d SOC3"  },
  { TH500_THERMAL_ZONE_SOC4,   !FixedPcdGetBool (PcdUseSinglePassiveThermalZone), TRUE, ThermalZoneSoc4_List,  L"Thermal Zone Skt%d SOC4"  },
  { TH500_THERMAL_ZONE_TJ_MAX, FixedPcdGetBool (PcdUseSinglePassiveThermalZone),  TRUE, ThermalZoneTjMax_List, L"Thermal Zone Skt%d TJMax" },
  { TH500_THERMAL_ZONE_TJ_MIN, FALSE,                                             TRUE, NULL,                  L"Thermal Zone Skt%d TJMin" },
  { TH500_THERMAL_ZONE_TJ_AVG, FALSE,                                             TRUE, NULL,                  L"Thermal Zone Skt%d TJAvg" }
};

/** Thermal Zone patcher function.

  The SSDT table is potentially patched with the following information:
  \_SB.BPM*.TEMP
  \_SB_.C000.C*.C* or \_SB_.C*.C*
  _SB_.TZL*
  TZ*

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
ThermalZoneInfoParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch
  )
{
  EFI_STATUS                   Status;
  INT32                        NodeOffset;
  UINT32                       SocketId;
  UINT32                       PsvTemp;
  UINT32                       CrtTemp;
  UINT32                       ThermCoeff1;
  UINT32                       ThermCoeff2;
  UINT32                       FastSampPeriod;
  AML_ROOT_NODE_HANDLE         RootNode;
  AML_OBJECT_NODE_HANDLE       ScopeNode;
  AML_OBJECT_NODE_HANDLE       LimitNode;
  AML_OBJECT_NODE_HANDLE       TZNode;
  AML_OBJECT_NODE_HANDLE       Node;
  EFI_ACPI_DESCRIPTION_HEADER  *BpmpTable;
  CHAR8                        ThermalZoneString[ACPI_PATCH_MAX_PATH];
  CHAR8                        LimitString[ACPI_PATCH_MAX_PATH];
  CHAR16                       UnicodeString[MAX_UNICODE_STRING_LEN];
  UINTN                        ThermalZoneIndex;
  UINTN                        ThermalZoneUid = 0;
  UINTN                        DevicesPerZone;
  UINTN                        DeviceIndex;
  UINTN                        TotalZones;
  UINTN                        DevicesPerSubZone;
  UINTN                        SubZoneIndex;
  UINTN                        CurrentDevice;
  UINT32                       CurrentCluster;
  UINT64                       CoreId;
  UINT32                       MaxSocket;
  BOOLEAN                      IsMultiSocketSystem;
  CM_STD_OBJ_ACPI_TABLE_INFO   NewAcpiTable;

  if (ParserHandle == NULL) {
    ASSERT (0);
    return EFI_INVALID_PARAMETER;
  }

  if (TegraGetPlatform () != TEGRA_PLATFORM_SILICON) {
    DEBUG ((DEBUG_ERROR, "%a: Skipping parser because platform isn't Silicon\n", __FUNCTION__));
    return EFI_SUCCESS;
  }

  Status = DeviceTreeGetNodeByPath ("/firmware/acpi", &NodeOffset);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Got %r trying to get /firmware/acpi node from DTB\n", __FUNCTION__, Status));
    return EFI_SUCCESS;
  }

  PsvTemp        = MAX_UINT16;
  CrtTemp        = MAX_UINT16;
  ThermCoeff1    = MAX_UINT8;
  ThermCoeff2    = MAX_UINT8;
  FastSampPeriod = MAX_UINT32;

  Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "override-thermal-zone-passive-cooling-trip-point-temp", &PsvTemp);
  if (EFI_ERROR (Status) || (PsvTemp == MAX_UINT16)) {
    PsvTemp = TH500_THERMAL_ZONE_PSV;
  }

  PsvTemp = CELSIUS_TO_KELVIN (PsvTemp);

  Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "override-thermal-zone-critical-point-temp", &CrtTemp);
  if (EFI_ERROR (Status) || (CrtTemp == MAX_UINT16)) {
    CrtTemp = TH500_THERMAL_ZONE_CRT;
  }

  CrtTemp = CELSIUS_TO_KELVIN (CrtTemp);

  Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "override-thermal-coefficient-tc1", &ThermCoeff1);
  if (EFI_ERROR (Status) || (ThermCoeff1 == MAX_UINT8)) {
    ThermCoeff1 = TH500_THERMAL_ZONE_TC1;
  }

  Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "override-thermal-coefficient-tc2", &ThermCoeff2);
  if (EFI_ERROR (Status) || (ThermCoeff2 == MAX_UINT8)) {
    ThermCoeff2 = TH500_THERMAL_ZONE_TC2;
  }

  Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "override-thermal-fast-sampling-period", &FastSampPeriod);
  if (EFI_ERROR (Status) || (FastSampPeriod == MAX_UINT32)) {
    FastSampPeriod = TH500_THERMAL_ZONE_TFP;
  }

  Status = MpCoreInfoGetPlatformInfo (NULL, &MaxSocket, NULL, NULL, NULL);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to get platform info - %r\r\n", __FUNCTION__, Status));
    return Status;
  }

  IsMultiSocketSystem = (MaxSocket >= 1);

  MPCORE_FOR_EACH_ENABLED_SOCKET (SocketId) {
    Status = AmlParseDefinitionBlock (AcpiBpmpTableArray[SocketId], &RootNode);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to open BPMP socket ACPI table - %r\r\n", Status));
      return Status;
    }

    Status = AmlFindNode (RootNode, "_SB", &ScopeNode);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to find scope node\r\n"));
      ASSERT_EFI_ERROR (Status);
      return Status;
    }

    AsciiSPrint (LimitString, sizeof (LimitString), "_SB_.TZL%01x", SocketId);
    Status = AmlFindNode (RootNode, LimitString, &LimitNode);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to find node %a\r\n", LimitString));
      ASSERT_EFI_ERROR (Status);
      return Status;
    }

    Status = AmlDetachNode (LimitNode);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to detach node %a\r\n", LimitString));
      ASSERT_EFI_ERROR (Status);
      return Status;
    }

    for (ThermalZoneIndex = 0; ThermalZoneIndex < ARRAY_SIZE (ThermalZoneData); ThermalZoneIndex++) {
      DevicesPerZone = 0;
      if (ThermalZoneData[ThermalZoneIndex].PassiveSupported &&
          (ThermalZoneData[ThermalZoneIndex].PassiveCpus != NULL))
      {
        DevicesPerZone = 0;
        while (ThermalZoneData[ThermalZoneIndex].PassiveCpus[DevicesPerZone] != MAX_UINT32) {
          DevicesPerZone++;
        }
      }

      TotalZones = (DevicesPerZone + (MAX_DEVICES_PER_THERMAL_ZONE - 1)) / MAX_DEVICES_PER_THERMAL_ZONE;
      if (TotalZones == 0) {
        TotalZones = 1;
      }

      CurrentDevice     = 0;
      DevicesPerSubZone = DevicesPerZone / TotalZones;
      if ((DevicesPerZone % TotalZones) != 0) {
        DevicesPerSubZone++;
      }

      if (ThermalZoneData[ThermalZoneIndex].SocketFormatString != NULL) {
        UnicodeSPrint (
          UnicodeString,
          sizeof (UnicodeString),
          ThermalZoneData[ThermalZoneIndex].SocketFormatString,
          SocketId
          );
      }

      for (DeviceIndex = 0; DeviceIndex < TotalZones; DeviceIndex++) {
        AsciiSPrint (ThermalZoneString, sizeof (ThermalZoneString), "TZ%02x", ThermalZoneUid);
        ThermalZoneUid++;
        Status = AmlCodeGenThermalZone (ThermalZoneString, ScopeNode, &TZNode);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "Failed to create thermal zone - %r\r\n", Status));
          ASSERT_EFI_ERROR (Status);
          return Status;
        }

        AsciiSPrint (ThermalZoneString, sizeof (ThermalZoneString), "\\_SB.BPM%0u.TEMP", SocketId);
        Status = AmlCodeGenMethodRetNameStringIntegerArgument (
                   "_TMP",
                   ThermalZoneString,
                   0,
                   FALSE,
                   0,
                   ThermalZoneData[ThermalZoneIndex].ZoneId,
                   TZNode,
                   NULL
                   );
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "Failed to create TMP method - %r\r\n", Status));
          ASSERT_EFI_ERROR (Status);
          return Status;
        }

        Status = AmlCodeGenNameInteger ("_TZP", TEMP_POLL_TIME_100MS, TZNode, NULL);
        if (EFI_ERROR (Status)) {
          DEBUG ((DEBUG_ERROR, "Failed to create _TZP node - %r\r\n", Status));
          ASSERT_EFI_ERROR (Status);
          return Status;
        }

        if (ThermalZoneData[ThermalZoneIndex].SocketFormatString != NULL) {
          Status = AmlCodeGenNameUnicodeString ("_STR", UnicodeString, TZNode, NULL);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "Failed to create _STR node - %r\r\n", Status));
            ASSERT_EFI_ERROR (Status);
            return Status;
          }
        }

        if (ThermalZoneData[ThermalZoneIndex].CriticalSupported) {
          Status = AmlCodeGenNameInteger ("_CRT", CrtTemp, TZNode, NULL);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "Failed to create _CRT node - %r\r\n", Status));
            ASSERT_EFI_ERROR (Status);
            return Status;
          }
        }

        if (ThermalZoneData[ThermalZoneIndex].PassiveSupported && (DevicesPerZone != 0)) {
          Status = AmlCodeGenNameInteger ("_PSV", PsvTemp, TZNode, NULL);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "Failed to create _PSV node - %r\r\n", Status));
            ASSERT_EFI_ERROR (Status);
            return Status;
          }

          Status = AmlCodeGenNameInteger ("_TC1", ThermCoeff1, TZNode, NULL);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "Failed to create _TC1 node - %r\r\n", Status));
            ASSERT_EFI_ERROR (Status);
            return Status;
          }

          Status = AmlCodeGenNameInteger ("_TC2", ThermCoeff2, TZNode, NULL);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "Failed to create _TC2 node - %r\r\n", Status));
            ASSERT_EFI_ERROR (Status);
            return Status;
          }

          Status = AmlCodeGenNameInteger ("_TSP", TH500_THERMAL_ZONE_TSP, TZNode, NULL);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "Failed to create _TSP node - %r\r\n", Status));
            ASSERT_EFI_ERROR (Status);
            return Status;
          }

          Status = AmlCodeGenNameInteger ("_TFP", FastSampPeriod, TZNode, NULL);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "-->Failed to create _TFP node - %r\r\n", Status));
            ASSERT_EFI_ERROR (Status);
            return Status;
          }

          Status = AmlCodeGenNamePackage ("_PSL", TZNode, &Node);
          if (EFI_ERROR (Status)) {
            DEBUG ((DEBUG_ERROR, "Failed to create _PSL node - %r\r\n", Status));
            ASSERT_EFI_ERROR (Status);
            return Status;
          }

          for (SubZoneIndex = 0; SubZoneIndex < DevicesPerSubZone; SubZoneIndex++) {
            if (ThermalZoneData[ThermalZoneIndex].ZoneId == TH500_THERMAL_ZONE_TJ_MAX ) {
              Status = MpCoreInfoGetSocketInfo (SocketId, NULL, NULL, NULL, NULL, &CoreId);
              if (EFI_ERROR (Status)) {
                DEBUG ((DEBUG_ERROR, "Failed to get socket info - %r\r\n", Status));
                ASSERT_EFI_ERROR (Status);
                return Status;
              }

              Status = MpCoreInfoGetProcessorLocation (CoreId, NULL, &CurrentCluster, NULL, NULL);
              if (EFI_ERROR (Status)) {
                DEBUG ((DEBUG_ERROR, "Failed to get processor location - %r\r\n", Status));
                ASSERT_EFI_ERROR (Status);
                return Status;
              }
            } else {
              CurrentCluster = ThermalZoneData[ThermalZoneIndex].PassiveCpus[CurrentDevice];
              if (CurrentCluster == MAX_UINT32) {
                break;
              }

              Status = MpCoreInfoGetProcessorIdFromLocation (SocketId, CurrentCluster, 0, 0, &CoreId);
              if (EFI_ERROR (Status)) {
                CurrentDevice++;
                continue;
              }

              Status = MpCoreInfoIsProcessorEnabled (CoreId);
              if (EFI_ERROR (Status)) {
                CurrentDevice++;
                continue;
              }
            }

            if (IsMultiSocketSystem) {
              AsciiSPrint (ThermalZoneString, sizeof (ThermalZoneString), "\\_SB_.C000.C%03x.C%03x", SocketId, CurrentCluster);
            } else {
              AsciiSPrint (ThermalZoneString, sizeof (ThermalZoneString), "\\_SB_.C%03x.C%03x", SocketId, CurrentCluster);
            }

            Status = AmlAddNameStringToNamedPackage (ThermalZoneString, Node);
            if (EFI_ERROR (Status)) {
              DEBUG ((DEBUG_ERROR, "Failed to add %a to _PSL node - %r\r\n", ThermalZoneString, Status));
              ASSERT_EFI_ERROR (Status);
              return Status;
            }

            CurrentDevice++;
          }
        }
      }
    }

    Status = AmlAttachNode (RootNode, LimitNode);
    if (EFI_ERROR (Status)) {
      // Free the detached node.
      AmlDeleteTree (LimitNode);
      DEBUG ((DEBUG_ERROR, "Failed to reattach node %a\r\n", LimitString));
      ASSERT_EFI_ERROR (Status);
      return Status;
    }

    Status = AmlSerializeDefinitionBlock (RootNode, &BpmpTable);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to Serialize BPMP socket ACPI table - %r\r\n", Status));
      return Status;
    }

    NewAcpiTable.AcpiTableSignature = EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_SIGNATURE;
    NewAcpiTable.AcpiTableRevision  = EFI_ACPI_6_4_SECONDARY_SYSTEM_DESCRIPTION_TABLE_REVISION;
    NewAcpiTable.TableGeneratorId   = CREATE_STD_ACPI_TABLE_GEN_ID (EStdAcpiTableIdSsdt);
    NewAcpiTable.AcpiTableData      = BpmpTable;
    NewAcpiTable.OemTableId         = PcdGet64 (PcdAcpiDefaultOemTableId);
    NewAcpiTable.OemRevision        = FixedPcdGet64 (PcdAcpiDefaultOemRevision);
    NewAcpiTable.MinorRevision      = 0;

    Status = NvAddAcpiTableGenerator (ParserHandle, &NewAcpiTable);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Got %r trying to add the BPMP SSDT table for Socket %u\n", __FUNCTION__, Status, SocketId));
      return Status;
    }
  }

  return EFI_SUCCESS;
}

REGISTER_PARSER_FUNCTION (ThermalZoneInfoParser, NULL)
