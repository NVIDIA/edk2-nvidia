/** @file

  SD MMC Override Driver

  Copyright (c) 2018, NVIDIA Corporation. All rights reserved.

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>
#include <Library/IoLib.h>

#include <Protocol/SdMmcOverride.h>
#include <Protocol/NonDiscoverableDevice.h>

#define SD_MMC_HC_CLOCK_CTRL          0x2C
#define SD_MMC_CLK_CTRL_SD_CLK_EN     BIT2

typedef struct {
  UINT32   TimeoutFreq:6;     // bit 0:5
  UINT32   Reserved:1;        // bit 6
  UINT32   TimeoutUnit:1;     // bit 7
  UINT32   BaseClkFreq:8;     // bit 8:15
  UINT32   MaxBlkLen:2;       // bit 16:17
  UINT32   BusWidth8:1;       // bit 18
  UINT32   Adma2:1;           // bit 19
  UINT32   Reserved2:1;       // bit 20
  UINT32   HighSpeed:1;       // bit 21
  UINT32   Sdma:1;            // bit 22
  UINT32   SuspRes:1;         // bit 23
  UINT32   Voltage33:1;       // bit 24
  UINT32   Voltage30:1;       // bit 25
  UINT32   Voltage18:1;       // bit 26
  UINT32   Reserved3:1;       // bit 27
  UINT32   SysBus64:1;        // bit 28
  UINT32   AsyncInt:1;        // bit 29
  UINT32   SlotType:2;        // bit 30:31
  UINT32   Sdr50:1;           // bit 32
  UINT32   Sdr104:1;          // bit 33
  UINT32   Ddr50:1;           // bit 34
  UINT32   Reserved4:1;       // bit 35
  UINT32   DriverTypeA:1;     // bit 36
  UINT32   DriverTypeC:1;     // bit 37
  UINT32   DriverTypeD:1;     // bit 38
  UINT32   DriverType4:1;     // bit 39
  UINT32   TimerCount:4;      // bit 40:43
  UINT32   Reserved5:1;       // bit 44
  UINT32   TuningSDR50:1;     // bit 45
  UINT32   RetuningMod:2;     // bit 46:47
  UINT32   ClkMultiplier:8;   // bit 48:55
  UINT32   Reserved6:7;       // bit 56:62
  UINT32   Hs400:1;           // bit 63
} SD_MMC_HC_SLOT_CAP;

EFI_STATUS
SdMmcCapability (
  IN      EFI_HANDLE                      ControllerHandle,
  IN      UINT8                           Slot,
  IN  OUT VOID                            *SdMmcHcSlotCapability
  )
{
  SD_MMC_HC_SLOT_CAP *Capability = (SD_MMC_HC_SLOT_CAP *)SdMmcHcSlotCapability;

  if (SdMmcHcSlotCapability == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Capability->Sdr104 = FALSE;
  Capability->Ddr50 = FALSE;
  Capability->HighSpeed = FALSE;
  Capability->Hs400 = FALSE;
  Capability->SlotType = 0x1; //Embedded slot

  return EFI_SUCCESS;
}


EFI_STATUS
SdMmcNotify (
  IN      EFI_HANDLE                      ControllerHandle,
  IN      UINT8                           Slot,
  IN      EDKII_SD_MMC_PHASE_TYPE         PhaseType
  )
{
  UINT64                              SlotBaseAddress  = 0;

  NON_DISCOVERABLE_DEVICE             *Device;
  EFI_STATUS                          Status;
  EFI_ACPI_ADDRESS_SPACE_DESCRIPTOR   *Desc;
  UINT8                               CurrentResource = 0;

  Status = gBS->HandleProtocol (ControllerHandle,
                  &gEdkiiNonDiscoverableDeviceProtocolGuid, (VOID **)&Device);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  //
  // We only support MMIO devices, so iterate over the resources to ensure
  // that they only describe things that we can handle
  //
  for (Desc = Device->Resources; Desc->Desc != ACPI_END_TAG_DESCRIPTOR;
       Desc = (VOID *)((UINT8 *)Desc + Desc->Len + 3)) {
    if (Desc->Desc != ACPI_ADDRESS_SPACE_DESCRIPTOR ||
        Desc->ResType != ACPI_ADDRESS_SPACE_TYPE_MEM) {
      break;
    }
    if (CurrentResource == Slot) {
      SlotBaseAddress = Desc->AddrRangeMin;
      break;
    }
  }

  if (SlotBaseAddress == 0) {
    DEBUG ((DEBUG_ERROR, "SdMmcNotify: Unable to locate address range for slot %d\n", Slot));
    return EFI_UNSUPPORTED;
  }

  if (PhaseType == EdkiiSdMmcInitHostPre) {
    // Scale SDMMC Clock to 102MHz.
  }
  else if (PhaseType == EdkiiSdMmcInitHostPost) {
    // Enable SDMMC Clock again.
    MmioOr32(SlotBaseAddress + SD_MMC_HC_CLOCK_CTRL, SD_MMC_CLK_CTRL_SD_CLK_EN);
  }

  return EFI_SUCCESS;
}


EDKII_SD_MMC_OVERRIDE gSdMmcOverride = {
  EDKII_SD_MMC_OVERRIDE_PROTOCOL_VERSION,
  SdMmcCapability,
  SdMmcNotify
};


EFI_HANDLE  gSdMmcOverrideHandle = NULL;


/**
  Initialize the state information for the SD MMC Override Protocol

  @param  ImageHandle   of the loaded driver
  @param  SystemTable   Pointer to the System Table

  @retval EFI_SUCCESS           Protocol registered
  @retval EFI_OUT_OF_RESOURCES  Cannot allocate protocol data structure
  @retval EFI_DEVICE_ERROR      Hardware problems

**/
EFI_STATUS
SdMmcOverrideInitialize (
  IN EFI_HANDLE         ImageHandle,
  IN EFI_SYSTEM_TABLE   *SystemTable
  )
{
  EFI_STATUS  Status;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &gSdMmcOverrideHandle,
                  &gEdkiiSdMmcOverrideProtocolGuid,
                  &gSdMmcOverride,
                  NULL
                  );
  ASSERT_EFI_ERROR (Status);

  return Status;
}

