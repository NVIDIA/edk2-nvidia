/** @file
  Header file for Redfish HTTP Boot Configuration DXE driver.

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef REDFISH_HTTP_BOOT_CONFIG_DXE_H_
#define REDFISH_HTTP_BOOT_CONFIG_DXE_H_

#include <Guid/MdeModuleHii.h>
#include <Protocol/HiiConfigAccess.h>
#include <Protocol/HiiConfigRouting.h>
#include <Library/UefiDriverEntryPoint.h>
#include <Library/HiiLib.h>
#include <Library/UefiHiiServicesLib.h>
#include <Library/NetLib.h>

#include "RedfishHttpBootConfigVfrDefs.h"
#include "RedfishHttpBootConfigFormset.h"
#include "RedfishHttpBootConfigUtils.h"

extern UINT8     RedfishHttpBootConfigVfrBin[];
extern UINT8     RedfishHttpBootConfigDxeStrings[];
extern EFI_GUID  gNvidiaHttpBootConfigGuid;

//
// Private data structure
//
#define REDFISH_HTTP_BOOT_CONFIG_SIGNATURE  SIGNATURE_32('R','H','B','C')

typedef struct {
  UINT32                            Signature;
  EFI_HII_CONFIG_ACCESS_PROTOCOL    ConfigAccess;
} REDFISH_HTTP_BOOT_CONFIG_PRIVATE_DATA;

//
// HII vendor device path
//
typedef struct {
  VENDOR_DEVICE_PATH          VendorDevicePath;
  EFI_DEVICE_PATH_PROTOCOL    End;
} HII_VENDOR_DEVICE_PATH;

//
// Function prototypes
//

/**
  ExtractConfig callback for HII Config Access Protocol.

  @param[in]  This           Config Access Protocol instance
  @param[in]  Request        Request string
  @param[out] Progress       Progress pointer
  @param[out] Results        Results string

  @retval EFI_SUCCESS        Configuration extracted successfully
  @retval Others             Error occurred
**/
EFI_STATUS
EFIAPI
HttpBootConfigExtractConfig (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL  *This,
  IN  CONST EFI_STRING                      Request,
  OUT EFI_STRING                            *Progress,
  OUT EFI_STRING                            *Results
  );

/**
  RouteConfig callback for HII Config Access Protocol.

  @param[in]  This           Config Access Protocol instance
  @param[in]  Configuration  Configuration string
  @param[out] Progress       Progress pointer

  @retval EFI_SUCCESS        Configuration routed successfully
  @retval Others             Error occurred
**/
EFI_STATUS
EFIAPI
HttpBootConfigRouteConfig (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL  *This,
  IN  CONST EFI_STRING                      Configuration,
  OUT EFI_STRING                            *Progress
  );

/**
  Callback handler for HII Config Access Protocol.

  @param[in]     This           Config Access Protocol instance
  @param[in]     Action         Action type
  @param[in]     QuestionId     Question ID
  @param[in]     Type           Value type
  @param[in,out] Value          Value
  @param[out]    ActionRequest  Action request

  @retval EFI_SUCCESS           Callback handled successfully
  @retval Others                Error occurred
**/
EFI_STATUS
EFIAPI
HttpBootConfigCallback (
  IN  CONST EFI_HII_CONFIG_ACCESS_PROTOCOL  *This,
  IN  EFI_BROWSER_ACTION                    Action,
  IN  EFI_QUESTION_ID                       QuestionId,
  IN  UINT8                                 Type,
  IN  OUT EFI_IFR_TYPE_VALUE                *Value,
  OUT EFI_BROWSER_ACTION_REQUEST            *ActionRequest
  );

#endif // REDFISH_HTTP_BOOT_CONFIG_DXE_H_
