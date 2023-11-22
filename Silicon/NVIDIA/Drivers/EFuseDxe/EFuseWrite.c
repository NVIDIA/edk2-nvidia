/** @file

  EFUSE Write functions

  SPDX-FileCopyrightText: Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>
#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/TimerLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>
#include <Library/DeviceDiscoveryDriverLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/TegraPlatformInfoLib.h>
#include <Protocol/ResetNodeProtocol.h>
#include <libfdt.h>
#include <Protocol/DeviceTreeCompatibility.h>
#include <T194/T194Definitions.h>
#include "EFuseDxePrivate.h"

STATIC UINT32                FuseWord;
STATIC EFI_PHYSICAL_ADDRESS  EFuseRegBaseAddress;

#define PmcMiscRead(reg)        MmioRead32 (((UINT32)T194_PMC_MISC_BASE_ADDR + (UINT32)(reg)))
#define PmcMiscWrite(reg, val)  MmioWrite32 (((UINT32)T194_PMC_MISC_BASE_ADDR + (UINT32)(reg)), (val))
#define FuseRead(reg)           MmioRead32 (((UINT32)EFuseRegBaseAddress + (UINT32)(reg)))
#define FuseWrite(reg, val)     MmioWrite32 (((UINT32)EFuseRegBaseAddress + (UINT32)(reg)), (val))

#define WriteFuseWord0(name, data)                                                      \
{                                                                                       \
        FuseWord = ((UINT32)(name##_ADDR_0_MASK) & data) << (name##_ADDR_0_SHIFT);      \
}

#define WriteFuseWord1(name, data)                                                      \
{                                                                                       \
        FuseWord = ((UINT64)(name##_ADDR_1_MASK) & data) >> (name##_ADDR_1_SHIFT);      \
}

#define WriteOdmFuse(name, data)                                                        \
{                                                                                       \
        WriteFuseWord0(name, data)                                                      \
        Status = FuseBurn (name##_ADDR_0);                                              \
        if (Status) {                                                                   \
          goto fail;                                                                    \
        }                                                                               \
        WriteFuseWord1(name, data)                                                      \
        Status = FuseBurn (name##_ADDR_1);                                              \
        if (Status) {                                                                   \
          goto fail;                                                                    \
        }                                                                               \
        WriteFuseWord0(name##_REDUNDANT, data)                                          \
        Status = FuseBurn (name##_REDUNDANT_ADDR_0);                                    \
        if (Status) {                                                                   \
          goto fail;                                                                    \
        }                                                                               \
        WriteFuseWord1(name##_REDUNDANT, data)                                          \
        Status = FuseBurn (name##_REDUNDANT_ADDR_1);                                    \
        if (Status) {                                                                   \
          goto fail;                                                                    \
        }                                                                               \
}

/**
  Confirm if fuse write access is permanently disabled

  @retval TRUE                  register offset is invalide
  @retval FALSE                 register offset is valide
**/
STATIC BOOLEAN
EFuseIsFuseWriteDisabled (
  VOID
  )
{
  UINT32  Value;

  Value = FuseRead (FUSE_DISABLEREGPROGRAM_0);

  if ((Value & FUSE_DISABLEREGPROGRAM_0_VAL_MASK) != 0U) {
    return TRUE;
  } else {
    return FALSE;
  }
}

/**
  Set the fuse strobe programming width
**/
STATIC VOID
EFuseProgramFuseStrobe (
  VOID
  )
{
  UINT32  OscillatorFrequencyKhz = 38400;
  UINT32  OscillatorFrequency;
  UINT32  StrobePulse = FUSE_STROBE_PROGRAMMING_PULSE;
  UINT32  StrobeWidth;
  UINT32  Data;

  OscillatorFrequency = OscillatorFrequencyKhz * 1000U;

  StrobeWidth = (OscillatorFrequency * StrobePulse) / (1000U * 1000U);

  /* Program FUSE_FUSETIME_PGM2_0 with StrobeWidth */
  Data = FuseRead (FUSE_FUSETIME_PGM2_0);
  Data = NV_FLD_SET_DRF_NUM (
           FUSE,
           FUSETIME_PGM2,
           FUSETIME_PGM2_TWIDTH_PGM,
           StrobeWidth,
           Data
           );
  FuseWrite (FUSE_FUSETIME_PGM2_0, Data);
}

/**
  Milli second delay
**/
STATIC VOID
MilliSecondDelay (
  UINT64  Msec
  )
{
  MicroSecondDelay (Msec * 1000LLU);
}

/**
  Assert and set pd

  @param[in]  IsAssert         Assert or not
**/
STATIC VOID
EFuseAssertPd (
  BOOLEAN  IsAssert
  )
{
  UINT32   Data;
  BOOLEAN  PdCtrl = FALSE;

  Data   = FuseRead (FUSE_FUSECTRL_0);
  PdCtrl = NV_DRF_VAL (FUSE, FUSECTRL, FUSECTRL_PD_CTRL, Data) > 0UL;

  if (IsAssert) {
    if (PdCtrl) {
      return;
    } else {
      Data = NV_FLD_SET_DRF_NUM (
               FUSE,
               FUSECTRL,
               FUSECTRL_PD_CTRL,
               0x1,
               Data
               );
      FuseWrite (FUSE_FUSECTRL_0, Data);
      Data = FuseRead (FUSE_FUSECTRL_0);
      MicroSecondDelay (1);
    }
  } else {
    if (!PdCtrl) {
      return;
    } else {
      Data = NV_FLD_SET_DRF_NUM (
               FUSE,
               FUSECTRL,
               FUSECTRL_PD_CTRL,
               0x0,
               Data
               );
      MicroSecondDelay (1);
      FuseWrite (FUSE_FUSECTRL_0, Data);
      Data = FuseRead (FUSE_FUSECTRL_0);
    }
  }
}

/**
  Clear PS18 latch to gate programming voltage
**/
STATIC VOID
EFusePmcFuseControlPs18LatchClear (
  VOID
  )
{
  UINT32  Data;

  Data = PmcMiscRead (PMC_MISC_FUSE_CONTROL_0);
  Data = NV_FLD_SET_DRF_NUM (
           PMC_MISC,
           FUSE_CONTROL,
           PS18_LATCH_SET,
           0,
           Data
           );
  PmcMiscWrite (PMC_MISC_FUSE_CONTROL_0, Data);
  MilliSecondDelay (1);

  Data = NV_FLD_SET_DRF_NUM (
           PMC_MISC,
           FUSE_CONTROL,
           PS18_LATCH_CLEAR,
           1,
           Data
           );
  PmcMiscWrite (PMC_MISC_FUSE_CONTROL_0, Data);
  MilliSecondDelay (1);
}

/**
  Assert ps18 to enable programming voltage
**/
STATIC VOID
EFusePmcFuseControlPs18LatchSet (
  VOID
  )
{
  UINT32  Data;

  Data = PmcMiscRead (PMC_MISC_FUSE_CONTROL_0);
  Data = NV_FLD_SET_DRF_NUM (PMC_MISC, FUSE_CONTROL, PS18_LATCH_CLEAR, 0, Data);
  PmcMiscWrite (PMC_MISC_FUSE_CONTROL_0, Data);
  MilliSecondDelay (1);

  Data = NV_FLD_SET_DRF_NUM (PMC_MISC, FUSE_CONTROL, PS18_LATCH_SET, 1, Data);
  PmcMiscWrite (PMC_MISC_FUSE_CONTROL_0, Data);
  MilliSecondDelay (1);
}

/**
  Check if fuse mirroring is enable

  @param[out]  IsEnable         Fuse mirroring is enable or not
**/
STATIC VOID
EFuseProgramMirroring (
  BOOLEAN  IsEnable
  )
{
  UINT32   Data;
  UINT32   Reg;
  BOOLEAN  DisableMirror;

  Data = PmcMiscRead (PMC_MISC_FUSE_CONTROL_0);
  if ((Data & PMC_FUSE_CTRL_ENABLE_REDIRECTION_STICKY) != 0U) {
    DisableMirror = (IsEnable ? FALSE : TRUE);
    Reg           = FuseRead (FUSE_FUSECTRL_0);
    Reg           = NV_FLD_SET_DRF_NUM (FUSE, FUSECTRL, FUSECTRL_DISABLE_MIRROR, DisableMirror, Reg);
    FuseWrite (FUSE_FUSECTRL_0, Reg);
    do {
      Reg = FuseRead (FUSE_FUSECTRL_0);
    } while (NV_DRF_VAL (FUSE, FUSECTRL, FUSECTRL_STATE, Reg) != FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE);
  } else {
    Data = NV_FLD_SET_DRF_NUM (PMC_MISC, FUSE_CONTROL, ENABLE_REDIRECTION, IsEnable, Data);
    PmcMiscWrite (PMC_MISC_FUSE_CONTROL_0, Data);
  }
}

/**
  Fuse burn setup

  @param[in]  Enable            Enable or disable fuse burn
**/
STATIC VOID
FuseBurnSetup (
  BOOLEAN  Enable
  )
{
  if (Enable == TRUE) {
    /* Disable fuse mirroring and set PD to 0, wait for the required setup time
     * This insures that the fuse macro is not power gated
     */
    EFuseProgramMirroring (FALSE);
    EFuseAssertPd (FALSE);

    /* Assert ps18 to enable programming voltage */
    EFusePmcFuseControlPs18LatchSet ();
  } else {
    /* Clear PS18 latch to gate programming voltage */
    EFusePmcFuseControlPs18LatchClear ();

    /* Enable back fuse mirroring and set PD to 1,
     * wait for the required setup time
     */
    EFuseProgramMirroring (TRUE);
    EFuseAssertPd (TRUE);
  }
}

/**
  pre fuse write process

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.
**/
STATIC EFI_STATUS
EFuseWritePreProcess (
  VOID
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  UINT32      Data;

  /* Confirm fuse option write access hasn't already
   * been permanently disabled
   */
  if (EFuseIsFuseWriteDisabled ()) {
    DEBUG ((DEBUG_ERROR, "Fuse write is permanently disable.\n"));
    Status = EFI_ACCESS_DENIED;
    goto fail;
  }

  /* Enable software writes to fuse registers.*/
  Data = FuseRead (FUSE_WRITE_ACCESS_SW_0);
  Data = NV_FLD_SET_DRF_NUM (
           FUSE,
           WRITE_ACCESS_SW,
           WRITE_ACCESS_SW_CTRL,
           0x1,
           Data
           );
  FuseWrite (FUSE_WRITE_ACCESS_SW_0, Data);

  /* Set the fuse strobe programming width */
  EFuseProgramFuseStrobe ();

  FuseBurnSetup (TRUE);

  /* Make sure the fuse burning voltage is present and stable
   * (Assuming this is already set)
   */

  /* Confirm the fuse wrapper's state machine is idle */
  Data = FuseRead (FUSE_FUSECTRL_0);
  Data = NV_DRF_VAL (FUSE, FUSECTRL, FUSECTRL_STATE, Data);
  if (Data != FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE) {
    DEBUG ((DEBUG_ERROR, "Fuse wrapper's state is not ready.\n"));
    Status = EFI_NOT_READY;
    FuseBurnSetup (FALSE);
    goto fail;
  }

fail:
  return Status;
}

/**
  Post fuse write process
**/
STATIC VOID
EFuseWritePostProcess (
  VOID
  )
{
  UINT32  Data;

  FuseBurnSetup (FALSE);

  /* If desired the newly burned raw fuse values can take effect without
   * a reset, cold boot, or SC7LP0 resume
   */
  Data = FuseRead (FUSE_FUSECTRL_0);
  Data = NV_FLD_SET_DRF_DEF (FUSE, FUSECTRL, FUSECTRL_CMD, SENSE_CTRL, Data);
  FuseWrite (FUSE_FUSECTRL_0, Data);

  /* Wait at least 400ns as per IAS. Waiting 50us here to make sure h/w is
   * stable and eliminate any issue with our timer driver. Since fuse burning
   * is invoked rarely, KPIs doesn't matter here.
   */
  MicroSecondDelay (50);

  /* Poll FUSE_FUSECTRL_0_FUSECTRL_STATE until it reads back STATE_IDLE */
  do {
    Data = FuseRead (FUSE_FUSECTRL_0);
    Data = NV_DRF_VAL (FUSE, FUSECTRL, FUSECTRL_STATE, Data);
  } while (Data != FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE);

  /* Simultaneously set FUSE_PRIV2INTFC_START_0_PRIV2INTFC_START_DATA &
   * _PRIV2INTFC_SKIP_RECORDS
   */
  Data = FuseRead (FUSE_PRIV2INTFC_START_0);
  Data = NV_FLD_SET_DRF_NUM (
           FUSE,
           PRIV2INTFC_START,
           PRIV2INTFC_START_DATA,
           1,
           Data
           );
  Data = NV_FLD_SET_DRF_NUM (
           FUSE,
           PRIV2INTFC_START,
           PRIV2INTFC_SKIP_RECORDS,
           1,
           Data
           );
  FuseWrite (FUSE_PRIV2INTFC_START_0, Data);

  /* Wait at least 400ns as per IAS. Waiting 50us here to make sure h/w is
   * stable and eliminate any issue with our timer driver. Since fuse burning
   * is invoked rarely, KPIs doesn't matter here.
   */
  MicroSecondDelay (50);

  /* Poll FUSE_FUSECTRL_0 until both FUSECTRL_FUSE_SENSE_DONE is set,
   * and FUSECTRL_STATE is STATE_IDLE
   */
  do {
    Data = FuseRead (FUSE_FUSECTRL_0);
    Data = NV_DRF_VAL (FUSE, FUSECTRL, FUSECTRL_FUSE_SENSE_DONE, Data);
  } while (Data == 0U);

  do {
    Data = FuseRead (FUSE_FUSECTRL_0);
    Data = NV_DRF_VAL (FUSE, FUSECTRL, FUSECTRL_STATE, Data);
  } while (Data != FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE);
}

/**
  Initiate the fuse burn
**/
STATIC VOID
EFuseInitiateBurn (
  VOID
  )
{
  UINT32  Data;

  /* Initiate the fuse burn */
  Data = FuseRead (FUSE_FUSECTRL_0);
  Data = NV_FLD_SET_DRF_DEF (FUSE, FUSECTRL, FUSECTRL_CMD, WRITE, Data);
  FuseWrite (FUSE_FUSECTRL_0, Data);

  /* Wait at least 400ns as per IAS. Waiting 50us here to make sure h/w is
   * stable and eliminate any issue with our timer driver. Since fuse burning
   * is invoked rarely, KPIs doesn't matter here.
   */
  MicroSecondDelay (50);

  /* Wait for the fuse burn to complete */
  do {
    Data = FuseRead (FUSE_FUSECTRL_0);
    Data = NV_DRF_VAL (FUSE, FUSECTRL, FUSECTRL_STATE, Data);
  } while (Data != FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE);

  /* check that the correct Data has been burned correctly
   * by reading back the data
   */
  do {
    Data = FuseRead (FUSE_FUSECTRL_0);
    Data = NV_DRF_VAL (FUSE, FUSECTRL, FUSECTRL_STATE, Data);
  } while (Data != FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE);

  Data = FuseRead (FUSE_FUSECTRL_0);
  Data = NV_FLD_SET_DRF_DEF (FUSE, FUSECTRL, FUSECTRL_CMD, READ, Data);
  FuseWrite (FUSE_FUSECTRL_0, Data);

  /* Wait at least 400ns as per IAS. Waiting 50us here to make sure h/w is
   * stable and eliminate any issue with our timer driver. Since fuse burning
   * is invoked rarely, KPIs doesn't matter here.
   */
  MicroSecondDelay (50);

  do {
    Data = FuseRead (FUSE_FUSECTRL_0);
    Data = NV_DRF_VAL (FUSE, FUSECTRL, FUSECTRL_STATE, Data);
  } while (Data != FUSE_FUSECTRL_0_FUSECTRL_STATE_STATE_IDLE);

  Data = FuseRead (FUSE_FUSERDATA_0);
}

/**
  Burns the specified fuse.

  @param[in]  RegisterOffset     Offset of the fuse to be burnt

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.
**/
STATIC EFI_STATUS
FuseBurn (
  UINT32  RegisterOffset
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  if (FuseWord == 0U) {
    DEBUG ((DEBUG_INFO, "No need to burn offset: 0x%x\n", RegisterOffset));
    goto fail;
  }

  Status = EFuseWritePreProcess ();
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "Fuse pre process failed: 0x%x\n", Status));
    goto fail;
  }

  /* Set the desired fuse dword address */
  FuseWrite (FUSE_FUSEADDR_0, RegisterOffset);

  /* Set the desired fuses to burn */
  FuseWrite (FUSE_FUSEWDATA_0, FuseWord);

  EFuseInitiateBurn ();

  EFuseWritePostProcess ();

fail:
  return Status;
}

/**
  Check if the register offset is invliad

  @param[in]  RegisterOffset    Offset of the fuse to be check

  @retval TRUE                  register offset is invalide
  @retval FALSE                 register offset is valide
**/
STATIC BOOLEAN
EFuseIsRegisterOffsetInvalid (
  UINT32  RegisterOffset
  )
{
  if (  (RegisterOffset != FUSE_RESERVED_ODM8_0)
     && (RegisterOffset != FUSE_RESERVED_ODM9_0)
     && (RegisterOffset != FUSE_RESERVED_ODM10_0)
     && (RegisterOffset != FUSE_RESERVED_ODM11_0))
  {
    return TRUE;
  } else {
    return FALSE;
  }
}

/**
  Reads the requested fuse into the buffer.

  @param[in]  RegisterOffset     Offset of the fuse to be read
  @param[out] Buffer             Buffer to hold the data read
  @param[in]  Size               Size (in bytes) of the fuse to be read

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.
**/
STATIC EFI_STATUS
EFuseRead (
  UINT32  RegisterOffset,
  UINT32  *Buffer,
  UINT32  Size
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;

  if (EFuseIsRegisterOffsetInvalid (RegisterOffset)) {
    DEBUG ((DEBUG_ERROR, "Invalid fuse offset.\n"));
    Status = EFI_INVALID_PARAMETER;
    goto fail;
  }

  if ((Buffer == NULL) || (Size != sizeof (UINT32))) {
    DEBUG ((DEBUG_ERROR, "Invalid paramters.\n"));
    Status = EFI_INVALID_PARAMETER;
    goto fail;
  }

  switch (RegisterOffset) {
    case FUSE_RESERVED_ODM8_0:
      *Buffer = FuseRead (FUSE_RESERVED_ODM8_0);
      break;
    case FUSE_RESERVED_ODM9_0:
      *Buffer = FuseRead (FUSE_RESERVED_ODM9_0);
      break;
    case FUSE_RESERVED_ODM10_0:
      *Buffer = FuseRead (FUSE_RESERVED_ODM10_0);
      break;
    case FUSE_RESERVED_ODM11_0:
      *Buffer = FuseRead (FUSE_RESERVED_ODM11_0);
      break;
    default:
      DEBUG ((DEBUG_ERROR, "Unsupported to read this offset\n"));
      Status = EFI_UNSUPPORTED;
      break;
  }

fail:
  return Status;
}

/**
  Set macro and burn fuse

  @param[in]  RegisterOffset     Offset of the fuse to be burnt
  @param[in]  Buffer             Buffer data with which the fuse is to be burnt
  @param[in]  Size               Size (in bytes) of the fuse to be burnt

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.
**/
STATIC EFI_STATUS
EFuseSetMacroAndBurn (
  UINT32  RegisterOffset,
  UINT32  *Buffer,
  UINT32  Size
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  UINT32      Value  = 0;

  if (EFuseIsRegisterOffsetInvalid (RegisterOffset)) {
    DEBUG ((DEBUG_ERROR, "Invalid fuse offset.\n"));
    Status = EFI_INVALID_PARAMETER;
    goto fail;
  }

  if ((Buffer == NULL) || (Size != sizeof (UINT32))) {
    DEBUG ((DEBUG_ERROR, "Invalid paramters.\n"));
    Status = EFI_INVALID_PARAMETER;
    goto fail;
  }

  Status = EFuseRead (RegisterOffset, &Value, Size);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "Read fuse failed: 0x%x\n", Status));
    goto fail;
  }

  Buffer[0] = Buffer[0] ^ Value;
  if ((Buffer[0] & Value) != 0U) {
    DEBUG ((DEBUG_ERROR, "Invalid fuse data.\n"));
    Status = EFI_INVALID_PARAMETER;
    goto fail;
  }

  switch (RegisterOffset) {
    case FUSE_RESERVED_ODM8_0:
      WriteOdmFuse (FUSE_RESERVED_ODM8, Buffer[0])
      break;
    case FUSE_RESERVED_ODM9_0:
      WriteOdmFuse (FUSE_RESERVED_ODM9, Buffer[0])
      break;
    case FUSE_RESERVED_ODM10_0:
      WriteOdmFuse (FUSE_RESERVED_ODM10, Buffer[0])
      break;
    case FUSE_RESERVED_ODM11_0:
      WriteOdmFuse (FUSE_RESERVED_ODM11, Buffer[0])
      break;
    default:
      DEBUG ((DEBUG_ERROR, "Invalid register offset\n"));
      Status = EFI_INVALID_PARAMETER;
      break;
  }

fail:
  return Status;
}

/**
  Confirm if fuse is burnt

  @param[in]  RegisterOffset     Offset of the fuse to be burnt
  @param[in]  ValWritten         The Value expected to be burnt
  @param[in]  Size               Size (in bytes) of the fuse to be burnt

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.
**/
STATIC EFI_STATUS
EFuseConfirmBurn (
  UINT32  RegisterOffset,
  UINT32  ValWritten,
  UINT32  Size
  )
{
  UINT32      Value  = 0;
  EFI_STATUS  Status = EFI_SUCCESS;

  if (EFuseIsRegisterOffsetInvalid (RegisterOffset)) {
    DEBUG ((DEBUG_ERROR, "Invalid fuse offset.\n"));
    Status = EFI_INVALID_PARAMETER;
    goto fail;
  }

  if (Size != sizeof (UINT32)) {
    DEBUG ((DEBUG_ERROR, "Invalid paramters.\n"));
    Status = EFI_INVALID_PARAMETER;
    goto fail;
  }

  Status = EFuseRead (RegisterOffset, &Value, Size);
  if (Status) {
    DEBUG ((DEBUG_ERROR, "Read fuse failed: 0x%x\n", Status));
    goto fail;
  }

  if (Value != ValWritten) {
    DEBUG ((DEBUG_ERROR, "Fuse read and write mismatch.\n"));
    Status = EFI_NOT_READY;
  } else {
    DEBUG ((DEBUG_INFO, "Fuse burnt successfully.\n"));
  }

fail:
  return Status;
}

/**
  Burns the desired fuse.

  @param[in]  BaseAddressbase    Base address of fuse register
  @param[in]  RegisterOffset     Offset of the fuse to be burnt
  @param[in]  Buffer             Buffer data with which the fuse is to be burnt
  @param[in]  Size               Size (in bytes) of the fuse to be burnt

  @retval EFI_SUCCESS           function is finished successfully.
  @retval Others                Error occurs.
**/
EFI_STATUS
EFuseWrite (
  EFI_PHYSICAL_ADDRESS  BaseAddress,
  UINT32                RegisterOffset,
  UINT32                *Buffer,
  UINT32                Size
  )
{
  EFI_STATUS  Status = EFI_SUCCESS;
  UINT32      ValBefBurn;

  if (EFuseIsRegisterOffsetInvalid (RegisterOffset)) {
    DEBUG ((DEBUG_ERROR, "Invalid fuse offset.\n"));
    Status = EFI_INVALID_PARAMETER;
    goto fail;
  }

  if ((Buffer == NULL) || (Size != sizeof (UINT32))) {
    DEBUG ((DEBUG_ERROR, "Invalid Paramters.\n"));
    Status = EFI_INVALID_PARAMETER;
    goto fail;
  }

  /* set fuse register base address */
  EFuseRegBaseAddress = BaseAddress;

  ValBefBurn = *Buffer;

  EFusePmcFuseControlPs18LatchSet ();

  Status = EFuseSetMacroAndBurn (RegisterOffset, Buffer, Size);
  if (Status != EFI_SUCCESS) {
    DEBUG ((DEBUG_ERROR, "Write fuse failed: 0x%x\n", Status));
    goto fail;
  }

  /* Wait to make sure fuses are burnt */
  MilliSecondDelay (2);

  EFusePmcFuseControlPs18LatchClear ();

  /* confirm fuses are burnt */
  Status = EFuseConfirmBurn (RegisterOffset, ValBefBurn, Size);
  if (Status) {
    DEBUG ((DEBUG_ERROR, "Write confirm failed: 0x%x\n", Status));
    goto fail;
  }

fail:
  return Status;
}
