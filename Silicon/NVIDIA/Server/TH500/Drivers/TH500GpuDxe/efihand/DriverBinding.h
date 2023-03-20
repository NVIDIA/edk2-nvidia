/** @file
  NVIDIA GPU Driver Binding Private Data structures and declarations

  Copyright (c) 2020-2023, NVIDIA CORPORATION. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _GPU_DRIVER_BINDING_H_
#define _GPU_DRIVER_BINDING_H_

///
/// Libraries
///
#include <Library/UefiLib.h>
#include <Library/BaseLib.h>
#include <Library/DevicePathLib.h>
#include <Library/DebugLib.h>

/* Protocols */

///
/// DriverBinding private data declaration
///

#define NVIDIA_GPU_DRIVER_BINDING_PRIVATE_DATA_SIGNATURE  SIGNATURE_32 ('N', 'g', 'p', 'u')

#pragma pack(1)

typedef struct {
  /// NVIDIA GPU Driver Binding Private Data signature
  UINT32                         Signature;

  /// NVIDIA Gpu Driver Binding Handle
  EFI_HANDLE                     Handle;

  /// Driver Binding Protocol
  EFI_DRIVER_BINDING_PROTOCOL    DriverBinding;

  /// EFI System Table pointer
  EFI_SYSTEM_TABLE               *SystemTable;

  /// Boot Services Table pointer
  EFI_BOOT_SERVICES              *BootServices;

  /// Array of handles that the Gpu Driver Binding can manage (supported)
  EFI_HANDLE                     *ManagedControllerHandles;

  /// Count of handle in the ManagedControllerHandles array
  UINTN                          nManagedControllers;

  /// Original Pci Attributes
  UINT64                         PciAttributes;
} NVIDIA_GPU_DRIVER_BINDING_PRIVATE_DATA;

#pragma pack()

typedef EFI_DRIVER_BINDING_PROTOCOL NVIDIA_GPU_DRIVER_BINDING_PROTOCOL;

#define NVIDIA_GPU_DRIVER_BINDING_PRIVATE_DATA_FROM_THIS(a) \
    CR (a, NVIDIA_GPU_DRIVER_BINDING_PRIVATE_DATA, DriverBinding, NVIDIA_GPU_DRIVER_BINDING_PRIVATE_DATA_SIGNATURE );

///
/// Driver Binding externs
///
extern EFI_DRIVER_BINDING_PROTOCOL  *gNVIDIAGpuDeviceLibDriverBinding;
// extern EFI_COMPONENT_NAME_PROTOCOL   gNVIDIAGpuDriverComponentName;
extern EFI_COMPONENT_NAME2_PROTOCOL  gNVIDIAGpuDriverComponentName2Protocol;

/** Driver Binding protocol interface to check whether the controller handle is supported by the driver
        Check the PCI VendorId and DeviceId for supported controllers.
    @param  EFI_DRIVER_BINDING_PROTOCOL*        Pointer to the Driver Binding protocol
    @param  EFI_HANDLE                          Device Handle of the controller to assess
    @param  EFI_DEVICE_PATH_PROTOCOL*           Remaining Device Path (ignored)
    @retval Support status of the driver on the controller handle.
                        EFI_SUCCESS             - The driver is supported on the controller handle.
                        EFI_INVALID_PARAMETER   - ControllerHandle is NULL
                        EFI_UNSUPPORTED         - Driver does not support the controller.
**/
EFI_STATUS
EFIAPI
NVIDIAGpuDriverSupported (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath
  );

/** Driver Binding protocol interface to start the driver on the controller handle supplied
    @param  EFI_DRIVER_BINDING_PROTOCOL*        Pointer to the Driver Binding protocol
    @param  EFI_HANDLE                          Device Handle of the controller to start the driver on
    @param  EFI_DEVICE_PATH_PROTOCOL*           Remaining Device Path (ignored)
    @retval Status of starting the NVIDIAGpuDriverStart
                        EFI_SUCCESS             - The Driver was successfully started on the contoller handle
                        EFI_NOT_READY           - Private Data is not initialized.
                        EFI_INVALID_PARAMETER   - ControllerHandle is NULL
                        EFI_UNSUPPORTED         - Driver does not support the controller.
**/
EFI_STATUS
EFIAPI
NVIDIAGpuDriverStart (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN EFI_DEVICE_PATH_PROTOCOL     *RemainingDevicePath OPTIONAL
  );

/** Driver Binding protocol interface to stop the driver on the controller handle supplied
    @param  EFI_DRIVER_BINDING_PROTOCOL*        Pointer to the Driver Binding protocol
    @param  EFI_HANDLE                          Device Handle of the controller to assess
    @param  UINTN                               Number of children (ignored - no support for children)
    @param  EFI_HANDLE                          Child Handle Buffer (ignored)
    @retval Status of stopping the driver
                        EFI_SUCCESS             - The driver was successfully stopped on the controller handle
                        EFI_NOT_READY           - Private Data is not initialized.
                        EFI_INVALID_PARAMETER   - ControllerHandle is NULL
                        EFI_UNSUPPORTED         - Driver does not support the controller.
**/
EFI_STATUS
EFIAPI
NVIDIAGpuDriverStop (
  IN EFI_DRIVER_BINDING_PROTOCOL  *This,
  IN EFI_HANDLE                   ControllerHandle,
  IN UINTN                        NumberOfChildren,
  IN EFI_HANDLE                   *ChildHandleBuffer OPTIONAL
  );

/** Install the driver binding on the ImageHandle
    @param[in] ImageHandle  ImageHandle to install the DriverBinding on
    @param{in} SystemTable  Pointer to the EFI System Table structure
    @retval EFI_STATUS  EFI_SUCCESS
    @retval EFI_NOT_READY ImageHandle is NULL
    @retval EFI_NOT_READY SystemTable is NULL
      (pass through error from EfiLibInstallDriverBindingComponentName2)
**/
EFI_STATUS
EFIAPI
NVIDIAGpuDriverEntryPoint (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  );

/** Function that unloads the NVIDIA GPU Driver binding.

    @param[in]  ImageHandle  The driver handle managing the Firmware Management Protocol instance to unload.

    @retval EFI_SUCCESS               Driver image was removed successfully.
    @retval EFI_INVALID_PARAMETER     ImageHandle is NULL.
    @retval EFI_INVALID_PARAMETER     ImageHandle does not match driver image handle.
    @retval EFI_UNSUPPORTED           Private Data ImageHandle is NULL
    @retval EFI_UNSUPPORTED           Private Data DriverBindingHandle is NULL
    @retval EFI_UNSUPPORTED           The device is not managed by a driver that follows
                                        the UEFI Driver Model.
    @retval other                     Status from EfiLibUninstallDriverBindingComponentName2

**/
EFI_STATUS
EFIAPI
NVIDIAGpuDriverUnloadImage (
  IN EFI_HANDLE  ImageHandle
  );

#endif // _GPU_DRIVER_BINDING_H_
