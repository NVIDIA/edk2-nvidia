/** @file

  SMMUv3 Driver

  SPDX-FileCopyrightText: Copyright (c) 2025, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/ArmLib.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/IoLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include "SmmuV3DxePrivate.h"

NVIDIA_COMPATIBILITY_MAPPING  gDeviceCompatibilityMap[] = {
  { "arm,smmu-v3", &gNVIDIANonDiscoverableSmmuV3DeviceGuid },
  { NULL,          NULL                                    }
};

NVIDIA_DEVICE_DISCOVERY_CONFIG  gDeviceDiscoverDriverConfig = {
  .DriverName = L"NVIDIA Smmu V3 Controller Driver"
};

STATIC
EFI_STATUS
EFIAPI
ResetSmmuV3Controller (
  IN  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private
  )
{
  UINT32  GbpSetting;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Set the controller in global bypass mode
  GbpSetting = BIT_FIELD_SET (1U, SMMU_V3_GBPA_UPDATE_MASK, SMMU_V3_GBPA_UPDATE_SHIFT);
  GbpSetting = GbpSetting | BIT_FIELD_SET (0, SMMU_V3_GBPA_ABORT_MASK, SMMU_V3_GBPA_ABORT_SHIFT);
  GbpSetting = GbpSetting | BIT_FIELD_SET (0, SMMU_V3_GBPA_INSTCFG_MASK, SMMU_V3_GBPA_INSTCFG_SHIFT);
  GbpSetting = GbpSetting | BIT_FIELD_SET (0, SMMU_V3_GBPA_PRIVCFG_MASK, SMMU_V3_GBPA_PRIVCFG_SHIFT);
  GbpSetting = GbpSetting | BIT_FIELD_SET (1, SMMU_V3_GBPA_SHCFG_MASK, SMMU_V3_GBPA_SHCFG_SHIFT);
  GbpSetting = GbpSetting | BIT_FIELD_SET (0, SMMU_V3_GBPA_ALLOCFG_MASK, SMMU_V3_GBPA_ALLOCFG_SHIFT);
  GbpSetting = GbpSetting | BIT_FIELD_SET (0, SMMU_V3_GBPA_MTCFG_MASK, SMMU_V3_GBPA_MTCFG_SHIFT);
  MmioWrite32 (Private->BaseAddress + SMMU_V3_GBPA_OFFSET, GbpSetting);

  // Wait for the controller to enter global bypass mode
  gBS->Stall (10000);
  if (((MmioRead32 (Private->BaseAddress + SMMU_V3_GBPA_OFFSET) >> SMMU_V3_GBPA_UPDATE_SHIFT) & SMMU_V3_GBPA_UPDATE_MASK) == 1) {
    return EFI_TIMEOUT;
  }

  MmioBitFieldWrite32 (
    Private->BaseAddress + SMMU_V3_CR0_OFFSET,
    SMMU_V3_CR0_SMMUEN_BIT,
    SMMU_V3_CR0_SMMUEN_BIT,
    SMMU_V3_DISABLE
    );

  // Wait for the controller to disable SMMU operation
  gBS->Stall (10000);
  if (((MmioRead32 (Private->BaseAddress + SMMU_V3_CR0ACK_OFFSET) >> SMMU_V3_CR0ACK_SMMUEN_SHIFT) & SMMU_V3_CR0ACK_SMMUEN_MASK) != SMMU_V3_DISABLE) {
    return EFI_TIMEOUT;
  }

  return EFI_SUCCESS;
}

/**
  Identify SMMUv3 controller features from registers and populate values in the
  SMMU_V3_CONTROLLER_FEATURES structure.

  @param[in]  Private       Pointer to the SMMU_V3_CONTROLLER_PRIVATE_DATA instance.

  @retval EFI_SUCCESS              The SMMUv3 features were identified successfully.
  @retval EFI_INVALID_PARAMETER    Private is NULL.
  @retval EFI_DEVICE_ERROR         The SMMUv3 hardware features identification failed.
  @retval EFI_UNSUPPORTED          The SMMUv3 driver does not support this feature.

 **/
STATIC
EFI_STATUS
EFIAPI
IdentifySmmuV3ControllerFeatures (
  IN  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private
  )
{
  UINT32  Idr0;
  UINT32  ArchVersion;
  UINT32  XlatFormat;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ArchVersion = MmioRead32 (Private->BaseAddress + SMMU_V3_AIDR_OFFSET);
  ArchVersion = BIT_FIELD_GET (ArchVersion, SMMU_V3_AIDR_ARCH_REV_MASK, SMMU_V3_AIDR_ARCH_REV_SHIFT);

  if (ArchVersion > 2) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid architecture version\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  Private->Features.MinorVersion = ArchVersion;

  Idr0 = MmioRead32 (Private->BaseAddress + SMMU_V3_IDR0_OFFSET);
  if ((BIT_FIELD_GET (Idr0, SMMU_V3_IDR0_ST_LEVEL_MASK, SMMU_V3_IDR0_ST_LEVEL_SHIFT) == SMMU_V3_LINEAR_STR_TABLE) ||
      (BIT_FIELD_GET (Idr0, SMMU_V3_IDR0_ST_LEVEL_MASK, SMMU_V3_IDR0_ST_LEVEL_SHIFT) == SMMU_V3_TWO_LVL_STR_TABLE))
  {
    Private->Features.LinearStrTable = TRUE;
  } else {
    DEBUG ((DEBUG_ERROR, "%a: Invalid value for Multi-level Stream table support\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  Private->Features.Endian = BIT_FIELD_GET (Idr0, SMMU_V3_IDR0_TTENDIAN_MASK, SMMU_V3_IDR0_TTENDIAN_SHIFT);
  if (Private->Features.Endian == SMMU_V3_RES_ENDIAN) {
    DEBUG ((DEBUG_ERROR, "%a: Unsupported endianness for translation table walks\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  if (BIT_FIELD_GET (Idr0, SMMU_V3_IDR0_BTM_MASK, SMMU_V3_IDR0_BTM_SHIFT)) {
    Private->Features.BroadcastTlb = TRUE;
  } else {
    Private->Features.BroadcastTlb = FALSE;
    DEBUG ((DEBUG_INFO, "%a: Broadcast TLB maintenance not supported in hardware\n", __FUNCTION__));
  }

  XlatFormat = BIT_FIELD_GET (Idr0, SMMU_V3_IDR0_TTF_MASK, SMMU_V3_IDR0_TTF_SHIFT);
  switch (XlatFormat) {
    case SMMU_V3_AARCH32_TTF:
      DEBUG ((DEBUG_ERROR, "%a: AArch32 translation table format not supported\n", __FUNCTION__));
      return EFI_UNSUPPORTED;
    case SMMU_V3_AARCH64_TTF:
    case SMMU_V3_AARCH32_64_TTF:
      break;
    case SMMU_V3_RES_TTF:
    default:
      DEBUG ((DEBUG_ERROR, "%a: Unsupported translation table format\n", __FUNCTION__));
      return EFI_UNSUPPORTED;
  }

  Private->Features.XlatFormat = XlatFormat;
  Private->Features.XlatStages = BIT_FIELD_GET (Idr0, SMMU_V3_IDR0_XLAT_STG_MASK, SMMU_V3_IDR0_XLAT_STG_SHIFT);

  return EFI_SUCCESS;
}

/**
  Configure SMMUv3 controller translation address bits values in the
  SMMU_V3_CONTROLLER_FEATURES structure. This function also set cacheability
  and shareability attributes for Table and Queue access.

  @param[in]  Private       Pointer to the SMMU_V3_CONTROLLER_PRIVATE_DATA instance.

  @retval EFI_SUCCESS              The SMMUv3 controller translation support configured successfully.
  @retval EFI_INVALID_PARAMETER    Private is NULL.
  @retval EFI_DEVICE_ERROR         The SMMUv3 controller translation support configuration failed.

 **/
STATIC
EFI_STATUS
EFIAPI
ConfigureSmmuV3ControllerXlatSupport (
  IN  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private
  )
{
  UINT32   Idr5;
  UINT32   Cr1Setting;
  UINT32   Cr2Setting;
  UINT64   Oas;
  UINT64   OasBits;
  UINT64   IasAarch32;
  UINT64   IasAarch64;
  BOOLEAN  TtfAarch32;
  BOOLEAN  TtfAarch64;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Idr5    = MmioRead32 (Private->BaseAddress + SMMU_V3_IDR5_OFFSET);
  OasBits = BIT_FIELD_GET (Idr5, SMMU_V3_IDR5_OAS_MASK, SMMU_V3_IDR5_OAS_SHIFT);

  IasAarch32 = 0;
  IasAarch64 = 0;

  TtfAarch32 = FALSE;
  TtfAarch64 = TRUE;

  if (Private->Features.XlatFormat == SMMU_V3_AARCH32_64_TTF) {
    TtfAarch32 = TRUE;
  }

  switch (OasBits) {
    case SMMU_V3_OAS_32BITS:
      Oas = 32;
      break;

    case SMMU_V3_OAS_36BITS:
      Oas = 36;
      break;

    case SMMU_V3_OAS_40BITS:
      Oas = 40;
      break;

    case SMMU_V3_OAS_42BITS:
      Oas = 42;
      break;

    case SMMU_V3_OAS_44BITS:
      Oas = 44;
      break;

    case SMMU_V3_OAS_48BITS:
      Oas = 48;
      break;

    case SMMU_V3_OAS_52BITS:
      if (Private->Features.MinorVersion == 0) {
        DEBUG ((DEBUG_ERROR, "%a: 52 bit Output address size not supported for SMMUv3.0\n", __FUNCTION__));
        return EFI_DEVICE_ERROR;
      }

      Oas = 52;
      break;

    case SMMU_V3_OAS_RES:
    default:
      DEBUG ((DEBUG_ERROR, "%a: Output address size unknown\n", __FUNCTION__));
      return EFI_DEVICE_ERROR;
  }

  Private->Features.Oas         = Oas;
  Private->Features.OasEncoding = OasBits;
  IasAarch32                    = TtfAarch32 ? 40 : 0;
  IasAarch64                    = TtfAarch64 ? Private->Features.Oas : 0;
  Private->Features.Ias         = IasAarch64;

  if (IasAarch32 > IasAarch64) {
    Private->Features.Ias = IasAarch32;
  }

  DEBUG ((DEBUG_INFO, "%a: Input Addr: %d-bits, Output Addr: %d-bits\n", __FUNCTION__, Private->Features.Ias, Private->Features.Oas));

  // Set cachebiity and shareability attributes for Table and Queue access
  Cr1Setting = BIT_FIELD_SET (SMMU_V3_CR1_INSH, SMMU_V3_CR1_SH_MASK, SMMU_V3_CR1_TAB_SH_SHIFT);
  Cr1Setting = Cr1Setting | BIT_FIELD_SET (SMMU_V3_CR1_WBCACHE, SMMU_V3_CR1_OC_MASK, SMMU_V3_CR1_TAB_OC_SHIFT);
  Cr1Setting = Cr1Setting | BIT_FIELD_SET (SMMU_V3_CR1_WBCACHE, SMMU_V3_CR1_IC_MASK, SMMU_V3_CR1_TAB_IC_SHIFT);
  Cr1Setting = Cr1Setting | BIT_FIELD_SET (SMMU_V3_CR1_INSH, SMMU_V3_CR1_SH_MASK, SMMU_V3_CR1_QUE_SH_SHIFT);
  Cr1Setting = Cr1Setting | BIT_FIELD_SET (SMMU_V3_CR1_WBCACHE, SMMU_V3_CR1_OC_MASK, SMMU_V3_CR1_QUE_OC_SHIFT);
  Cr1Setting = Cr1Setting | BIT_FIELD_SET (SMMU_V3_CR1_WBCACHE, SMMU_V3_CR1_IC_MASK, SMMU_V3_CR1_QUE_IC_SHIFT);

  MmioWrite32 (Private->BaseAddress + SMMU_V3_CR1_OFFSET, Cr1Setting);

  Cr2Setting = MmioRead32 (Private->BaseAddress + SMMU_V3_CR2_OFFSET);

  // Clear and program Private TLB maintenance bit
  Cr2Setting = Cr2Setting & ~(BIT_FIELD_SET (1, SMMU_V3_CR2_PTM_MASK, SMMU_V3_CR2_PTM_SHIFT));
  Cr2Setting = Cr2Setting | BIT_FIELD_SET (SMMU_V3_CR2_PTM_ENABLE, SMMU_V3_CR2_PTM_MASK, SMMU_V3_CR2_PTM_SHIFT);
  Cr2Setting = Cr2Setting | BIT_FIELD_SET (SMMU_V3_CR2_RECINVSID_ENABLE, SMMU_V3_CR2_RECINVSID_MASK, SMMU_V3_CR2_RECINVSID_SHIFT);

  MmioWrite32 (Private->BaseAddress + SMMU_V3_CR2_OFFSET, Cr2Setting);

  return EFI_SUCCESS;
}

/**
  Configure SMMUv3 controller command and event queue sizes, stream and sub stream bits in
  SMMU_V3_CONTROLLER_FEATURES structure.

  @param[in]  Private       Pointer to the SMMU_V3_CONTROLLER_PRIVATE_DATA instance.

  @retval EFI_SUCCESS              The SMMUv3 controller queue sizes were configured successfully.
  @retval EFI_INVALID_PARAMETER    Private is NULL.
  @retval EFI_DEVICE_ERROR         The SMMUv3 controller queue sizes configuration failed.
  @retval EFI_UNSUPPORTED          The SMMUv3 driver does not support this feature.

 **/
STATIC
EFI_STATUS
EFIAPI
ConfigureSmmuV3ControllerQueueSizes (
  IN  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private
  )
{
  UINT32  Idr1;
  UINT32  Size;
  UINT32  Preset;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Idr1   = MmioRead32 (Private->BaseAddress + SMMU_V3_IDR1_OFFSET);
  Preset = BIT_FIELD_GET (Idr1, SMMU_V3_IDR1_PRESET_MASK, SMMU_V3_IDR1_PRESET_SHIFT);

  // SMMUv3 driver does not support fixed address Table or Queue base
  if (Preset != 0) {
    DEBUG ((DEBUG_ERROR, "%a: Driver does not support TABLES_PRESET, QUEUES_PRESET\n", __FUNCTION__));
    return EFI_UNSUPPORTED;
  }

  Size = BIT_FIELD_GET (Idr1, SMMU_V3_IDR1_CMDQS_MASK, SMMU_V3_IDR1_CMDQS_SHIFT);
  if (Size > SMMU_V3_CMDQS_MAX) {
    DEBUG ((DEBUG_ERROR, "%a: Command queue entries(log2) cannot exceed %d\n", __FUNCTION__, SMMU_V3_CMDQS_MAX));
    return EFI_DEVICE_ERROR;
  }

  Private->Features.CmdqEntriesLog2 = Size;

  Size = BIT_FIELD_GET (Idr1, SMMU_V3_IDR1_EVTQS_MASK, SMMU_V3_IDR1_EVTQS_SHIFT);
  if (Size > SMMU_V3_EVTQS_MAX) {
    DEBUG ((DEBUG_ERROR, "%a: Event queue entries(log2) cannot exceed %d\n", __FUNCTION__, SMMU_V3_EVTQS_MAX));
    return EFI_DEVICE_ERROR;
  }

  Private->Features.EvtqEntriesLog2 = Size;

  Size = BIT_FIELD_GET (Idr1, SMMU_V3_IDR1_SUB_SID_MASK, SMMU_V3_IDR1_SUB_SID_SHIFT);
  if (Size > SMMU_V3_SUB_SID_SIZE_MAX) {
    DEBUG ((DEBUG_ERROR, "%a: Max bits of SubStreamID cannot exceed %d\n", __FUNCTION__, SMMU_V3_SUB_SID_SIZE_MAX));
    return EFI_DEVICE_ERROR;
  }

  Private->Features.SubStreamNBits = Size;

  Size = BIT_FIELD_GET (Idr1, SMMU_V3_IDR1_SID_MASK, SMMU_V3_IDR1_SID_SHIFT);
  if (Size > SMMU_V3_SID_SIZE_MAX) {
    DEBUG ((DEBUG_ERROR, "%a: Max bits of StreamID cannot exceed %d\n", __FUNCTION__, SMMU_V3_SID_SIZE_MAX));
    return EFI_DEVICE_ERROR;
  }

  Private->Features.StreamNBits = Size;

  MmioWrite32 (Private->BaseAddress + SMMU_V3_IDR1_OFFSET, Idr1);

  return EFI_SUCCESS;
}

/**
  Configure SMMUv3 controller global setting in SMMU_V3_CONTROLLER_FEATURES structure.

  @param[in]  Private       Pointer to the SMMU_V3_CONTROLLER_PRIVATE_DATA instance.

  @retval EFI_SUCCESS              The SMMUv3 controller global settings were configured successfully.
  @retval EFI_INVALID_PARAMETER    Private is NULL.
  @retval other                    The SMMUv3 controller global settings configuration failed.

 **/
STATIC
EFI_STATUS
EFIAPI
ConfigureSmmuV3ControllerSettings (
  IN  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = IdentifySmmuV3ControllerFeatures (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to identify SMMUv3 features\n", __FUNCTION__));
    return Status;
  }

  Status = ConfigureSmmuV3ControllerXlatSupport (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to configure SMMUv3 translation support\n", __FUNCTION__));
    return Status;
  }

  Status = ConfigureSmmuV3ControllerQueueSizes (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to configure SMMUv3 queue sizes\n", __FUNCTION__));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Configure SMMUv3 command queue in SMMU_V3_QUEUE structure. Also, update command queue register
  with queue base address and initialize command queue consumer and producer registers.

  @param[in]  Private       Pointer to the SMMU_V3_CONTROLLER_PRIVATE_DATA instance.

  @retval EFI_SUCCESS              The SMMUv3 controller command queue was configured successfully.
  @retval EFI_INVALID_PARAMETER    Private is NULL.
  @retval EFI_OUT_OF_RESOURCES     Failed to allocate memory for command queue.

 **/
STATIC
EFI_STATUS
EFIAPI
SetupSmmuV3Cmdq (
  IN  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private
  )
{
  UINT32                CmdqSize;
  UINT64                CmdqBaseReg;
  EFI_PHYSICAL_ADDRESS  QBase;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  CmdqSize = (1 << Private->Features.CmdqEntriesLog2) * SMMU_V3_CMD_SIZE;
  DEBUG ((DEBUG_INFO, "%a: Total CMDQ entries: %d\n", __FUNCTION__, (1 << Private->Features.CmdqEntriesLog2)));

  QBase = (EFI_PHYSICAL_ADDRESS)AllocateAlignedPages (EFI_SIZE_TO_PAGES (CmdqSize), CmdqSize);
  ZeroMem ((VOID *)QBase, EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (CmdqSize)));

  if (!QBase) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory for CMDQ\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  DEBUG ((DEBUG_INFO, "%a: Memory allocated at %lx for CMDQ\n", __FUNCTION__, QBase));
  Private->CmdQueue.QBase = QBase;

  CmdqBaseReg = QBase & (SMMU_V3_CMDQ_BASE_ADDR_MASK << SMMU_V3_CMDQ_BASE_ADDR_SHIFT);
  CmdqBaseReg = CmdqBaseReg | (1ULL << SMMU_V3_RA_HINT_SHIFT);
  CmdqBaseReg = CmdqBaseReg | Private->Features.CmdqEntriesLog2;

  Private->CmdQueue.ConsRegBase = Private->BaseAddress + SMMU_V3_CMDQ_CONS_OFFSET;
  Private->CmdQueue.ProdRegBase = Private->BaseAddress + SMMU_V3_CMDQ_PROD_OFFSET;

  // Initialize command queue base register
  DEBUG ((DEBUG_INFO, "%a: Write to CMDQ_BASE 0x%llx CMDQ_BASE Addr 0x%p\n", __FUNCTION__, CmdqBaseReg, Private->BaseAddress + SMMU_V3_CMDQ_BASE_OFFSET));
  MmioWrite64 (Private->BaseAddress + SMMU_V3_CMDQ_BASE_OFFSET, CmdqBaseReg);

  // Initialize command queue producer and consumer registers
  MmioWrite32 (Private->CmdQueue.ConsRegBase, 0);
  MmioWrite32 (Private->CmdQueue.ProdRegBase, 0);

  return EFI_SUCCESS;
}

/**
  Configure SMMUv3 event queue in SMMU_V3_QUEUE structure. Also, update event queue register
  with queue base address and initialize event queue consumer and producer registers.

  @param[in]  Private       Pointer to the SMMU_V3_CONTROLLER_PRIVATE_DATA instance.

  @retval EFI_SUCCESS              The SMMUv3 controller event queue was configured successfully.
  @retval EFI_INVALID_PARAMETER    Private is NULL.
  @retval EFI_OUT_OF_RESOURCES     Failed to allocate memory for event queue.

 **/
STATIC
EFI_STATUS
EFIAPI
SetupSmmuV3Evtq (
  IN  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private
  )
{
  UINT32                EvtqSize;
  UINT64                EvtqBaseReg;
  EFI_PHYSICAL_ADDRESS  QBase;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  EvtqSize = (1 << Private->Features.EvtqEntriesLog2) * SMMU_V3_EVT_RECORD_SIZE;
  DEBUG ((DEBUG_INFO, "%a: Total EVTQ entries: %d\n", __FUNCTION__, (1 << Private->Features.EvtqEntriesLog2)));

  QBase = (EFI_PHYSICAL_ADDRESS)AllocateAlignedPages (EFI_SIZE_TO_PAGES (EvtqSize), EvtqSize);
  ZeroMem ((VOID *)QBase, EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (EvtqSize)));

  if (!QBase) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory for EVTQ\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  DEBUG ((DEBUG_INFO, "%a: Memory allocated at %lx for EVTQ\n", __FUNCTION__, QBase));
  Private->EvtQueue.QBase = QBase;

  EvtqBaseReg = QBase & (SMMU_V3_EVTQ_BASE_ADDR_MASK << SMMU_V3_EVTQ_BASE_ADDR_SHIFT);
  EvtqBaseReg = EvtqBaseReg | (1ULL << SMMU_V3_WA_HINT_SHIFT);
  EvtqBaseReg = EvtqBaseReg | Private->Features.EvtqEntriesLog2;

  Private->EvtQueue.ConsRegBase = Private->BaseAddress + SMMU_V3_EVTQ_CONS_OFFSET;
  Private->EvtQueue.ProdRegBase = Private->BaseAddress + SMMU_V3_EVTQ_PROD_OFFSET;

  // Initialize event queue base register
  DEBUG ((DEBUG_INFO, "%a: Write to EVTQ_BASE 0x%llx EVTQ_BASE Addr 0x%p\n", __FUNCTION__, EvtqBaseReg, Private->BaseAddress + SMMU_V3_EVTQ_BASE_OFFSET));
  MmioWrite64 (Private->BaseAddress + SMMU_V3_EVTQ_BASE_OFFSET, EvtqBaseReg);

  // Initialize event queue producer and consumer registers
  MmioWrite32 (Private->EvtQueue.ConsRegBase, 0);
  MmioWrite32 (Private->EvtQueue.ProdRegBase, 0);

  return EFI_SUCCESS;
}

/**
  Clear stream table entry (STE)

  @param[in, out]      SteData       Pointer to STE Data

 **/
STATIC
VOID
ClearSte (
  IN OUT UINT64  *SteData
  )
{
  UINT32  Index;

  for (Index = 0; Index < SMMU_V3_STRTAB_ENTRY_SIZE_DW; Index++) {
    SteData[Index] = 0;
  }
}

/**
  Write stream table entry (STE)

  @param[in, out]      StEntry      Pointer to STE Entry
  @param[in]           SteData      Pointer to STE Data

 **/
STATIC
VOID
WriteSte (
  IN OUT UINT64    *StEntry,
  IN CONST UINT64  *SteData
  )
{
  INT32  Index;

  /*
    Invalidate Stream Table Entry by clearing the Valid bit
    Sets STE.Valid to 0 (bit[0] of first 64-bit word)
  */
  StEntry[0] = 0;

  /*
    Update Stream Table Entry by writing the upper 64-bit word first,
    followed by the lower 64-bit word containing STE.Valid bit,
    ensuring proper memory ordering
   */
  for (Index = SMMU_V3_STRTAB_ENTRY_SIZE_DW - 1; Index >= 0; Index--) {
    StEntry[Index] = SteData[Index];
  }

  // Ensure written data (STE) is observable to SMMU controller by performing DSB
  ArmDataSynchronizationBarrier ();
}

/**
  Invalidate all Stream Table Entries (STE) in the SMMUv3 controller.

  @param[in]  Private       Pointer to the SMMU_V3_CONTROLLER_PRIVATE_DATA instance.

  @retval EFI_SUCCESS              The SMMUv3 controller STEs were invalidated successfully.
  @retval EFI_INVALID_PARAMETER    Private is NULL.

 **/
STATIC
EFI_STATUS
InvalidateStes (
  IN  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private
  )
{
  UINT32  Index;
  UINT32  SteCount;
  UINT64  SteData[SMMU_V3_STRTAB_ENTRY_SIZE_DW];
  UINT64  *SteAddr;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  ClearSte (SteData);
  SteAddr  = (UINT64 *)Private->SteBase;
  SteCount = 1 << Private->Features.StreamNBits;

  for (Index = 0; Index < SteCount; Index++) {
    WriteSte (SteAddr, SteData);
    SteAddr += SMMU_V3_STRTAB_ENTRY_SIZE_DW;
  }

  return EFI_SUCCESS;
}

/**
  Setup SMMUv3 Stream Table (STRTAB) in SMMU_V3_STREAM_TABLE_COFIG structure.
  Update stream table configuration register with linear format and stream
  table base register with stream table base. Invalidate all stream table
  entries (STE).

  @param[in]  Private       Pointer to the SMMU_V3_CONTROLLER_PRIVATE_DATA instance.

  @retval EFI_SUCCESS              The SMMUv3 controller event queue was configured successfully.
  @retval EFI_INVALID_PARAMETER    Private is NULL.
  @retval EFI_OUT_OF_RESOURCES     Failed to allocate memory for event queue.

 **/
STATIC
EFI_STATUS
EFIAPI
SetupSmmuV3StrTable (
  IN  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS            Status;
  UINT32                StrtabSize;
  UINT32                StrtabCfgSetting;
  UINT64                StrtabBaseReg;
  EFI_PHYSICAL_ADDRESS  TblBase;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  StrtabSize = (1 << Private->Features.StreamNBits) * SMMU_V3_STRTAB_ENTRY_SIZE;
  DEBUG ((DEBUG_INFO, "%a: Total STRTAB entries: %d\n", __FUNCTION__, (1 << Private->Features.StreamNBits)));

  TblBase = (EFI_PHYSICAL_ADDRESS)AllocateAlignedPages (EFI_SIZE_TO_PAGES (StrtabSize), StrtabSize);
  ZeroMem ((VOID *)TblBase, EFI_PAGES_TO_SIZE (EFI_SIZE_TO_PAGES (StrtabSize)));

  if (!TblBase) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to allocate memory for STRTAB\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  DEBUG ((DEBUG_INFO, "%a: Memory allocated at %lx for STRTAB\n", __FUNCTION__, TblBase));

  Private->SteBase = TblBase;
  StrtabBaseReg    = (UINT64)TblBase & (SMMU_V3_STRTAB_BASE_ADDR_MASK << SMMU_V3_STRTAB_BASE_ADDR_SHIFT);
  StrtabBaseReg    = StrtabBaseReg | (1ULL << SMMU_V3_RA_HINT_SHIFT);

  // Assume linear format for stream table
  StrtabCfgSetting = SMMU_V3_LINEAR_STR_TABLE << SMMU_V3_STR_FMT_SHIFT;
  StrtabCfgSetting = StrtabCfgSetting | Private->Features.StreamNBits;

  DEBUG ((DEBUG_INFO, "%a: Write to STRTAB_BASE_CFG 0x%x STRTAB_BASE_CFG reg 0x%p\n", __FUNCTION__, StrtabCfgSetting, Private->BaseAddress + SMMU_V3_STRTAB_BASE_CFG_OFFSET));
  MmioWrite32 (Private->BaseAddress + SMMU_V3_STRTAB_BASE_CFG_OFFSET, StrtabCfgSetting);

  DEBUG ((DEBUG_INFO, "%a: Write to STRTAB_BASE 0x%llx STRTAB_BASE reg 0x%p\n", __FUNCTION__, StrtabBaseReg, Private->BaseAddress + SMMU_V3_STRTAB_BASE_OFFSET));
  MmioWrite64 (Private->BaseAddress + SMMU_V3_STRTAB_BASE_OFFSET, StrtabBaseReg);

  // Mark STE as invalid
  Status = InvalidateStes (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to invalidate STEs\n", __FUNCTION__));
    return Status;
  }

  return EFI_SUCCESS;
}

/**
  Track CMDQ producer and consumer indexes

  @param[in]  Private       Pointer to the SMMU_V3_CONTROLLER_PRIVATE_DATA instance.

 **/
STATIC
VOID
TrackCmdqIdx (
  IN  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private
  )
{
  if (Private == NULL) {
    return;
  }

  DEBUG ((
    DEBUG_INFO,
    "%a: Track CMDQ consumer_idx: %x; producer_idx: %x\n",
    __FUNCTION__,
    MmioRead32 (Private->CmdQueue.ConsRegBase),
    MmioRead32 (Private->CmdQueue.ProdRegBase)
    ));
}

/**
  Construct command CMD_CFGI_ALL

  This command invalidates all information cached in the SMMUv3 controller.

  @param[in, out]      Cmd        Pointer to Command

 **/
STATIC
VOID
ConstructInvAllCfg (
  IN OUT UINT64  *Cmd
  )
{
  UINT32  Stream;

  Stream = SMMU_V3_NS_STREAM;

  Cmd[0]  = BIT_FIELD_SET (SMMU_V3_OP_CFGI_ALL, SMMU_V3_OP_MASK, SMMU_V3_OP_SHIFT);
  Cmd[0] |= BIT_FIELD_SET (Stream, SMMU_V3_SSEC_MASK, SMMU_V3_SSEC_SHIFT);
  Cmd[1]  = BIT_FIELD_SET (SMMU_V3_SID_ALL, SMMU_V3_SID_RANGE_MASK, SMMU_V3_SID_RANGE_SHIFT);
}

/**
  Display Command Queue error status and handle error conditions.

  This function reads GERROR and GERRORN registers to check for command queue errors,
  displays appropriate error messages, and acknowledges errors by toggling bits in GERRORN.

  @param[in] Private          Pointer to the SMMU_V3_CONTROLLER_PRIVATE_DATA instance.

  @retval None

 **/
STATIC
VOID
DisplayCmdqErr (
  IN  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private
  )
{
  UINT32  ConsReg;
  UINT32  GerrorReg;
  UINT32  GerrorNReg;

  if (Private == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter Private\n", __FUNCTION__));
    return;
  }

  // Check global error conditions
  GerrorReg  = MmioRead32 (Private->BaseAddress + SMMU_V3_GERROR_OFFSET);
  GerrorNReg = MmioRead32 (Private->BaseAddress + SMMU_V3_GERRORN_OFFSET);

  // Check if the bits differ between GERROR and GERRORN
  if (BIT_FIELD_GET (GerrorReg, SMMU_V3_GERROR_SFM_ERR_MASK, SMMU_V3_GERROR_SFM_ERR_SHIFT) !=
      BIT_FIELD_GET (GerrorNReg, SMMU_V3_GERRORN_SFM_ERR_MASK, SMMU_V3_GERRORN_SFM_ERR_SHIFT))
  {
    DEBUG ((DEBUG_ERROR, "%a: Entered service failure mode\n", __FUNCTION__));
  }

  // Return if no command queue errors
  if (BIT_FIELD_GET (GerrorReg, SMMU_V3_GERROR_CMDQ_ERR_MASK, SMMU_V3_GERROR_CMDQ_ERR_SHIFT) ==
      BIT_FIELD_GET (GerrorNReg, SMMU_V3_GERRORN_CMDQ_ERR_MASK, SMMU_V3_GERRORN_CMDQ_ERR_SHIFT))
  {
    return;
  }

  DEBUG ((DEBUG_ERROR, "%a: SMMU cannot process commands\n", __FUNCTION__));
  DEBUG ((DEBUG_ERROR, "%a: GERROR: 0x%08x GERRORN: 0x%08x\n", __FUNCTION__, GerrorReg, GerrorNReg));

  ConsReg = MmioRead32 (Private->CmdQueue.ConsRegBase);

  switch (BIT_FIELD_GET (ConsReg, SMMU_V3_CMDQ_ERRORCODE_MASK, SMMU_V3_CMDQ_ERRORCODE_SHIFT)) {
    case SMMU_V3_CMDQ_CERROR_NONE:
      break;
    case SMMU_V3_CMDQ_CERROR_ILL:
      DEBUG ((DEBUG_ERROR, "%a: CMDQ error - Invalid command\n", __FUNCTION__));
      break;
    case SMMU_V3_CMDQ_CERROR_ABT:
      DEBUG ((DEBUG_ERROR, "%a: CMDQ error - Command abort\n", __FUNCTION__));
      break;
    case SMMU_V3_CMDQ_CERROR_ATC_INV_SYNC:
      DEBUG ((DEBUG_ERROR, "%a: CMDQ error - ATC invalidation sync\n", __FUNCTION__));
      break;
    default:
      DEBUG ((DEBUG_ERROR, "%a: CMDQ error - Unknown CMDQ CONS REG 0x%x\n", __FUNCTION__, ConsReg));
      break;
  }

  // Acknowledge error
  DEBUG ((DEBUG_ERROR, "%a: Acknowledging error by toggling GERRORN[CMD_ERR]\n", __FUNCTION__));
  GerrorNReg = GerrorNReg ^ (SMMU_V3_GERRORN_CMDQ_ERR_MASK << SMMU_V3_GERRORN_CMDQ_ERR_SHIFT);
  MmioWrite32 (Private->BaseAddress + SMMU_V3_GERRORN_OFFSET, GerrorNReg);
}

/**
  Writes a command to the SMMUv3 Command Queue.

  This function copies the command data to the specified command queue entry and
  ensures the data is observable to SMMU through a data synchronization barrier.

  @param[in]  CmdqEntry    Pointer to command queue entry.
  @param[in]  CmdDword     Pointer to command data to write.

  @retval None
**/
STATIC
VOID
PushEntryToCmdq (
  IN UINT64        *CmdqEntry,
  IN CONST UINT64  *CmdDword
  )
{
  UINT32  Index;

  if ((CmdqEntry == NULL) || (CmdDword == NULL)) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter\n", __FUNCTION__));
    return;
  }

  DEBUG ((
    DEBUG_VERBOSE,
    "%a: Writing command to address: 0x%p\n",
    __FUNCTION__,
    (VOID *)CmdqEntry
    ));

  for (Index = 0; Index < SMMU_V3_CMD_SIZE_DW; Index++) {
    CmdqEntry[Index] = CmdDword[Index];
  }

  // Ensure data is observable to SMMU
  ArmDataSynchronizationBarrier ();
}

/**
  Calculates the next write index for the command queue.

  This function handles index wraparound and wrap bit toggling when the current index
  reaches the maximum value.

  @param[in]  Private     Pointer to the SMMU_V3_CONTROLLER_PRIVATE_DATA instance.
  @param[out] NextWrIdx   Pointer to the next produce index
  @param[in]  CurrentIdx  Current producer index.
  @param[in]  ProdWrap    Current producer wrap bit value.

  @retval     Returns next write index with appropriate wrap bit.
**/
STATIC
EFI_STATUS
FindOffsetNextWrIdx (
  IN SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private,
  IN UINT32                           *NextWrIdx,
  IN UINT32                           CurrentIdx,
  IN UINT32                           ProdWrap
  )
{
  UINT32  NextIdx;
  UINT32  MaxIdx;
  UINT32  WrapBitSet;

  if ((Private == NULL) || (NextWrIdx == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  MaxIdx = (1 << Private->Features.CmdqEntriesLog2) - 1;
  if (CurrentIdx > MaxIdx) {
    DEBUG ((DEBUG_ERROR, "%a: Producer index overflow: 0x%x\n", __FUNCTION__, CurrentIdx));
    ASSERT (FALSE);
  }

  if (CurrentIdx < MaxIdx) {
    NextIdx    = CurrentIdx + 1;
    *NextWrIdx = NextIdx | ProdWrap;
    return EFI_SUCCESS;
  }

  // Handle index wraparound - reset index to 0 and toggle wrap bit
  NextIdx    = 0;
  WrapBitSet = 1 << Private->Features.CmdqEntriesLog2;

  if (ProdWrap == 0) {
    *NextWrIdx = NextIdx | WrapBitSet;
    return EFI_SUCCESS;
  }

  *NextWrIdx = NextIdx;

  return EFI_SUCCESS;
}

/**
  Updates the SMMUv3 Command Queue producer register with new index.

  @param[in]  Private   Pointer to the SMMU_V3_CONTROLLER_PRIVATE_DATA instance.
  @param[in]  Idx       New producer index value to write.

  @retval None
**/
STATIC
VOID
UpdateCmdqProd (
  IN  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private,
  IN UINT32                            Idx
  )
{
  if (Private == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter\n", __FUNCTION__));
    return;
  }

  DEBUG ((DEBUG_INFO, "%a: Updating CMDQ-PRODBASE Idx:%u\n", __FUNCTION__, Idx));
  MmioWrite32 (Private->CmdQueue.ProdRegBase, Idx);

  // Verify write was successful
  if (MmioRead32 (Private->CmdQueue.ProdRegBase) != Idx) {
    DEBUG ((DEBUG_ERROR, "%a: Hardware not updated with write index\n", __FUNCTION__));
    ASSERT (FALSE);
  }
}

/**
  Issues a command to the SMMUv3 command queue.

  This function:
  1. Checks command queue space availability
  2. Adds command to queue if space available
  3. Updates producer index after queuing

  @param[in]  Private     Pointer to SMMU_V3_CONTROLLER_PRIVATE_DATA instance.
  @param[in]  Cmd         Pointer to command to be queued.

  @retval EFI_SUCCESS           Command queued successfully.
  @retval EFI_INVALID_PARAMETER Private or Cmd is NULL.
  @retval EFI_OUT_OF_RESOURCES  Command queue is full.
  @retval EFI_DEVICE_ERROR      Error calculating next write index.
**/
STATIC
EFI_STATUS
IssueCmdToSmmuV3Controller (
  IN  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private,
  IN  UINT64                           *Cmd
  )
{
  EFI_STATUS            Status;
  UINT32                ProdIdx;
  UINT32                ConsIdx;
  UINT32                ProdWrap;
  UINT32                ConsWrap;
  UINT32                ProdReg;
  UINT32                ConsReg;
  UINT32                IndexMask;
  UINT32                QMaxEntries;
  UINT32                QEmptySlots;
  EFI_PHYSICAL_ADDRESS  CmdTarget;
  UINT32                NextWrIdx;
  UINT32                CurrentWrIdx;

  if ((Private == NULL) || (Cmd == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  QMaxEntries = 1 << Private->Features.CmdqEntriesLog2;
  IndexMask   = SMMUV3_ALL_ONES (Private->Features.CmdqEntriesLog2);
  ProdReg     = MmioRead32 (Private->CmdQueue.ProdRegBase);
  ProdWrap    = BIT_FIELD_GET (ProdReg, SMMU_V3_WRAP_MASK, Private->Features.CmdqEntriesLog2);
  ProdIdx     = ProdReg & IndexMask;

  ConsReg  = MmioRead32 (Private->CmdQueue.ConsRegBase);
  ConsWrap = BIT_FIELD_GET (ConsReg, SMMU_V3_WRAP_MASK, Private->Features.CmdqEntriesLog2);
  ConsIdx  = ConsReg & IndexMask;

  DisplayCmdqErr (Private);

  // Calculate empty slots in queue
  if (ProdWrap == ConsWrap) {
    QEmptySlots = QMaxEntries - (ProdIdx - ConsIdx);
  } else {
    QEmptySlots = ConsIdx - ProdIdx;
  }

  if (QEmptySlots == 0) {
    DEBUG ((DEBUG_ERROR, "%a: Command queue full; No new cmd can be issued\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  // Add command to queue
  CurrentWrIdx = ProdIdx;
  CmdTarget    = Private->CmdQueue.QBase + CurrentWrIdx * SMMU_V3_CMD_SIZE;
  PushEntryToCmdq ((UINT64 *)CmdTarget, (UINT64 *)Cmd);

  Status = FindOffsetNextWrIdx (Private, &NextWrIdx, CurrentWrIdx, (ProdWrap << Private->Features.CmdqEntriesLog2));
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to find next write index\n", __FUNCTION__));
    return EFI_OUT_OF_RESOURCES;
  }

  TrackCmdqIdx (Private);
  DEBUG ((DEBUG_INFO, "%a: CurrentWrIdx: %x; NextWrIdx: %x\n", __FUNCTION__, CurrentWrIdx, NextWrIdx));

  // Update producer register with next write index
  UpdateCmdqProd (Private, NextWrIdx);

  return EFI_SUCCESS;
}

/**
  Constructs a CMD_SYNC command for SMMUv3 command queue.

  CMD_SYNC provides synchronization for preceding commands issued to the same
  command queue. It ensures commands are completed before proceeding.

  @param[out] Cmd     Pointer to command buffer to store the constructed CMD_SYNC.
                      Must be at least 2 UINT64s in size.

  @retval None
**/
STATIC
VOID
ConstructCmdSync (
  OUT UINT64  *Cmd
  )
{
  if (Cmd == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter\n", __FUNCTION__));
    ASSERT (FALSE);
    return;
  }

  //
  // Construct CMD_SYNC command
  //
  Cmd[0]  = BIT_FIELD_SET (SMMU_V3_OP_CMD_SYNC, SMMU_V3_OP_MASK, SMMU_V3_OP_SHIFT);
  Cmd[0] |= BIT_FIELD_SET (SMMU_V3_CSIGNAL_NONE, SMMU_V3_CSIGNAL_MASK, SMMU_V3_CSIGNAL_SHIFT);
  Cmd[1]  = 0;
}

/**
  Checks if command queue read index has caught up with write index.

  This function polls the command queue consumer register to check if SMMU
  has processed all commands up to the current producer index.

    @param[in]  Private       Pointer to SMMUv3 controller private data structure.

  @retval EFI_SUCCESS           Read index matches write index.
  @retval EFI_TIMEOUT           Timeout waiting for indices to match.
  @retval EFI_INVALID_PARAMETER Private pointer is NULL.
**/
STATIC
EFI_STATUS
RdIdxMeetsWrIdx (
  IN  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private
  )
{
  UINT32  Attempts;
  UINT32  ProdReg;
  UINT32  ConsReg;
  UINT32  ProdIdx;
  UINT32  ConsIdx;
  UINT32  ProdWrap;
  UINT32  ConsWrap;
  UINT32  IndexMask;

  if (Private == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  IndexMask = SMMUV3_ALL_ONES (Private->Features.CmdqEntriesLog2);
  ProdReg   = MmioRead32 (Private->CmdQueue.ProdRegBase);
  ProdWrap  = BIT_FIELD_GET (ProdReg, SMMU_V3_WRAP_MASK, Private->Features.CmdqEntriesLog2);
  ProdIdx   = ProdReg & IndexMask;

  ConsReg  = MmioRead32 (Private->CmdQueue.ConsRegBase);
  ConsWrap = BIT_FIELD_GET (ConsReg, SMMU_V3_WRAP_MASK, Private->Features.CmdqEntriesLog2);
  ConsIdx  = ConsReg & IndexMask;

  Attempts = 0;
  while (Attempts++ < SMMU_V3_POLL_ATTEMPTS) {
    if ((ConsWrap == ProdWrap) && (ProdIdx == ConsIdx)) {
      return EFI_SUCCESS;
    }

    ConsReg  = MmioRead32 (Private->CmdQueue.ConsRegBase);
    ConsWrap = BIT_FIELD_GET (ConsReg, SMMU_V3_WRAP_MASK, Private->Features.CmdqEntriesLog2);
    ConsIdx  = ConsReg & IndexMask;
  }

  DEBUG ((
    DEBUG_ERROR,
    "%a: Timeout - CONS_REG: 0x%x PROD_REG: 0x%x\n",
    __FUNCTION__,
    ConsReg,
    ProdReg
    ));

  return EFI_TIMEOUT;
}

/**
  Synchronizes the SMMUv3 command queue by issuing a CMD_SYNC command.

  The CMD_SYNC command ensures completion of all prior commands and
  observability of related transactions through the SMMU. This function
  waits for the command queue read index to catch up with the write index.

  @param[in]  Private       Pointer to SMMUv3 controller private data structure.

  @retval EFI_SUCCESS       Command queue synchronized successfully.
  @retval EFI_TIMEOUT       Timeout waiting for command completion.
  @retval EFI_DEVICE_ERROR  Error issuing sync command.
**/
STATIC
EFI_STATUS
SynchronizeCmdq (
  IN  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;
  UINT64      Cmd[SMMU_V3_CMD_SIZE_DW];

  if (Private == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  // Issue CMD_SYNC command
  ConstructCmdSync (Cmd);
  Status = IssueCmdToSmmuV3Controller (Private, Cmd);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to issue CMD_SYNC command to CMDQ\n", __FUNCTION__));
    return Status;
  }

  // Track command queue indices for verification
  TrackCmdqIdx (Private);

  // Wait for read index to catch up with write index
  Status = RdIdxMeetsWrIdx (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Timeout: CMDQ populated by PE not consumed by SMMU\n", __FUNCTION__));
    return Status;
  }

  // Track final command queue state
  TrackCmdqIdx (Private);

  return EFI_SUCCESS;
}

/**
  Invalidates all cached configurations in the SMMUv3 controller.

  This function issues a CFGI_ALL command to invalidate configuration caches and
  synchronizes the command queue to ensure completion.

  @param[in]  Private       Pointer to SMMUv3 controller private data structure.

  @retval EFI_SUCCESS           Configurations were invalidated successfully.
  @retval EFI_INVALID_PARAMETER Private pointer is NULL.
  @retval EFI_DEVICE_ERROR      Failed to issue or synchronize invalidation command.
**/
STATIC
EFI_STATUS
InvalCachedCfgs (
  IN  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;
  UINT64      Cmd[SMMU_V3_CMD_SIZE_DW];

  if (Private == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter\n", __FUNCTION__));
    return EFI_INVALID_PARAMETER;
  }

  // Invalidate configuration caches
  ConstructInvAllCfg (Cmd);

  Status = IssueCmdToSmmuV3Controller (Private, Cmd);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to issue CFGI_ALL command to CMDQ\n", __FUNCTION__));
    return Status;
  }

  // Issue CMD_SYNC to ensure completion of prior commands used for invalidation
  Status = SynchronizeCmdq (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to synchronize command queue\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Constructs a TLB invalidation command for SMMUv3.

  This function builds a command to invalidate TLB entries based on the
  provided command format.

  @param[out] Cmd         Pointer to command buffer to store the constructed command.
                          Must be at least 2 UINT64s in size.
  @param[in]  CmdOpcode   TLB invalidation opcode.

  @retval None
**/
STATIC
VOID
EFIAPI
ConstructTlbiCmd (
  OUT     UINT64  *Cmd,
  IN      UINT8   CmdOpcode
  )
{
  if (Cmd == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter\n", __FUNCTION__));
    return;
  }

  // Construct TLB invalidation command
  Cmd[0] = BIT_FIELD_SET (CmdOpcode, SMMU_V3_OP_MASK, SMMU_V3_OP_SHIFT);
  Cmd[1] = 0;
}

/**
  Configure SMMUv3 controller to invalidate all TLBs and cached configuration.

  @param[in]  Private       Pointer to the SMMU_V3_CONTROLLER_PRIVATE_DATA instance.

  @retval EFI_SUCCESS              The SMMUv3 controller event queue was configured successfully.
  @retval EFI_INVALID_PARAMETER    Private is NULL.
  @retval EFI_DEVICE_ERROR         Failed to invalidate all caches and TLBs.

 **/
STATIC
EFI_STATUS
InvalidateCachedCfgsTlbs (
  IN  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;
  UINT64      Cmd[SMMU_V3_CMD_SIZE_DW];
  UINT8       CmdOpcode;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  // Invalidate cached configuration
  Status = InvalCachedCfgs (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to invalidate cached configurations\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  // Invalidate EL2 TLB entries
  CmdOpcode = SMMU_V3_OP_TLBI_EL2_ALL;
  ConstructTlbiCmd (Cmd, CmdOpcode);

  Status = IssueCmdToSmmuV3Controller (Private, Cmd);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to invalidate EL2 TLB entries\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  // Invalidate Non-secure Non-Hypervisor TLB entries
  CmdOpcode = SMMU_V3_OP_TLBI_NSNH_ALL;
  ConstructTlbiCmd (Cmd, CmdOpcode);

  Status = IssueCmdToSmmuV3Controller (Private, Cmd);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to invalidate NSNH TLB entries\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

/**
  Creates a stream table entry (STE) configured for bypass mode.

  This function configures an STE to bypass translation and use incoming
  transaction attributes for:
  - Memory attributes (MTCFG)
  - Allocation hints (ALLOCCFG)
  - Shareability (SHCFG)
  - Security state (NSCFG)
  - Privilege (PRIVCFG)
  - Instruction fetch (INSTCFG)

  @param[out] Ste    Pointer to STE buffer to populate.
                     Must be 8 UINT64s in size.

  @retval None
**/
STATIC
VOID
CreateBypassSte (
  OUT UINT64  *Ste
  )
{
  if (Ste == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter\n", __FUNCTION__));
    return;
  }

  // Set STE to bypass mode
  Ste[0] = SMMU_V3_STE_VALID;
  Ste[0] = Ste[0] | BIT_FIELD_SET (SMMU_V3_STE_CFG_BYPASS, SMMU_V3_STE_CFG_MASK, SMMU_V3_STE_CFG_SHIFT);
  Ste[1] = BIT_FIELD_SET (SMMU_V3_USE_INCOMING_ATTR, SMMU_V3_STE_MTCFG_MASK, SMMU_V3_STE_MTCFG_SHIFT);
  Ste[1] = Ste[1] | BIT_FIELD_SET (SMMU_V3_USE_INCOMING_ATTR, SMMU_V3_STE_ALLOCCFG_MASK, SMMU_V3_STE_ALLOCCFG_SHIFT);
  Ste[1] = Ste[1] | BIT_FIELD_SET (SMMU_V3_USE_INCOMING_SH_ATTR, SMMU_V3_STE_SHCFG_MASK, SMMU_V3_STE_SHCFG_SHIFT);
  Ste[1] = Ste[1] | BIT_FIELD_SET (SMMU_V3_USE_INCOMING_ATTR, SMMU_V3_STE_NSCFG_MASK, SMMU_V3_STE_NSCFG_SHIFT);
  Ste[1] = Ste[1] | BIT_FIELD_SET (SMMU_V3_USE_INCOMING_ATTR, SMMU_V3_STE_PRIVCFG_MASK, SMMU_V3_STE_PRIVCFG_SHIFT);
  Ste[1] = Ste[1] | BIT_FIELD_SET (SMMU_V3_USE_INCOMING_ATTR, SMMU_V3_STE_INSTCFG_MASK, SMMU_V3_STE_INSTCFG_SHIFT);
  Ste[2] = 0;
  Ste[3] = 0;
  Ste[4] = 0;
  Ste[5] = 0;
  Ste[6] = 0;
  Ste[7] = 0;
}

/**
  Sets up default translation behavior for SMMUv3 streams.

  This function configures all stream table entries (STEs) to bypass translation
  and use incoming attributes. After configuration, it invalidates cached entries.

  @param[in]  Private       Pointer to the SMMU_V3_CONTROLLER_PRIVATE_DATA instance.

  @retval None
**/
STATIC
VOID
Smmuv3DefaultTranslation (
  IN  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;
  UINT32      Index;
  UINT32      SteCount;
  UINT64      SteData[SMMU_V3_STRTAB_ENTRY_SIZE_DW];
  UINT64      *SteAddr;

  if (Private == NULL) {
    DEBUG ((DEBUG_ERROR, "%a: Invalid parameter\n", __FUNCTION__));
    return;
  }

  SteCount = (1 << Private->Features.StreamNBits);
  CreateBypassSte (SteData);
  SteAddr = (UINT64 *)Private->SteBase;

  // Populate all stream table entries
  for (Index = 0; Index < SteCount; Index++) {
    WriteSte (SteAddr, SteData);
    SteAddr += SMMU_V3_STRTAB_ENTRY_SIZE_DW;
  }

  /* After an SMMU configuration structure, such as STE, is altered in any way, an invalidation command
   * must be issued to ensure any cached copies of stale configuration are discarded.
   */
  Status = InvalCachedCfgs (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to invalidate config caches - %r\n", __FUNCTION__, Status));
    ASSERT_EFI_ERROR (Status);
  }
}

/**
  Enables the SMMUv3 controller.

  This function:
  1. Enables Command Queue
  2. Enables Event Queue
  3. Invalidates configuration caches and TLBs
  4. Sets up default translation behavior
  5. Enables SMMU translation

  @param[in]  Private       Pointer to the SMMU_V3_CONTROLLER_PRIVATE_DATA instance.

  @retval EFI_SUCCESS              SMMUv3 controller enabled successfully.
  @retval EFI_INVALID_PARAMETER    Private is NULL.
  @retval EFI_TIMEOUT             Timeout waiting for queue/SMMU enable.
  @retval EFI_DEVICE_ERROR        Failed to invalidate caches/TLBs.
**/
STATIC
EFI_STATUS
EFIAPI
EnableSmmuV3 (
  IN  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  TrackCmdqIdx (Private);

  // Enable Command Queue
  MmioBitFieldWrite32 (
    Private->BaseAddress + SMMU_V3_CR0_OFFSET,
    SMMU_V3_CMDQEN_BIT,
    SMMU_V3_CMDQEN_BIT,
    SMMU_V3_Q_ENABLE
    );

  // Wait for the controller to enable command queue
  gBS->Stall (10000);
  if (((MmioRead32 (Private->BaseAddress + SMMU_V3_CR0ACK_OFFSET) >> SMMU_V3_CR0ACK_CMDQEN_SHIFT) & SMMU_V3_CR0ACK_CMDQEN_MASK) != SMMU_V3_Q_ENABLE) {
    return EFI_TIMEOUT;
  }

  // Enable Event Queue
  MmioBitFieldWrite32 (
    Private->BaseAddress + SMMU_V3_CR0_OFFSET,
    SMMU_V3_EVTQEN_BIT,
    SMMU_V3_EVTQEN_BIT,
    SMMU_V3_Q_ENABLE
    );

  // Wait for the controller to enable event queue
  gBS->Stall (10000);
  if (((MmioRead32 (Private->BaseAddress + SMMU_V3_CR0ACK_OFFSET) >> SMMU_V3_CR0ACK_EVTQEN_SHIFT) & SMMU_V3_CR0ACK_EVTQEN_MASK) != SMMU_V3_Q_ENABLE) {
    return EFI_TIMEOUT;
  }

  // Invalidate cached configurations and TLBs
  Status = InvalidateCachedCfgsTlbs (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Failed to invalidate cached configurations and TLBs\n", __FUNCTION__));
    return EFI_DEVICE_ERROR;
  }

  Smmuv3DefaultTranslation (Private);

  // Enable SMMUv3 translation
  MmioBitFieldWrite32 (
    Private->BaseAddress + SMMU_V3_CR0_OFFSET,
    SMMU_V3_CR0_SMMUEN_BIT,
    SMMU_V3_CR0_SMMUEN_BIT,
    SMMU_V3_ENABLE
    );

  // Wait for smmu to enable
  gBS->Stall (10000);
  if (((MmioRead32 (Private->BaseAddress + SMMU_V3_CR0ACK_OFFSET) >> SMMU_V3_CR0ACK_SMMUEN_SHIFT) & SMMU_V3_CR0ACK_SMMUEN_MASK) != SMMU_V3_ENABLE) {
    return EFI_TIMEOUT;
  }

  TrackCmdqIdx (Private);
  return EFI_SUCCESS;
}

/**
  Initialize the SMMUv3 controller.

  @param[in]  Private       Pointer to the SMMU_V3_CONTROLLER_PRIVATE_DATA instance.

  @retval EFI_SUCCESS              The SMMUv3 was initialized successfully.
  @retval EFI_INVALID_PARAMETER    Private is NULL.
  @retval EFI_DEVICE_ERROR        The SMMUv3 hardware initialization failed.
**/
EFI_STATUS
EFIAPI
InitializeSmmuV3 (
  IN  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private
  )
{
  EFI_STATUS  Status;

  if (Private == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  DEBUG ((DEBUG_INFO, "%a: Initializing SMMUv3 at 0x%lx\n", __FUNCTION__, Private->BaseAddress));

  Status = ResetSmmuV3Controller (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to reset SMMUv3\n", __FUNCTION__));
    return Status;
  }

  // TODO: Implement SMMUv3 initialization steps:
  // 1. Check hardware status
  // 2. Configure global settings
  // 3. Setup command queue
  // 4. Setup event queue
  // 5. Enable SMMU operation
  Status = ConfigureSmmuV3ControllerSettings (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to configure SMMUv3 settings\n", __FUNCTION__));
    return Status;
  }

  Status = SetupSmmuV3Cmdq (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to setup SMMUv3 command queue\n", __FUNCTION__));
    return Status;
  }

  Status = SetupSmmuV3Evtq (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to setup SMMUv3 event queue\n", __FUNCTION__));
    return Status;
  }

  Status = SetupSmmuV3StrTable (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to setup SMMUv3 stream table\n", __FUNCTION__));
    return Status;
  }

  Status = EnableSmmuV3 (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to enable SMMUv3\n", __FUNCTION__));
    return Status;
  }

  // Temporary placeholder - just return success
  return EFI_SUCCESS;
}

/**
  Set IOMMU attribute for a system memory.

  If the IOMMU protocol exists, the system memory cannot be used
  for DMA by default.

  When a device requests a DMA access for a system memory,
  the device driver need use SetAttribute() to update the IOMMU
  attribute to request DMA access (read and/or write).

  The DeviceHandle is used to identify which device submits the request.
  The IOMMU implementation need translate the device path to an IOMMU device ID,
  and set IOMMU hardware register accordingly.
  1) DeviceHandle can be a standard PCI device.
     The memory for BusMasterRead need set EDKII_IOMMU_ACCESS_READ.
     The memory for BusMasterWrite need set EDKII_IOMMU_ACCESS_WRITE.
     The memory for BusMasterCommonBuffer need set EDKII_IOMMU_ACCESS_READ|EDKII_IOMMU_ACCESS_WRITE.
     After the memory is used, the memory need set 0 to keep it being protected.
  2) DeviceHandle can be an ACPI device (ISA, I2C, SPI, etc).
     The memory for DMA access need set EDKII_IOMMU_ACCESS_READ and/or EDKII_IOMMU_ACCESS_WRITE.

  @param[in]  This              The protocol instance pointer.
  @param[in]  DeviceHandle      The device who initiates the DMA access request.
  @param[in]  Mapping           The mapping value returned from Map().
  @param[in]  IoMmuAccess       The IOMMU access.

  @retval EFI_SUCCESS            The IoMmuAccess is set for the memory range specified by DeviceAddress and Length.
  @retval EFI_INVALID_PARAMETER  DeviceHandle is an invalid handle.
  @retval EFI_INVALID_PARAMETER  Mapping is not a value that was returned by Map().
  @retval EFI_INVALID_PARAMETER  IoMmuAccess specified an illegal combination of access.
  @retval EFI_UNSUPPORTED        DeviceHandle is unknown by the IOMMU.
  @retval EFI_UNSUPPORTED        The bit mask of IoMmuAccess is not supported by the IOMMU.
  @retval EFI_UNSUPPORTED        The IOMMU does not support the memory range specified by Mapping.
  @retval EFI_OUT_OF_RESOURCES   There are not enough resources available to modify the IOMMU access.
  @retval EFI_DEVICE_ERROR       The IOMMU device reported an error while attempting the operation.

**/
EFI_STATUS
EFIAPI
SetAttributeSmmuV3 (
  IN EDKII_IOMMU_PROTOCOL  *This,
  IN EFI_HANDLE            DeviceHandle,
  IN VOID                  *Mapping,
  IN UINT64                IoMmuAccess
  )
{
  return EFI_DEVICE_ERROR;
}

/**
  Provides the controller-specific addresses required to access system memory from a
  DMA bus master.

  @param  This                  The protocol instance pointer.
  @param  Operation             Indicates if the bus master is going to read or write to system memory.
  @param  HostAddress           The system memory address to map to the PCI controller.
  @param  NumberOfBytes         On input the number of bytes to map. On output the number of bytes
                                that were mapped.
  @param  DeviceAddress         The resulting map address for the bus master PCI controller to use to
                                access the hosts HostAddress.
  @param  Mapping               A resulting value to pass to Unmap().

  @retval EFI_SUCCESS           The range was mapped for the returned NumberOfBytes.
  @retval EFI_UNSUPPORTED       The HostAddress cannot be mapped as a common buffer.
  @retval EFI_INVALID_PARAMETER One or more parameters are invalid.
  @retval EFI_OUT_OF_RESOURCES  The request could not be completed due to a lack of resources.
  @retval EFI_DEVICE_ERROR      The system hardware could not map the requested address.

**/
EFI_STATUS
EFIAPI
MapSmmuV3 (
  IN     EDKII_IOMMU_PROTOCOL   *This,
  IN     EDKII_IOMMU_OPERATION  Operation,
  IN     VOID                   *HostAddress,
  IN OUT UINTN                  *NumberOfBytes,
  OUT    EFI_PHYSICAL_ADDRESS   *DeviceAddress,
  OUT    VOID                   **Mapping
  )
{
  return EFI_DEVICE_ERROR;
}

/**
  Completes the Map() operation and releases any corresponding resources.

  @param  This                  The protocol instance pointer.
  @param  Mapping               The mapping value returned from Map().

  @retval EFI_SUCCESS           The range was unmapped.
  @retval EFI_INVALID_PARAMETER Mapping is not a value that was returned by Map().
  @retval EFI_DEVICE_ERROR      The data was not committed to the target system memory.
**/
EFI_STATUS
EFIAPI
UnmapSmmuV3 (
  IN  EDKII_IOMMU_PROTOCOL  *This,
  IN  VOID                  *Mapping
  )
{
  return EFI_DEVICE_ERROR;
}

/**
  Exit Boot Services Event notification handler.

  @param[in]  Event     Event whose notification function is being invoked.
  @param[in]  Context   Pointer to the notification function's context.

**/
VOID
EFIAPI
OnReadyToBoot (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private;
  UINT32                           GbpSetting;

  gBS->CloseEvent (Event);

  Private = (SMMU_V3_CONTROLLER_PRIVATE_DATA *)Context;

  // TODO: Implement SMMUv3 exit boot services steps:
  // 1. Disable SMMU operation
  // 2. Disable command queue
  // 3. Disable event queue
  // 4. Disable SMMU operation
  if (Private == NULL) {
    return;
  }

  if (Private->CmdQueue.QBase != 0) {
    FreePages ((VOID *)Private->CmdQueue.QBase, EFI_SIZE_TO_PAGES ((1 << Private->Features.CmdqEntriesLog2) * SMMU_V3_CMD_SIZE));
  }

  if (Private->EvtQueue.QBase != 0) {
    FreePages ((VOID *)Private->EvtQueue.QBase, EFI_SIZE_TO_PAGES ((1 << Private->Features.EvtqEntriesLog2) * SMMU_V3_EVT_RECORD_SIZE));
  }

  if (Private->SteBase != 0) {
    FreePages ((VOID *)Private->SteBase, EFI_SIZE_TO_PAGES ((1 << Private->Features.StreamNBits) * SMMU_V3_STRTAB_ENTRY_SIZE));
  }

  // Disable Command Queue
  MmioBitFieldWrite32 (
    Private->BaseAddress + SMMU_V3_CR0_OFFSET,
    SMMU_V3_CMDQEN_BIT,
    SMMU_V3_CMDQEN_BIT,
    SMMU_V3_Q_DISABLE
    );

  // Wait for the controller to disable command queue
  gBS->Stall (10000);
  if (((MmioRead32 (Private->BaseAddress + SMMU_V3_CR0ACK_OFFSET) >> SMMU_V3_CR0ACK_CMDQEN_SHIFT) & SMMU_V3_CR0ACK_CMDQEN_MASK) != SMMU_V3_Q_DISABLE) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to disable command queue 0x%lx\n", __FUNCTION__, Private->BaseAddress));
    return;
  }

  // Disable Event Queue
  MmioBitFieldWrite32 (
    Private->BaseAddress + SMMU_V3_CR0_OFFSET,
    SMMU_V3_EVTQEN_BIT,
    SMMU_V3_EVTQEN_BIT,
    SMMU_V3_Q_DISABLE
    );

  // Wait for the controller to disable event queue
  gBS->Stall (10000);
  if (((MmioRead32 (Private->BaseAddress + SMMU_V3_CR0ACK_OFFSET) >> SMMU_V3_CR0ACK_EVTQEN_SHIFT) & SMMU_V3_CR0ACK_EVTQEN_MASK) != SMMU_V3_Q_DISABLE) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to disable event queue 0x%lx\n", __FUNCTION__, Private->BaseAddress));
    return;
  }

  // Set the controller in global bypass mode
  GbpSetting = BIT_FIELD_SET (1U, SMMU_V3_GBPA_UPDATE_MASK, SMMU_V3_GBPA_UPDATE_SHIFT);
  GbpSetting = GbpSetting | BIT_FIELD_SET (0, SMMU_V3_GBPA_ABORT_MASK, SMMU_V3_GBPA_ABORT_SHIFT);
  GbpSetting = GbpSetting | BIT_FIELD_SET (0, SMMU_V3_GBPA_INSTCFG_MASK, SMMU_V3_GBPA_INSTCFG_SHIFT);
  GbpSetting = GbpSetting | BIT_FIELD_SET (0, SMMU_V3_GBPA_PRIVCFG_MASK, SMMU_V3_GBPA_PRIVCFG_SHIFT);
  GbpSetting = GbpSetting | BIT_FIELD_SET (1, SMMU_V3_GBPA_SHCFG_MASK, SMMU_V3_GBPA_SHCFG_SHIFT);
  GbpSetting = GbpSetting | BIT_FIELD_SET (0, SMMU_V3_GBPA_ALLOCFG_MASK, SMMU_V3_GBPA_ALLOCFG_SHIFT);
  GbpSetting = GbpSetting | BIT_FIELD_SET (0, SMMU_V3_GBPA_MTCFG_MASK, SMMU_V3_GBPA_MTCFG_SHIFT);
  MmioWrite32 (Private->BaseAddress + SMMU_V3_GBPA_OFFSET, GbpSetting);

  // Wait for the controller to enter global bypass mode
  gBS->Stall (10000);
  if (((MmioRead32 (Private->BaseAddress + SMMU_V3_GBPA_OFFSET) >> SMMU_V3_GBPA_UPDATE_SHIFT) & SMMU_V3_GBPA_UPDATE_MASK) == 1) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to put SMMU in global bypass mode 0x%lx\n", __FUNCTION__, Private->BaseAddress));
    return;
  }

  MmioBitFieldWrite32 (
    Private->BaseAddress + SMMU_V3_CR0_OFFSET,
    SMMU_V3_CR0_SMMUEN_BIT,
    SMMU_V3_CR0_SMMUEN_BIT,
    SMMU_V3_DISABLE
    );

  // Wait for the controller to disable SMMU operation
  gBS->Stall (10000);
  if (((MmioRead32 (Private->BaseAddress + SMMU_V3_CR0ACK_OFFSET) >> SMMU_V3_CR0ACK_SMMUEN_SHIFT) & SMMU_V3_CR0ACK_SMMUEN_MASK) != SMMU_V3_DISABLE) {
    DEBUG ((DEBUG_ERROR, "%a: Unable disable SMMU 0x%lx\n", __FUNCTION__, Private->BaseAddress));
    return;
  }

  FreePool (Private);
}

/**
  Clean up SMMUv3 controller resources.

  @param[in]  Private       Pointer to the SMMU_V3_CONTROLLER_PRIVATE_DATA instance.

 **/
STATIC
VOID
Smmuv3Cleanup (
  IN  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private
  )
{
  if (Private == NULL) {
    return;
  }

  if (Private->CmdQueue.QBase != 0) {
    FreePages ((VOID *)Private->CmdQueue.QBase, EFI_SIZE_TO_PAGES ((1 << Private->Features.CmdqEntriesLog2) * SMMU_V3_CMD_SIZE));
  }

  if (Private->EvtQueue.QBase != 0) {
    FreePages ((VOID *)Private->EvtQueue.QBase, EFI_SIZE_TO_PAGES ((1 << Private->Features.EvtqEntriesLog2) * SMMU_V3_EVT_RECORD_SIZE));
  }

  if (Private->SteBase != 0) {
    FreePages ((VOID *)Private->SteBase, EFI_SIZE_TO_PAGES ((1 << Private->Features.StreamNBits) * SMMU_V3_STRTAB_ENTRY_SIZE));
  }

  if (Private->ReadyToBootEvent != NULL) {
    gBS->CloseEvent (Private->ReadyToBootEvent);
  }

  FreePool (Private);
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
  IN  NVIDIA_DEVICE_DISCOVERY_PHASES          Phase,
  IN  EFI_HANDLE                              DriverHandle,
  IN  EFI_HANDLE                              ControllerHandle,
  IN  CONST NVIDIA_DEVICE_TREE_NODE_PROTOCOL  *DeviceTreeNode OPTIONAL
  )
{
  EFI_STATUS                       Status;
  EFI_PHYSICAL_ADDRESS             BaseAddress;
  UINTN                            RegionSize;
  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private;
  UINT32                           NodeHandle;

  Status      = EFI_SUCCESS;
  BaseAddress = 0;
  RegionSize  = 0;
  Private     = NULL;
  NodeHandle  = 0;

  switch (Phase) {
    case DeviceDiscoveryDriverBindingSupported:
      Status = DeviceTreeGetNodePHandle (DeviceTreeNode->NodeOffset, &NodeHandle);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to get phandle for node\n", __FUNCTION__));
        goto Exit;
      }

      break;

    case DeviceDiscoveryDriverBindingStart:
      Status = DeviceDiscoveryGetMmioRegion (ControllerHandle, 0, &BaseAddress, &RegionSize);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to locate address range\n", __FUNCTION__));
        goto Exit;
      }

      Private = AllocateZeroPool (sizeof (SMMU_V3_CONTROLLER_PRIVATE_DATA));
      if (Private == NULL) {
        Status = EFI_OUT_OF_RESOURCES;
        goto Exit;
      }

      Private->Signature      = SMMU_V3_CONTROLLER_SIGNATURE;
      Private->BaseAddress    = BaseAddress;
      Private->DeviceTreeBase = DeviceTreeNode->DeviceTreeBase;
      Private->NodeOffset     = DeviceTreeNode->NodeOffset;

      Status = DeviceTreeGetNodePHandle (DeviceTreeNode->NodeOffset, &Private->SmmuV3ControllerProtocol.PHandle);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to get phandle for node\n", __FUNCTION__));
        goto Exit;
      }

      DEBUG ((DEBUG_ERROR, "%a: Base Addr 0x%lx\n", __FUNCTION__, Private->BaseAddress));
      DEBUG ((DEBUG_ERROR, "%a: PHandle 0x%lx\n", __FUNCTION__, Private->SmmuV3ControllerProtocol.PHandle));

      Status = InitializeSmmuV3 (Private);
      if (EFI_ERROR (Status)) {
        DEBUG ((DEBUG_ERROR, "%a: Unable to initialize SMMUv3\n", __FUNCTION__));
        goto Exit;
      }

      // Create an event to notify when the system is ready to exit boot services.
      Status = gBS->CreateEventEx (
                      EVT_NOTIFY_SIGNAL,
                      TPL_NOTIFY,
                      OnReadyToBoot,
                      Private,
                      &gEfiEventReadyToBootGuid,
                      &Private->ReadyToBootEvent
                      );
      if (EFI_ERROR (Status)) {
        goto Exit;
      }

      // Install the SMMUv3 protocol.
      Status = gBS->InstallMultipleProtocolInterfaces (
                      &ControllerHandle,
                      &gNVIDIASmmuV3ProtocolGuid,
                      &Private->SmmuV3ControllerProtocol,
                      NULL
                      );
      if (EFI_ERROR (Status)) {
        goto Exit;
      }

      break;

    default:
      break;
  }

Exit:
  if (EFI_ERROR (Status) && (Private != NULL)) {
    Smmuv3Cleanup (Private);
  }

  return Status;
}
