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
    0
    );

  // Wait for the controller to disable SMMU operation
  gBS->Stall (10000);
  if (((MmioRead32 (Private->BaseAddress + SMMU_V3_CR0ACK_OFFSET) >> SMMU_V3_CR0_SMMUEN_SHIFT) & SMMU_V3_CR0_SMMUEN_MASK) != 0) {
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
  MmioWrite32 (Private->BaseAddress + SMMU_V3_STRTAB_BASE_OFFSET, StrtabBaseReg);

  // Mark STE as invalid
  Status = InvalidateStes (Private);
  if (EFI_ERROR (Status)) {
    DEBUG ((DEBUG_ERROR, "%a: Unable to invalidate STEs\n", __FUNCTION__));
    return Status;
  }

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

  // Temporary placeholder - just return success
  return EFI_SUCCESS;
}

/**
  Exit Boot Services Event notification handler.

  @param[in]  Event     Event whose notification function is being invoked.
  @param[in]  Context   Pointer to the notification function's context.

**/
VOID
EFIAPI
OnExitBootServices (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  SMMU_V3_CONTROLLER_PRIVATE_DATA  *Private;

  gBS->CloseEvent (Event);

  Private = (SMMU_V3_CONTROLLER_PRIVATE_DATA *)Context;

  // TODO: Implement SMMUv3 exit boot services steps:
  // 1. Disable SMMU operation
  // 2. Disable command queue
  // 3. Disable event queue
  // 4. Disable SMMU operation

  DEBUG ((DEBUG_ERROR, "%a: Put SMMU at 0x%lx back in global bypass\n", __FUNCTION__, Private->BaseAddress));
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

  if (Private->ExitBootServicesEvent != NULL) {
    gBS->CloseEvent (Private->ExitBootServicesEvent);
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
                      OnExitBootServices,
                      Private,
                      &gEfiEventExitBootServicesGuid,
                      &Private->ExitBootServicesEvent
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
