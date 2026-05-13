/** @file

  NVIDIA Driver Model Protocol Library

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#include <PiDxe.h>

#include <Library/BaseLib.h>
#include <Library/NvidiaDriverModelProtocolLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Library/UefiLib.h>

#include <Protocol/ComponentName.h>
#include <Protocol/ComponentName2.h>
#include <Protocol/DriverHealth.h>

STATIC EFI_DRIVER_BINDING_PROTOCOL  *mDriverBinding;
STATIC EFI_GUID                     *mControllerProtocolGuid;
STATIC EFI_DRIVER_HEALTH_STATUS     mDriverHealthStatus = EfiDriverHealthStatusHealthy;

STATIC EFI_UNICODE_STRING_TABLE  mDriverNameTable[] = {
  { "eng;en", NULL },
  { NULL,     NULL }
};

STATIC EFI_UNICODE_STRING_TABLE  mControllerNameTable[] = {
  { "eng;en", NULL },
  { NULL,     NULL }
};

STATIC
EFI_STATUS
EFIAPI
NvidiaComponentNameGetDriverName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **DriverName
  );

STATIC
EFI_STATUS
EFIAPI
NvidiaComponentNameGetControllerName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  EFI_HANDLE                   ControllerHandle,
  IN  EFI_HANDLE                   ChildHandle OPTIONAL,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **ControllerName
  );

STATIC EFI_COMPONENT_NAME_PROTOCOL  mComponentName = {
  NvidiaComponentNameGetDriverName,
  NvidiaComponentNameGetControllerName,
  "eng"
};

STATIC EFI_COMPONENT_NAME2_PROTOCOL  mComponentName2 = {
  (EFI_COMPONENT_NAME2_GET_DRIVER_NAME)NvidiaComponentNameGetDriverName,
  (EFI_COMPONENT_NAME2_GET_CONTROLLER_NAME)NvidiaComponentNameGetControllerName,
  "en"
};

STATIC
EFI_STATUS
EFIAPI
NvidiaDriverHealthGetHealthStatus (
  IN  EFI_DRIVER_HEALTH_PROTOCOL     *This,
  IN  EFI_HANDLE                     ControllerHandle OPTIONAL,
  IN  EFI_HANDLE                     ChildHandle OPTIONAL,
  OUT EFI_DRIVER_HEALTH_STATUS       *HealthStatus,
  OUT EFI_DRIVER_HEALTH_HII_MESSAGE  **MessageList OPTIONAL,
  OUT EFI_HII_HANDLE                 *FormHiiHandle OPTIONAL
  );

STATIC
EFI_STATUS
EFIAPI
NvidiaDriverHealthRepair (
  IN EFI_DRIVER_HEALTH_PROTOCOL       *This,
  IN EFI_HANDLE                       ControllerHandle,
  IN EFI_HANDLE                       ChildHandle OPTIONAL,
  IN EFI_DRIVER_HEALTH_REPAIR_NOTIFY  RepairNotify OPTIONAL
  );

STATIC EFI_DRIVER_HEALTH_PROTOCOL  mDriverHealth = {
  NvidiaDriverHealthGetHealthStatus,
  NvidiaDriverHealthRepair
};

STATIC
BOOLEAN
NvidiaIsDriverHealthStatusValid (
  IN EFI_DRIVER_HEALTH_STATUS  HealthStatus
  )
{
  return (BOOLEAN)(HealthStatus <= EfiDriverHealthStatusRebootRequired);
}

STATIC
EFI_STATUS
NvidiaInitializeDriverModelContext (
  IN CONST CHAR16                 *DriverName,
  IN CONST CHAR16                 *ControllerName OPTIONAL,
  IN EFI_DRIVER_BINDING_PROTOCOL  *DriverBinding OPTIONAL,
  IN EFI_GUID                     *ControllerProtocolGuid OPTIONAL
  )
{
  if (DriverName == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  mDriverNameTable[0].UnicodeString     = (CHAR16 *)DriverName;
  mControllerNameTable[0].UnicodeString = (CHAR16 *)ControllerName;
  mDriverBinding                        = DriverBinding;
  mControllerProtocolGuid               = ControllerProtocolGuid;
  mDriverHealthStatus                   = EfiDriverHealthStatusHealthy;

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
NvidiaVerifyManagedController (
  IN EFI_HANDLE  ControllerHandle,
  IN EFI_HANDLE  ChildHandle OPTIONAL
  )
{
  EFI_STATUS  Status;
  VOID        *Protocol;

  if ((ControllerHandle == NULL) ||
      (ChildHandle != NULL) ||
      (mDriverBinding == NULL) ||
      (mDriverBinding->DriverBindingHandle == NULL) ||
      (mControllerProtocolGuid == NULL))
  {
    return EFI_UNSUPPORTED;
  }

  Status = gBS->OpenProtocol (
                  ControllerHandle,
                  mControllerProtocolGuid,
                  &Protocol,
                  mDriverBinding->DriverBindingHandle,
                  ControllerHandle,
                  EFI_OPEN_PROTOCOL_BY_DRIVER
                  );
  if (!EFI_ERROR (Status)) {
    gBS->CloseProtocol (
           ControllerHandle,
           mControllerProtocolGuid,
           mDriverBinding->DriverBindingHandle,
           ControllerHandle
           );
    return EFI_UNSUPPORTED;
  }

  if (Status != EFI_ALREADY_STARTED) {
    return EFI_UNSUPPORTED;
  }

  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
NvidiaComponentNameGetDriverName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **DriverName
  )
{
  return LookupUnicodeString2 (
           Language,
           This->SupportedLanguages,
           mDriverNameTable,
           DriverName,
           (BOOLEAN)(This == &mComponentName)
           );
}

STATIC
EFI_STATUS
EFIAPI
NvidiaComponentNameGetControllerName (
  IN  EFI_COMPONENT_NAME_PROTOCOL  *This,
  IN  EFI_HANDLE                   ControllerHandle,
  IN  EFI_HANDLE                   ChildHandle OPTIONAL,
  IN  CHAR8                        *Language,
  OUT CHAR16                       **ControllerName
  )
{
  EFI_STATUS  Status;

  if (mControllerNameTable[0].UnicodeString == NULL) {
    return EFI_UNSUPPORTED;
  }

  Status = NvidiaVerifyManagedController (ControllerHandle, ChildHandle);
  if (EFI_ERROR (Status)) {
    return Status;
  }

  return LookupUnicodeString2 (
           Language,
           This->SupportedLanguages,
           mControllerNameTable,
           ControllerName,
           (BOOLEAN)(This == &mComponentName)
           );
}

STATIC
EFI_STATUS
EFIAPI
NvidiaDriverHealthGetHealthStatus (
  IN  EFI_DRIVER_HEALTH_PROTOCOL     *This,
  IN  EFI_HANDLE                     ControllerHandle OPTIONAL,
  IN  EFI_HANDLE                     ChildHandle OPTIONAL,
  OUT EFI_DRIVER_HEALTH_STATUS       *HealthStatus,
  OUT EFI_DRIVER_HEALTH_HII_MESSAGE  **MessageList OPTIONAL,
  OUT EFI_HII_HANDLE                 *FormHiiHandle OPTIONAL
  )
{
  EFI_STATUS  Status;

  if (HealthStatus == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  if (MessageList != NULL) {
    *MessageList = NULL;
  }

  if (FormHiiHandle != NULL) {
    *FormHiiHandle = NULL;
  }

  if (ControllerHandle != NULL) {
    Status = NvidiaVerifyManagedController (ControllerHandle, ChildHandle);
    if (EFI_ERROR (Status)) {
      return Status;
    }
  }

  *HealthStatus = mDriverHealthStatus;
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
NvidiaDriverHealthRepair (
  IN EFI_DRIVER_HEALTH_PROTOCOL       *This,
  IN EFI_HANDLE                       ControllerHandle,
  IN EFI_HANDLE                       ChildHandle OPTIONAL,
  IN EFI_DRIVER_HEALTH_REPAIR_NOTIFY  RepairNotify OPTIONAL
  )
{
  return EFI_UNSUPPORTED;
}

EFI_STATUS
EFIAPI
NvidiaInstallDriverHealthProtocol (
  IN EFI_HANDLE                   DriverHandle,
  IN EFI_DRIVER_BINDING_PROTOCOL  *DriverBinding OPTIONAL,
  IN EFI_GUID                     *ControllerProtocolGuid OPTIONAL
  )
{
  EFI_HANDLE  InstallHandle;

  if (DriverHandle == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  mDriverBinding          = DriverBinding;
  mControllerProtocolGuid = ControllerProtocolGuid;
  mDriverHealthStatus     = EfiDriverHealthStatusHealthy;
  InstallHandle           = DriverHandle;

  return gBS->InstallMultipleProtocolInterfaces (
                &InstallHandle,
                &gEfiDriverHealthProtocolGuid,
                &mDriverHealth,
                NULL
                );
}

EFI_STATUS
EFIAPI
NvidiaSetDriverHealthStatus (
  IN EFI_DRIVER_HEALTH_STATUS  HealthStatus
  )
{
  if (!NvidiaIsDriverHealthStatusValid (HealthStatus)) {
    return EFI_INVALID_PARAMETER;
  }

  mDriverHealthStatus = HealthStatus;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
NvidiaSetDriverHealthStatusFromEfiStatus (
  IN EFI_STATUS  Status
  )
{
  if (EFI_ERROR (Status)) {
    mDriverHealthStatus = EfiDriverHealthStatusFailed;
  } else {
    mDriverHealthStatus = EfiDriverHealthStatusHealthy;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
NvidiaGetDriverHealthStatus (
  OUT EFI_DRIVER_HEALTH_STATUS  *HealthStatus
  )
{
  if (HealthStatus == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  *HealthStatus = mDriverHealthStatus;
  return EFI_SUCCESS;
}

EFI_STATUS
EFIAPI
NvidiaInstallComponentName2AndDriverHealth (
  IN EFI_HANDLE    DriverHandle,
  IN CONST CHAR16  *DriverName,
  IN CONST CHAR16  *ControllerName OPTIONAL,
  IN EFI_GUID      *ControllerProtocolGuid OPTIONAL
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  InstallHandle;

  if (DriverHandle == NULL) {
    return EFI_INVALID_PARAMETER;
  }

  Status = NvidiaInitializeDriverModelContext (
             DriverName,
             ControllerName,
             NULL,
             ControllerProtocolGuid
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  InstallHandle = DriverHandle;
  return gBS->InstallMultipleProtocolInterfaces (
                &InstallHandle,
                &gEfiComponentNameProtocolGuid,
                &mComponentName,
                &gEfiComponentName2ProtocolGuid,
                &mComponentName2,
                &gEfiDriverHealthProtocolGuid,
                &mDriverHealth,
                NULL
                );
}

EFI_STATUS
EFIAPI
NvidiaInstallDriverModelProtocols (
  IN EFI_HANDLE                   ImageHandle,
  IN EFI_SYSTEM_TABLE             *SystemTable,
  IN EFI_DRIVER_BINDING_PROTOCOL  *DriverBinding,
  IN EFI_HANDLE                   DriverBindingHandle,
  IN CONST CHAR16                 *DriverName,
  IN CONST CHAR16                 *ControllerName OPTIONAL,
  IN EFI_GUID                     *ControllerProtocolGuid OPTIONAL
  )
{
  EFI_STATUS  Status;
  EFI_HANDLE  DriverHealthHandle;

  if ((ImageHandle == NULL) ||
      (SystemTable == NULL) ||
      (DriverBinding == NULL) ||
      (DriverBindingHandle == NULL))
  {
    return EFI_INVALID_PARAMETER;
  }

  Status = NvidiaInitializeDriverModelContext (
             DriverName,
             ControllerName,
             DriverBinding,
             ControllerProtocolGuid
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  Status = EfiLibInstallDriverBindingComponentName2 (
             ImageHandle,
             SystemTable,
             DriverBinding,
             DriverBindingHandle,
             &mComponentName,
             &mComponentName2
             );
  if (EFI_ERROR (Status)) {
    return Status;
  }

  DriverHealthHandle = DriverBinding->DriverBindingHandle;
  if (DriverHealthHandle == NULL) {
    DriverHealthHandle = DriverBindingHandle;
  }

  return NvidiaInstallDriverHealthProtocol (
           DriverHealthHandle,
           DriverBinding,
           ControllerProtocolGuid
           );
}
