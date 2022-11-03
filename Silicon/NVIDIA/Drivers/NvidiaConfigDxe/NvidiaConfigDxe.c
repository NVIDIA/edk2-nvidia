/** @file
*  NVIDIA Configuration Dxe
*
*  Copyright (c) 2020-2022, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*  Copyright (c) 2017, Linaro, Ltd. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#include <Guid/MdeModuleHii.h>
#include <Guid/HiiPlatformSetupFormset.h>

#include <Protocol/HiiConfigAccess.h>
#include <Protocol/HiiConfigRouting.h>

#include <Library/PrintLib.h>
#include <Library/DebugLib.h>
#include <Library/BaseMemoryLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiRuntimeServicesTableLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/BaseLib.h>
#include <Library/HiiLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DeviceTreeHelperLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiHiiServicesLib.h>
#include <Library/UefiLib.h>

#include <TH500/TH500MB1Configuration.h>
#include "NvidiaConfigHii.h"

#define MAX_VARIABLE_NAME  (256 * sizeof(CHAR16))

extern EFI_GUID  gNVIDIAResourceConfigFormsetGuid;

//
// These are the VFR compiler generated data representing our VFR data.
//
extern UINT8  NvidiaConfigHiiBin[];
extern UINT8  NvidiaConfigDxeStrings[];

// Used to make sure autogen isn't too smart
EFI_STRING_ID  UnusedStringArray[] = {
  STRING_TOKEN (STR_SOCKET0_CONFIG_FORM_TITLE),
  STRING_TOKEN (STR_SOCKET0_CONFIG_FORM_HELP),
  STRING_TOKEN (STR_SOCKET1_CONFIG_FORM_TITLE),
  STRING_TOKEN (STR_SOCKET1_CONFIG_FORM_HELP),
  STRING_TOKEN (STR_SOCKET2_CONFIG_FORM_TITLE),
  STRING_TOKEN (STR_SOCKET2_CONFIG_FORM_HELP),
  STRING_TOKEN (STR_SOCKET3_CONFIG_FORM_TITLE),
  STRING_TOKEN (STR_SOCKET3_CONFIG_FORM_HELP),
  STRING_TOKEN (STR_SPREAD_SPECTRUM_PROMPT),
  STRING_TOKEN (STR_SPREAD_SPECTRUM_HELP),
  STRING_TOKEN (STR_UPHY0_PROMPT),
  STRING_TOKEN (STR_UPHY0_HELP),
  STRING_TOKEN (STR_PCIE_C0_X16),
  STRING_TOKEN (STR_PCIE_C0_X8_C1_X8),
  STRING_TOKEN (STR_UPHY1_PROMPT),
  STRING_TOKEN (STR_UPHY1_HELP),
  STRING_TOKEN (STR_PCIE_C2_X16),
  STRING_TOKEN (STR_PCIE_C2_X8_C3_X8),
  STRING_TOKEN (STR_UPHY2_PROMPT),
  STRING_TOKEN (STR_UPHY2_HELP),
  STRING_TOKEN (STR_PCIE_C4_X16),
  STRING_TOKEN (STR_PCIE_C4_X8_C5_X8),
  STRING_TOKEN (STR_PCIE_C5_X4_NVLINK_X12),
  STRING_TOKEN (STR_UPHY3_PROMPT),
  STRING_TOKEN (STR_UPHY3_HELP),
  STRING_TOKEN (STR_PCIE_C6_X16),
  STRING_TOKEN (STR_PCIE_C6_X8_C7_X8),
  STRING_TOKEN (STR_PCIE_C7_X4_NVLINK_X12),
  STRING_TOKEN (STR_UPHY4_PROMPT),
  STRING_TOKEN (STR_UPHY4_HELP),
  STRING_TOKEN (STR_PCIE_C8_X2),
  STRING_TOKEN (STR_PCIE_C8_X1_USB),
  STRING_TOKEN (STR_UPHY5_PROMPT),
  STRING_TOKEN (STR_UPHY5_HELP),
  STRING_TOKEN (STR_PCIE_C9_X2),
  STRING_TOKEN (STR_PCIE0_CONFIG_FORM_TITLE),
  STRING_TOKEN (STR_PCIE1_CONFIG_FORM_TITLE),
  STRING_TOKEN (STR_PCIE2_CONFIG_FORM_TITLE),
  STRING_TOKEN (STR_PCIE3_CONFIG_FORM_TITLE),
  STRING_TOKEN (STR_PCIE4_CONFIG_FORM_TITLE),
  STRING_TOKEN (STR_PCIE5_CONFIG_FORM_TITLE),
  STRING_TOKEN (STR_PCIE6_CONFIG_FORM_TITLE),
  STRING_TOKEN (STR_PCIE7_CONFIG_FORM_TITLE),
  STRING_TOKEN (STR_PCIE8_CONFIG_FORM_TITLE),
  STRING_TOKEN (STR_PCIE9_CONFIG_FORM_TITLE),
  STRING_TOKEN (STR_PCIE_MAX_SPEED_TITLE),
  STRING_TOKEN (STR_PCIE_MAX_SPEED_HELP),
  STRING_TOKEN (STR_PCIE_GEN5),
  STRING_TOKEN (STR_PCIE_GEN4),
  STRING_TOKEN (STR_PCIE_GEN3),
  STRING_TOKEN (STR_PCIE_GEN2),
  STRING_TOKEN (STR_PCIE_GEN1),
  STRING_TOKEN (STR_PCIE_MAX_WIDTH_TITLE),
  STRING_TOKEN (STR_PCIE_MAX_WIDTH_HELP),
  STRING_TOKEN (STR_PCIE_X16),
  STRING_TOKEN (STR_PCIE_X8),
  STRING_TOKEN (STR_PCIE_X4),
  STRING_TOKEN (STR_PCIE_X2),
  STRING_TOKEN (STR_PCIE_X1),
  STRING_TOKEN (STR_PCIE_ENABLE_ASPM_L1_TITLE),
  STRING_TOKEN (STR_PCIE_ENABLE_ASPM_L1_1_TITLE),
  STRING_TOKEN (STR_PCIE_ENABLE_ASPM_L1_2_TITLE),
  STRING_TOKEN (STR_PCIE_ENABLE_PCIPM_L1_2_TITLE),
  STRING_TOKEN (STR_PCIE_SUPPORTS_CLK_REQ_TITLE),
  STRING_TOKEN (STR_PCIE_SUPPORTS_CLK_REQ_HELP),
};

//
// HII specific Vendor Device Path definition.
//
typedef struct {
  VENDOR_DEVICE_PATH          VendorDevicePath;
  EFI_DEVICE_PATH_PROTOCOL    End;
} HII_VENDOR_DEVICE_PATH;

HII_VENDOR_DEVICE_PATH  mNvidiaConfigHiiVendorDevicePath = {
  {
    {
      HARDWARE_DEVICE_PATH,
      HW_VENDOR_DP,
      {
        (UINT8)(sizeof (VENDOR_DEVICE_PATH)),
        (UINT8)((sizeof (VENDOR_DEVICE_PATH)) >> 8)
      }
    },
    NVIDIA_CONFIG_FORMSET_GUID
  },
  {
    END_DEVICE_PATH_TYPE,
    END_ENTIRE_DEVICE_PATH_SUBTYPE,
    {
      (UINT8)(END_DEVICE_PATH_LENGTH),
      (UINT8)((END_DEVICE_PATH_LENGTH) >> 8)
    }
  }
};

EFI_HII_CONFIG_ACCESS_PROTOCOL  mConfigAccess;
CHAR16                          mHiiControlStorageName[] = L"NVIDIA_CONFIG_HII_CONTROL";
NVIDIA_CONFIG_HII_CONTROL       mHiiControlSettings      = { 0 };
EFI_HANDLE                      mDriverHandle;
TEGRABL_EARLY_BOOT_VARIABLES    mMb1Config = { 0 };

/**
  Syncs settings betweem Control settings and MB1 Config structure

**/
VOID
EFIAPI
SyncHiiSettings (
  IN BOOLEAN  Read
  )
{
  UINTN  Index;

  if (Read) {
    mHiiControlSettings.EgmEnabled           = mMb1Config.Data.Mb1Data.FeatureData.EgmEnable;
    mHiiControlSettings.EgmHvSizeMb          = mMb1Config.Data.Mb1Data.HvRsvdMemSize;
    mHiiControlSettings.SpreadSpectrumEnable = mMb1Config.Data.Mb1Data.FeatureData.SpreadSpecEnable;

    for (Index = 0; Index < TEGRABL_MAX_UPHY_PER_SOCKET; Index++) {
      mHiiControlSettings.UphySetting0[Index] = mMb1Config.Data.Mb1Data.UphyConfig.UphyConfig[0][Index];
      mHiiControlSettings.UphySetting1[Index] = mMb1Config.Data.Mb1Data.UphyConfig.UphyConfig[1][Index];
      mHiiControlSettings.UphySetting2[Index] = mMb1Config.Data.Mb1Data.UphyConfig.UphyConfig[2][Index];
      mHiiControlSettings.UphySetting3[Index] = mMb1Config.Data.Mb1Data.UphyConfig.UphyConfig[3][Index];
    }

    for (Index = 0; Index < TEGRABL_MAX_PCIE_PER_SOCKET; Index++) {
      mHiiControlSettings.MaxSpeed0[Index]         = mMb1Config.Data.Mb1Data.PcieConfig[0][Index].MaxSpeed;
      mHiiControlSettings.MaxWidth0[Index]         = mMb1Config.Data.Mb1Data.PcieConfig[0][Index].MaxWidth;
      mHiiControlSettings.SlotType0[Index]         = mMb1Config.Data.Mb1Data.PcieConfig[0][Index].SlotType;
      mHiiControlSettings.EnableAspmL1_0[Index]    = mMb1Config.Data.Mb1Data.PcieConfig[0][Index].EnableAspmL1;
      mHiiControlSettings.EnableAspmL1_1_0[Index]  = mMb1Config.Data.Mb1Data.PcieConfig[0][Index].EnableAspmL1_1;
      mHiiControlSettings.EnableAspmL1_2_0[Index]  = mMb1Config.Data.Mb1Data.PcieConfig[0][Index].EnableAspmL1_2;
      mHiiControlSettings.EnablePciPmL1_2_0[Index] = mMb1Config.Data.Mb1Data.PcieConfig[0][Index].EnablePciPmL1_2;
      mHiiControlSettings.SupportsClkReq0[Index]   = mMb1Config.Data.Mb1Data.PcieConfig[0][Index].SupportsClkReq;
      mHiiControlSettings.MaxSpeed1[Index]         = mMb1Config.Data.Mb1Data.PcieConfig[1][Index].MaxSpeed;
      mHiiControlSettings.MaxWidth1[Index]         = mMb1Config.Data.Mb1Data.PcieConfig[1][Index].MaxWidth;
      mHiiControlSettings.SlotType1[Index]         = mMb1Config.Data.Mb1Data.PcieConfig[1][Index].SlotType;
      mHiiControlSettings.EnableAspmL1_1[Index]    = mMb1Config.Data.Mb1Data.PcieConfig[1][Index].EnableAspmL1;
      mHiiControlSettings.EnableAspmL1_1_1[Index]  = mMb1Config.Data.Mb1Data.PcieConfig[1][Index].EnableAspmL1_1;
      mHiiControlSettings.EnableAspmL1_2_1[Index]  = mMb1Config.Data.Mb1Data.PcieConfig[1][Index].EnableAspmL1_2;
      mHiiControlSettings.EnablePciPmL1_2_1[Index] = mMb1Config.Data.Mb1Data.PcieConfig[1][Index].EnablePciPmL1_2;
      mHiiControlSettings.SupportsClkReq1[Index]   = mMb1Config.Data.Mb1Data.PcieConfig[1][Index].SupportsClkReq;
      mHiiControlSettings.MaxSpeed2[Index]         = mMb1Config.Data.Mb1Data.PcieConfig[2][Index].MaxSpeed;
      mHiiControlSettings.MaxWidth2[Index]         = mMb1Config.Data.Mb1Data.PcieConfig[2][Index].MaxWidth;
      mHiiControlSettings.SlotType2[Index]         = mMb1Config.Data.Mb1Data.PcieConfig[2][Index].SlotType;
      mHiiControlSettings.EnableAspmL1_2[Index]    = mMb1Config.Data.Mb1Data.PcieConfig[2][Index].EnableAspmL1;
      mHiiControlSettings.EnableAspmL1_1_2[Index]  = mMb1Config.Data.Mb1Data.PcieConfig[2][Index].EnableAspmL1_1;
      mHiiControlSettings.EnableAspmL1_2_2[Index]  = mMb1Config.Data.Mb1Data.PcieConfig[2][Index].EnableAspmL1_2;
      mHiiControlSettings.EnablePciPmL1_2_2[Index] = mMb1Config.Data.Mb1Data.PcieConfig[2][Index].EnablePciPmL1_2;
      mHiiControlSettings.SupportsClkReq2[Index]   = mMb1Config.Data.Mb1Data.PcieConfig[2][Index].SupportsClkReq;
      mHiiControlSettings.MaxSpeed3[Index]         = mMb1Config.Data.Mb1Data.PcieConfig[3][Index].MaxSpeed;
      mHiiControlSettings.MaxWidth3[Index]         = mMb1Config.Data.Mb1Data.PcieConfig[3][Index].MaxWidth;
      mHiiControlSettings.SlotType3[Index]         = mMb1Config.Data.Mb1Data.PcieConfig[3][Index].SlotType;
      mHiiControlSettings.EnableAspmL1_3[Index]    = mMb1Config.Data.Mb1Data.PcieConfig[3][Index].EnableAspmL1;
      mHiiControlSettings.EnableAspmL1_1_3[Index]  = mMb1Config.Data.Mb1Data.PcieConfig[3][Index].EnableAspmL1_1;
      mHiiControlSettings.EnableAspmL1_2_3[Index]  = mMb1Config.Data.Mb1Data.PcieConfig[3][Index].EnableAspmL1_2;
      mHiiControlSettings.EnablePciPmL1_2_3[Index] = mMb1Config.Data.Mb1Data.PcieConfig[3][Index].EnablePciPmL1_2;
      mHiiControlSettings.SupportsClkReq3[Index]   = mMb1Config.Data.Mb1Data.PcieConfig[3][Index].SupportsClkReq;
    }
  } else {
    mMb1Config.Data.Mb1Data.FeatureData.EgmEnable        = mHiiControlSettings.EgmEnabled;
    mMb1Config.Data.Mb1Data.HvRsvdMemSize                = mHiiControlSettings.EgmHvSizeMb;
    mMb1Config.Data.Mb1Data.FeatureData.SpreadSpecEnable = mHiiControlSettings.SpreadSpectrumEnable;

    for (Index = 0; Index < TEGRABL_MAX_UPHY_PER_SOCKET; Index++) {
      mMb1Config.Data.Mb1Data.UphyConfig.UphyConfig[0][Index] = mHiiControlSettings.UphySetting0[Index];
      mMb1Config.Data.Mb1Data.UphyConfig.UphyConfig[1][Index] = mHiiControlSettings.UphySetting1[Index];
      mMb1Config.Data.Mb1Data.UphyConfig.UphyConfig[2][Index] = mHiiControlSettings.UphySetting2[Index];
      mMb1Config.Data.Mb1Data.UphyConfig.UphyConfig[3][Index] = mHiiControlSettings.UphySetting3[Index];
    }

    for (Index = 0; Index < TEGRABL_MAX_PCIE_PER_SOCKET; Index++) {
      mMb1Config.Data.Mb1Data.PcieConfig[0][Index].MaxSpeed        = mHiiControlSettings.MaxSpeed0[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[0][Index].MaxWidth        = mHiiControlSettings.MaxWidth0[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[0][Index].SlotType        = mHiiControlSettings.SlotType0[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[0][Index].EnableAspmL1    = mHiiControlSettings.EnableAspmL1_0[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[0][Index].EnableAspmL1_1  = mHiiControlSettings.EnableAspmL1_1_0[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[0][Index].EnableAspmL1_2  = mHiiControlSettings.EnableAspmL1_2_0[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[0][Index].EnablePciPmL1_2 = mHiiControlSettings.EnablePciPmL1_2_0[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[0][Index].SupportsClkReq  = mHiiControlSettings.SupportsClkReq0[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[1][Index].MaxSpeed        = mHiiControlSettings.MaxSpeed1[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[1][Index].MaxWidth        = mHiiControlSettings.MaxWidth1[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[1][Index].SlotType        = mHiiControlSettings.SlotType1[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[1][Index].EnableAspmL1    = mHiiControlSettings.EnableAspmL1_1[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[1][Index].EnableAspmL1_1  = mHiiControlSettings.EnableAspmL1_1_1[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[1][Index].EnableAspmL1_2  = mHiiControlSettings.EnableAspmL1_2_1[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[1][Index].EnablePciPmL1_2 = mHiiControlSettings.EnablePciPmL1_2_1[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[1][Index].SupportsClkReq  = mHiiControlSettings.SupportsClkReq1[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[2][Index].MaxSpeed        = mHiiControlSettings.MaxSpeed2[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[2][Index].MaxWidth        = mHiiControlSettings.MaxWidth2[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[2][Index].SlotType        = mHiiControlSettings.SlotType2[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[2][Index].EnableAspmL1    = mHiiControlSettings.EnableAspmL1_2[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[2][Index].EnableAspmL1_1  = mHiiControlSettings.EnableAspmL1_1_2[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[2][Index].EnableAspmL1_2  = mHiiControlSettings.EnableAspmL1_2_2[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[2][Index].EnablePciPmL1_2 = mHiiControlSettings.EnablePciPmL1_2_2[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[2][Index].SupportsClkReq  = mHiiControlSettings.SupportsClkReq2[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[3][Index].MaxSpeed        = mHiiControlSettings.MaxSpeed3[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[3][Index].MaxWidth        = mHiiControlSettings.MaxWidth3[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[3][Index].SlotType        = mHiiControlSettings.SlotType3[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[3][Index].EnableAspmL1    = mHiiControlSettings.EnableAspmL1_3[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[3][Index].EnableAspmL1_1  = mHiiControlSettings.EnableAspmL1_1_3[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[3][Index].EnableAspmL1_2  = mHiiControlSettings.EnableAspmL1_2_3[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[3][Index].EnablePciPmL1_2 = mHiiControlSettings.EnablePciPmL1_2_3[Index];
      mMb1Config.Data.Mb1Data.PcieConfig[3][Index].SupportsClkReq  = mHiiControlSettings.SupportsClkReq3[Index];
    }
  }
}

/**
  Initializes any variables to current or default settings

**/
VOID
EFIAPI
InitializeSettings (
  )
{
  EFI_STATUS                  Status;
  VOID                        *AcpiBase;
  NVIDIA_KERNEL_COMMAND_LINE  CmdLine;
  UINTN                       KernelCmdLineLen;
  UINTN                       BufferSize;

  // Initialize PCIe Form Settings
  PcdSet8S (PcdPcieResourceConfigNeeded, PcdGet8 (PcdPcieResourceConfigNeeded));
  PcdSet8S (PcdPcieEntryInAcpiConfigNeeded, PcdGet8 (PcdPcieEntryInAcpiConfigNeeded));
  PcdSet8S (PcdPcieEntryInAcpi, PcdGet8 (PcdPcieEntryInAcpi));
  if (PcdGet8 (PcdPcieResourceConfigNeeded) == 1) {
    Status = EfiGetSystemConfigurationTable (&gEfiAcpiTableGuid, &AcpiBase);
    if (EFI_ERROR (Status)) {
      PcdSet8S (PcdPcieResourceConfigNeeded, 0);
      PcdSet8S (PcdPcieEntryInAcpiConfigNeeded, 0);
    }
  }

  // Initialize Quick Boot Form Settings
  PcdSet8S (PcdQuickBootEnabled, PcdGet8 (PcdQuickBootEnabled));

  // Initialize New Device Hierarchy Form Settings
  PcdSet8S (PcdNewDeviceHierarchy, PcdGet8 (PcdNewDeviceHierarchy));

  // Initialize OS Chain A status Form Settings
  PcdSet32S (PcdOsChainStatusA, PcdGet32 (PcdOsChainStatusA));

  // Initialize OS Chain B status Form Settings
  PcdSet32S (PcdOsChainStatusB, PcdGet32 (PcdOsChainStatusB));

  // Initialize L4T boot mode form settings
  PcdSet32S (PcdL4TDefaultBootMode, PcdGet32 (PcdL4TDefaultBootMode));

  // Initialize Kernel Command Line Form Setting
  KernelCmdLineLen = 0;
  Status           = gRT->GetVariable (L"KernelCommandLine", &gNVIDIAPublicVariableGuid, NULL, &KernelCmdLineLen, NULL);
  if (Status == EFI_NOT_FOUND) {
    KernelCmdLineLen = 0;
  } else if (Status != EFI_BUFFER_TOO_SMALL) {
    DEBUG ((DEBUG_ERROR, "%a: Error Requesting command line variable %r\r\n", __FUNCTION__, Status));
    KernelCmdLineLen = 0;
  }

  if (KernelCmdLineLen < sizeof (CmdLine)) {
    KernelCmdLineLen = sizeof (CmdLine);
    ZeroMem (&CmdLine, KernelCmdLineLen);
    Status = gRT->SetVariable (L"KernelCommandLine", &gNVIDIAPublicVariableGuid, EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS, KernelCmdLineLen, (VOID *)&CmdLine);
    if (EFI_ERROR (Status)) {
      DEBUG ((DEBUG_ERROR, "%a: Error setting command line variable %r\r\n", __FUNCTION__, Status));
    }
  }

  BufferSize = sizeof (mHiiControlSettings.RootfsRedundancyLevel);
  Status     = gRT->GetVariable (
                      L"RootfsRedundancyLevel",
                      &gNVIDIAPublicVariableGuid,
                      NULL,
                      &BufferSize,
                      &mHiiControlSettings.RootfsRedundancyLevel
                      );
  if (EFI_ERROR (Status)) {
    mHiiControlSettings.RootfsRedundancyLevel = 0;
  }

  mHiiControlSettings.L4TSupported = PcdGetBool (PcdL4TConfigurationSupport);
}

/**
  This function allows a caller to extract the current configuration for one
  or more named elements from the target driver.

  @param[in]  This           Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param[in]  Request        A null-terminated Unicode string in
                             <ConfigRequest> format.
  @param[out] Progress       On return, points to a character in the Request
                             string. Points to the string's null terminator if
                             request was successful. Points to the most recent
                             '&' before the first failing name/value pair (or
                             the beginning of the string if the failure is in
                             the first name/value pair) if the request was not
                             successful.
  @param[out] Results        A null-terminated Unicode string in
                             <ConfigAltResp> format which has all values filled
                             in for the names in the Request string. String to
                             be allocated by the called function.

  @retval EFI_SUCCESS             The Results is filled with the requested
                                  values.
  @retval EFI_OUT_OF_RESOURCES    Not enough memory to store the results.
  @retval EFI_INVALID_PARAMETER   Request is illegal syntax, or unknown name.
  @retval EFI_NOT_FOUND           Routing data doesn't match any storage in
                                  this driver.

**/
EFI_STATUS
EFIAPI
ConfigExtractConfig (
  IN CONST EFI_HII_CONFIG_ACCESS_PROTOCOL  *This,
  IN CONST EFI_STRING                      Request,
  OUT EFI_STRING                           *Progress,
  OUT EFI_STRING                           *Results
  )
{
  EFI_STATUS  Status;
  UINTN       BufferSize;
  EFI_STRING  ConfigRequestHdr;
  EFI_STRING  ConfigRequest;
  BOOLEAN     AllocatedRequest;
  UINTN       Size;

  if ((This == NULL) || (Progress == NULL) || (Results == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  *Progress = Request;
  if ((Request != NULL) && !HiiIsConfigHdrMatch (Request, &gNVIDIAResourceConfigFormsetGuid, mHiiControlStorageName)) {
    return EFI_NOT_FOUND;
  }

  ConfigRequestHdr = NULL;
  ConfigRequest    = NULL;
  AllocatedRequest = FALSE;
  Size             = 0;

  //
  // Convert buffer data to <ConfigResp> by helper function BlockToConfig().
  //
  BufferSize    = sizeof (NVIDIA_CONFIG_HII_CONTROL);
  ConfigRequest = Request;
  if ((Request == NULL) || (StrStr (Request, L"OFFSET") == NULL)) {
    //
    // Request has no request element, construct full request string.
    // Allocate and fill a buffer large enough to hold the <ConfigHdr> template
    // followed by "&OFFSET=0&WIDTH=WWWWWWWWWWWWWWWW" followed by a Null-terminator
    //
    ConfigRequestHdr = HiiConstructConfigHdr (&gNVIDIAResourceConfigFormsetGuid, mHiiControlStorageName, mDriverHandle);
    if (ConfigRequestHdr == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    Size          = (StrLen (ConfigRequestHdr) + 32 + 1) * sizeof (CHAR16);
    ConfigRequest = AllocateZeroPool (Size);
    if (ConfigRequest == NULL) {
      return EFI_OUT_OF_RESOURCES;
    }

    AllocatedRequest = TRUE;
    UnicodeSPrint (ConfigRequest, Size, L"%s&OFFSET=0&WIDTH=%016LX", ConfigRequestHdr, (UINT64)BufferSize);
    FreePool (ConfigRequestHdr);
  }

  SyncHiiSettings (TRUE);

  Status = gHiiConfigRouting->BlockToConfig (
                                gHiiConfigRouting,
                                ConfigRequest,
                                (UINT8 *)&mHiiControlSettings,
                                BufferSize,
                                Results,
                                Progress
                                );
  //
  // Free the allocated config request string.
  //
  if (AllocatedRequest) {
    FreePool (ConfigRequest);
    ConfigRequest = NULL;
  }

  //
  // Set Progress string to the original request string.
  //
  if (Request == NULL) {
    *Progress = NULL;
  } else if (StrStr (Request, L"OFFSET") == NULL) {
    *Progress = Request + StrLen (Request);
  }

  return Status;
}

/**
  This function processes the results of changes in configuration.

  @param[in]  This           Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param[in]  Configuration  A null-terminated Unicode string in <ConfigResp>
                             format.
  @param[out] Progress       A pointer to a string filled in with the offset of
                             the most recent '&' before the first failing
                             name/value pair (or the beginning of the string if
                             the failure is in the first name/value pair) or
                             the terminating NULL if all was successful.

  @retval EFI_SUCCESS             The Results is processed successfully.
  @retval EFI_INVALID_PARAMETER   Configuration is NULL.
  @retval EFI_NOT_FOUND           Routing data doesn't match any storage in
                                  this driver.

**/
EFI_STATUS
EFIAPI
ConfigRouteConfig (
  IN CONST EFI_HII_CONFIG_ACCESS_PROTOCOL  *This,
  IN CONST EFI_STRING                      Configuration,
  OUT EFI_STRING                           *Progress
  )
{
  EFI_STATUS  Status;
  UINTN       BufferSize;

  Status = EFI_SUCCESS;

  if ((This == NULL) || (Configuration == NULL) || (Progress == NULL)) {
    return EFI_INVALID_PARAMETER;
  }

  //
  // Check routing data in <ConfigHdr>.
  // Note: if only one Storage is used, then this checking could be skipped.
  //
  if (!HiiIsConfigHdrMatch (Configuration, &gNVIDIAResourceConfigFormsetGuid, mHiiControlStorageName)) {
    *Progress = Configuration;
    return EFI_NOT_FOUND;
  }

  //
  // Convert <ConfigResp> to buffer data by helper function ConfigToBlock().
  //
  BufferSize = sizeof (NVIDIA_CONFIG_HII_CONTROL);
  Status     = gHiiConfigRouting->ConfigToBlock (
                                    gHiiConfigRouting,
                                    Configuration,
                                    (UINT8 *)&mHiiControlSettings,
                                    &BufferSize,
                                    Progress
                                    );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  SyncHiiSettings (FALSE);
  // Send settings to MB1 record.

  return Status;
}

/**
  This function processes the results of changes in configuration.

  @param[in]  This           Points to the EFI_HII_CONFIG_ACCESS_PROTOCOL.
  @param[in]  Action         Specifies the type of action taken by the browser.
  @param[in]  QuestionId     A unique value which is sent to the original
                             exporting driver so that it can identify the type
                             of data to expect.
  @param[in]  Type           The type of value for the question.
  @param[in]  Value          A pointer to the data being sent to the original
                             exporting driver.
  @param[out] ActionRequest  On return, points to the action requested by the
                             callback function.

  @retval EFI_SUCCESS             The callback successfully handled the action.
  @retval EFI_OUT_OF_RESOURCES    Not enough storage is available to hold the
                                  variable and its data.
  @retval EFI_DEVICE_ERROR        The variable could not be saved.
  @retval EFI_UNSUPPORTED         The specified Action is not supported by the
                                  callback.

**/
EFI_STATUS
EFIAPI
ConfigCallback (
  IN CONST EFI_HII_CONFIG_ACCESS_PROTOCOL  *This,
  IN     EFI_BROWSER_ACTION                Action,
  IN     EFI_QUESTION_ID                   QuestionId,
  IN     UINT8                             Type,
  IN     EFI_IFR_TYPE_VALUE                *Value,
  OUT EFI_BROWSER_ACTION_REQUEST           *ActionRequest
  )
{
  EFI_STATUS  Status;
  EFI_STATUS  VarDeleteStatus;
  CHAR16      *CurrentName;
  CHAR16      *NextName;
  EFI_GUID    CurrentGuid;
  EFI_GUID    NextGuid;
  UINTN       NameSize;

  Status = EFI_UNSUPPORTED;
  if ((Action == EFI_BROWSER_ACTION_FORM_OPEN) ||
      (Action == EFI_BROWSER_ACTION_FORM_CLOSE))
  {
    //
    // Do nothing for UEFI OPEN/CLOSE Action
    //
    Status = EFI_SUCCESS;
  } else if (Action == EFI_BROWSER_ACTION_CHANGED) {
    switch (QuestionId) {
      case KEY_RESET_VARIABLES:
        CurrentName = AllocateZeroPool (MAX_VARIABLE_NAME);
        if (CurrentName == NULL) {
          Status = EFI_OUT_OF_RESOURCES;
          break;
        }

        NextName = AllocateZeroPool (MAX_VARIABLE_NAME);
        if (NextName == NULL) {
          Status = EFI_OUT_OF_RESOURCES;
          FreePool (CurrentName);
          break;
        }

        NameSize = MAX_VARIABLE_NAME;
        Status   = gRT->GetNextVariableName (&NameSize, NextName, &NextGuid);

        while (!EFI_ERROR (Status)) {
          CopyMem (CurrentName, NextName, NameSize);
          CopyGuid (&CurrentGuid, &NextGuid);

          NameSize = MAX_VARIABLE_NAME;
          Status   = gRT->GetNextVariableName (&NameSize, NextName, &NextGuid);

          // Delete Current Name variable
          VarDeleteStatus = gRT->SetVariable (
                                   CurrentName,
                                   &CurrentGuid,
                                   0,
                                   0,
                                   NULL
                                   );
          DEBUG ((DEBUG_ERROR, "Delete Variable %g:%s %r\r\n", &CurrentGuid, CurrentName, VarDeleteStatus));
        }

        FreePool (NextName);
        FreePool (CurrentName);
        Status = EFI_SUCCESS;

        break;

      default:
        break;
    }
  }

  return Status;
}

VOID
EFIAPI
OnEndOfDxe (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  EFI_STATUS      Status;
  EFI_HII_HANDLE  HiiHandle;

  gBS->CloseEvent (Event);

  InitializeSettings ();

  mConfigAccess.Callback      = ConfigCallback;
  mConfigAccess.ExtractConfig = ConfigExtractConfig;
  mConfigAccess.RouteConfig   = ConfigRouteConfig;

  mDriverHandle = NULL;
  Status        = gBS->InstallMultipleProtocolInterfaces (
                         &mDriverHandle,
                         &gEfiDevicePathProtocolGuid,
                         &mNvidiaConfigHiiVendorDevicePath,
                         &gEfiHiiConfigAccessProtocolGuid,
                         &mConfigAccess,
                         NULL
                         );
  if (!EFI_ERROR (Status)) {
    HiiHandle = HiiAddPackages (
                  &gNVIDIAResourceConfigFormsetGuid,
                  mDriverHandle,
                  NvidiaConfigDxeStrings,
                  NvidiaConfigHiiBin,
                  NULL
                  );

    if (HiiHandle == NULL) {
      gBS->UninstallMultipleProtocolInterfaces (
             mDriverHandle,
             &gEfiDevicePathProtocolGuid,
             &mNvidiaConfigHiiVendorDevicePath,
             &gEfiHiiConfigAccessProtocolGuid,
             &mConfigAccess,
             NULL
             );
    }
  }
}

/**
  Update Serial Port PCDs.
**/
STATIC
EFI_STATUS
UpdateSerialPcds (
  VOID
  )
{
  UINT32      NumSbsaUartControllers;
  EFI_STATUS  Status;
  UINT8       DefaultPortConfig;
  UINTN       SerialPortVarLen;

  NumSbsaUartControllers = 0;

  // Obtain SBSA Handle Info
  Status = GetMatchingEnabledDeviceTreeNodes ("arm,sbsa-uart", NULL, &NumSbsaUartControllers);
  if (Status == EFI_NOT_FOUND) {
    PcdSet8S (PcdSerialTypeConfig, NVIDIA_SERIAL_PORT_TYPE_16550);
    DefaultPortConfig = NVIDIA_SERIAL_PORT_SPCR_FULL_16550;
  } else {
    PcdSet8S (PcdSerialTypeConfig, NVIDIA_SERIAL_PORT_TYPE_SBSA);
    DefaultPortConfig = NVIDIA_SERIAL_PORT_SPCR_SBSA;
  }

  SerialPortVarLen = 0;
  Status           = gRT->GetVariable (L"SerialPortConfig", &gNVIDIATokenSpaceGuid, NULL, &SerialPortVarLen, NULL);
  if (Status == EFI_NOT_FOUND) {
    PcdSet8S (PcdSerialPortConfig, DefaultPortConfig);
  }

  return EFI_SUCCESS;
}

/**
  Install NVIDIA Config driver.

  @param  ImageHandle     The image handle.
  @param  SystemTable     The system table.

  @retval EFI_SUCEESS     Install Boot manager menu success.
  @retval Other           Return error status.

**/
EFI_STATUS
EFIAPI
NvidiaConfigDxeInitialize (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  EFI_EVENT   EndOfDxeEvent;

  UpdateSerialPcds ();

  Status = gBS->CreateEventEx (
                  EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  OnEndOfDxe,
                  NULL,
                  &gEfiEndOfDxeEventGroupGuid,
                  &EndOfDxeEvent
                  );

  return Status;
}
