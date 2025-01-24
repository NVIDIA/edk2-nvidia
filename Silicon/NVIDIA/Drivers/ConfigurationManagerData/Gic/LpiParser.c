/** @file
  Lpi parser.

  SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Library/NvCmObjectDescUtility.h>
#include "GicParser.h"
#include <Library/DeviceTreeHelperLib.h>
#include <Library/NVIDIADebugLib.h>
#include <Library/BaseMemoryLib.h>

#define LPI_ARCH_FLAG_CORE_CONTEXT_LOST   BIT0
#define LPI_ARCH_FLAG_TRACE_CONTEXT_LOST  BIT1
#define LPI_ARCH_FLAG_GICR                BIT2
#define LPI_ARCH_FLAG_GICD                BIT3

/** Lpi parser function.

  The following structures are populated:
  - EArmObjLpiInfo
  - EArchCommonObjCmRef (LpiTokens)

  A parser parses a Device Tree to populate a specific CmObj type. None,
  one or many CmObj can be created by the parser.
  The created CmObj are then handed to the parser's caller through the
  HW_INFO_ADD_OBJECT interface.
  This can also be a dispatcher. I.e. a function that not parsing a
  Device Tree but calling other parsers.

  @param [in]  ParserHandle    A handle to the parser instance.
  @param [in]  FdtBranch       When searching for DT node name, restrict
                               the search to this Device Tree branch.
  @param [out] Token           The token for the array of object tokens.

  @retval EFI_SUCCESS             The function completed successfully.
  @retval EFI_ABORTED             An error occurred.
  @retval EFI_INVALID_PARAMETER   Invalid parameter.
  @retval EFI_NOT_FOUND           Not found.
  @retval EFI_UNSUPPORTED         Unsupported.
**/
EFI_STATUS
EFIAPI
LpiParser (
  IN  CONST HW_INFO_PARSER_HANDLE  ParserHandle,
  IN        INT32                  FdtBranch,
  OUT CM_OBJECT_TOKEN              *Token OPTIONAL
  )
{
  EFI_STATUS         Status;
  CM_OBJ_DESCRIPTOR  LpiInfoDesc;
  UINTN              LpiInfoSize;

  UINT32                   Index;
  UINT32                   *CpuIdleHandles;
  UINT32                   NumberOfCpuIdles;
  UINT32                   NumberOfLpiStates;
  CM_ARCH_COMMON_LPI_INFO  *LpiInfo;
  VOID                     *DeviceTreeBase;
  INT32                    NodeOffset;
  CONST VOID               *Property;
  UINT32                   PropertyLen;
  UINT32                   WakeupLatencyUs;
  UINT32                   ExitLatencyUs;
  UINT32                   SuspendAddr;

  CpuIdleHandles = NULL;
  LpiInfo        = NULL;

  if (ParserHandle == NULL) {
    ASSERT (ParserHandle != NULL);
    Status = EFI_INVALID_PARAMETER;
    goto CleanupAndReturn;
  }

  // Build LPI stuctures
  NumberOfCpuIdles = 0;

  // Get the list of idle nodes
  Status = GetMatchingEnabledDeviceTreeNodes ("arm,idle-state", NULL, &NumberOfCpuIdles);
  if (Status != EFI_BUFFER_TOO_SMALL) {
    NumberOfCpuIdles = 0;
  } else {
    CpuIdleHandles = AllocateZeroPool (sizeof (UINT32) * NumberOfCpuIdles);
    if (CpuIdleHandles == NULL) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to allocate array for cpuidle cores\r\n", __FUNCTION__));
      Status = EFI_OUT_OF_RESOURCES;
      goto CleanupAndReturn;
    }

    Status = GetMatchingEnabledDeviceTreeNodes ("arm,idle-state", CpuIdleHandles, &NumberOfCpuIdles);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Failed to get cpuidle cores %r\r\n", __FUNCTION__, Status));
      goto CleanupAndReturn;
    }
  }

  if (NumberOfCpuIdles == 0) {
    Status = GetMatchingEnabledDeviceTreeNodes ("nvidia,tegra194-cpuidle-core", NULL, &NumberOfCpuIdles);
    if (Status != EFI_BUFFER_TOO_SMALL) {
      NumberOfCpuIdles = 0;
    } else {
      CpuIdleHandles = AllocateZeroPool (sizeof (UINT32) * NumberOfCpuIdles);
      if (CpuIdleHandles == NULL) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to allocate array for cpuidle cores\r\n", __FUNCTION__));
        Status = EFI_OUT_OF_RESOURCES;
        goto CleanupAndReturn;
      }

      Status = GetMatchingEnabledDeviceTreeNodes ("nvidia,tegra194-cpuidle-core", CpuIdleHandles, &NumberOfCpuIdles);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Failed to get cpuidle cores %r\r\n", __FUNCTION__, Status));
        goto CleanupAndReturn;
      }
    }
  }

  LpiInfoSize = sizeof (CM_ARCH_COMMON_LPI_INFO) * (NumberOfCpuIdles + 1);       // 1 extra for WFI state
  LpiInfo     = AllocateZeroPool (LpiInfoSize);
  if (LpiInfo == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate array for lpi info\r\n", __FUNCTION__));
    Status = EFI_OUT_OF_RESOURCES;
    goto CleanupAndReturn;
  }

  // Create WFI entry
  LpiInfo[0].MinResidency                          = 1;
  LpiInfo[0].WorstCaseWakeLatency                  = 1;
  LpiInfo[0].Flags                                 = 1;
  LpiInfo[0].ArchFlags                             = 0;
  LpiInfo[0].EnableParentState                     = FALSE;
  LpiInfo[0].IsInteger                             = FALSE;
  LpiInfo[0].RegisterEntryMethod.AccessSize        = 3;
  LpiInfo[0].RegisterEntryMethod.Address           = 0xFFFFFFFF;
  LpiInfo[0].RegisterEntryMethod.AddressSpaceId    = EFI_ACPI_6_4_FUNCTIONAL_FIXED_HARDWARE;
  LpiInfo[0].RegisterEntryMethod.RegisterBitOffset = 0;
  LpiInfo[0].RegisterEntryMethod.RegisterBitWidth  = 0x20;
  CopyMem (LpiInfo[0].StateName, "WFI", sizeof ("WFI"));

  NumberOfLpiStates = 1;
  for (Index = 0; Index < NumberOfCpuIdles; Index++) {
    Status = GetDeviceTreeNode (CpuIdleHandles[Index], &DeviceTreeBase, &NodeOffset);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to get idle state node - %r\r\n", Status));
      continue;
    }

    Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "arm,psci-suspend-param", &SuspendAddr);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to get psci-suspend-param\r\n"));
      continue;
    }

    LpiInfo[NumberOfLpiStates].RegisterEntryMethod.Address = SuspendAddr;

    Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "min-residency-us", &LpiInfo[NumberOfLpiStates].MinResidency);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "Failed to get min-residency-us\r\n"));
      continue;
    }

    Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "wakeup-latency-us", &WakeupLatencyUs);
    if (EFI_ERROR (Status)) {
      Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "entry-latency-us", &WakeupLatencyUs);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Failed to get entry-latency-us\r\n"));
        continue;
      }

      Status = DeviceTreeGetNodePropertyValue32 (NodeOffset, "exit-latency-us", &ExitLatencyUs);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "Failed to get exit-latency-us\r\n"));
        continue;
      }

      WakeupLatencyUs += ExitLatencyUs;
    }

    LpiInfo[NumberOfLpiStates].WorstCaseWakeLatency = WakeupLatencyUs;
    LpiInfo[NumberOfLpiStates].Flags                = 1;
    Status                                          = DeviceTreeGetNodeProperty (NodeOffset, "local-timer-stop", NULL, NULL);
    if (Status == EFI_NOT_FOUND) {
      LpiInfo[NumberOfLpiStates].ArchFlags = 0;
    } else {
      LpiInfo[NumberOfLpiStates].ArchFlags = (LPI_ARCH_FLAG_CORE_CONTEXT_LOST |
                                              LPI_ARCH_FLAG_TRACE_CONTEXT_LOST |
                                              LPI_ARCH_FLAG_GICR |
                                              LPI_ARCH_FLAG_GICD);
    }

    LpiInfo[NumberOfLpiStates].EnableParentState                     = 0;
    LpiInfo[NumberOfLpiStates].IsInteger                             = FALSE;
    LpiInfo[NumberOfLpiStates].RegisterEntryMethod.AccessSize        = 3;
    LpiInfo[NumberOfLpiStates].RegisterEntryMethod.AddressSpaceId    = EFI_ACPI_6_4_FUNCTIONAL_FIXED_HARDWARE;
    LpiInfo[NumberOfLpiStates].RegisterEntryMethod.RegisterBitOffset = 0;
    LpiInfo[NumberOfLpiStates].RegisterEntryMethod.RegisterBitWidth  = 0x20;
    Status                                                           = DeviceTreeGetNodeProperty (NodeOffset, "idle-state-name", &Property, &PropertyLen);
    if (!EFI_ERROR (Status)) {
      CopyMem (LpiInfo[NumberOfLpiStates].StateName, Property, MIN (sizeof (LpiInfo[NumberOfLpiStates].StateName), PropertyLen));
      LpiInfo[NumberOfLpiStates].StateName[sizeof (LpiInfo[NumberOfLpiStates].StateName) - 1] = '\0';
    }

    NumberOfLpiStates++;
  }

  LpiInfoDesc.ObjectId = CREATE_CM_ARCH_COMMON_OBJECT_ID (EArchCommonObjLpiInfo);
  LpiInfoDesc.Size     = sizeof (CM_ARCH_COMMON_LPI_INFO) * NumberOfLpiStates;
  LpiInfoDesc.Count    = NumberOfLpiStates;
  LpiInfoDesc.Data     = LpiInfo;

  // Add the LpiInfo and get a token map token
  Status = NvAddMultipleCmObjWithCmObjRef (ParserHandle, &LpiInfoDesc, NULL, Token);
  ASSERT_EFI_ERROR (Status);

CleanupAndReturn:
  FREE_NON_NULL (LpiInfo);
  FREE_NON_NULL (CpuIdleHandles);
  return Status;
}
