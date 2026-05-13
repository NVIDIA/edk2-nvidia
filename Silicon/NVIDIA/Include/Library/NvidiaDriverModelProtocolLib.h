/** @file

  NVIDIA Driver Model Protocol Library

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef NVIDIA_DRIVER_MODEL_PROTOCOL_LIB_H_
#define NVIDIA_DRIVER_MODEL_PROTOCOL_LIB_H_

#include <Protocol/DriverBinding.h>
#include <Protocol/DriverHealth.h>

/**
  Installs Driver Binding, Component Name, Component Name 2, and Driver Health
  protocols on a driver handle.

  @param[in] ImageHandle             Driver image handle.
  @param[in] SystemTable             EFI system table.
  @param[in] DriverBinding           Driver Binding protocol instance.
  @param[in] DriverBindingHandle     Handle for the driver model protocols.
  @param[in] DriverName              User-readable driver name.
  @param[in] ControllerName          Optional user-readable controller name.
  @param[in] ControllerProtocolGuid  Optional controller protocol used to
                                     verify managed controllers.

  @retval EFI_SUCCESS            Protocols were installed.
  @retval EFI_INVALID_PARAMETER  A required parameter is NULL.
  @retval others                 Protocol installation failure.
**/
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
  );

/**
  Installs Component Name, Component Name 2, and Driver Health protocols on a
  non-Driver-Binding driver handle.

  @param[in] DriverHandle            Driver handle.
  @param[in] DriverName              User-readable driver name.
  @param[in] ControllerName          Optional user-readable controller name.
  @param[in] ControllerProtocolGuid  Optional controller protocol used to
                                     verify managed controllers.

  @retval EFI_SUCCESS            Protocols were installed.
  @retval EFI_INVALID_PARAMETER  A required parameter is NULL.
  @retval others                 Protocol installation failure.
**/
EFI_STATUS
EFIAPI
NvidiaInstallComponentName2AndDriverHealth (
  IN EFI_HANDLE    DriverHandle,
  IN CONST CHAR16  *DriverName,
  IN CONST CHAR16  *ControllerName OPTIONAL,
  IN EFI_GUID      *ControllerProtocolGuid OPTIONAL
  );

/**
  Installs Driver Health protocol on a driver handle.

  @param[in] DriverHandle            Driver handle.
  @param[in] DriverBinding           Optional Driver Binding protocol instance.
  @param[in] ControllerProtocolGuid  Optional controller protocol used to
                                     verify managed controllers.

  @retval EFI_SUCCESS            Protocol was installed.
  @retval EFI_INVALID_PARAMETER  DriverHandle is NULL.
  @retval others                 Protocol installation failure.
**/
EFI_STATUS
EFIAPI
NvidiaInstallDriverHealthProtocol (
  IN EFI_HANDLE                   DriverHandle,
  IN EFI_DRIVER_BINDING_PROTOCOL  *DriverBinding OPTIONAL,
  IN EFI_GUID                     *ControllerProtocolGuid OPTIONAL
  );

/**
  Uninstalls Driver Health protocol from a driver handle.

  @param[in] DriverHandle  Driver handle.

  @retval EFI_SUCCESS            Protocol was uninstalled.
  @retval EFI_INVALID_PARAMETER  DriverHandle is NULL.
  @retval others                 Protocol uninstallation failure.
**/
EFI_STATUS
EFIAPI
NvidiaUninstallDriverHealthProtocol (
  IN EFI_HANDLE  DriverHandle
  );

/**
  Sets the health status returned by the Driver Health protocol.

  @param[in] HealthStatus  Driver health status to report.

  @retval EFI_SUCCESS            Health status was updated.
  @retval EFI_INVALID_PARAMETER  HealthStatus is not a valid enum value.
**/
EFI_STATUS
EFIAPI
NvidiaSetDriverHealthStatus (
  IN EFI_DRIVER_HEALTH_STATUS  HealthStatus
  );

/**
  Maps an EFI status value to Driver Health status.

  @param[in] Status  EFI status to map.

  @retval EFI_SUCCESS  Health status was updated.
**/
EFI_STATUS
EFIAPI
NvidiaSetDriverHealthStatusFromEfiStatus (
  IN EFI_STATUS  Status
  );

/**
  Retrieves the current health status returned by the Driver Health protocol.

  @param[out] HealthStatus  Current driver health status.

  @retval EFI_SUCCESS            Health status was returned.
  @retval EFI_INVALID_PARAMETER  HealthStatus is NULL.
**/
EFI_STATUS
EFIAPI
NvidiaGetDriverHealthStatus (
  OUT EFI_DRIVER_HEALTH_STATUS  *HealthStatus
  );

#endif
