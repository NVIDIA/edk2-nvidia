# Capsule Update documentation for NVIDIA platforms using blob FW updates
Feature Name: Capsule Update

PI Phase(s) Supported: DXE

SMM Required? No

## Purpose
The Capsule Update feature provides system firmware update capabilities in accordance with the UEFI specification.  Delivery of capsules via file on mass storage device is supported.  This requires the capsule to be present in the EFI System Partition (ESP) in the \EFI\UpdateCapsule directory and the EFI_OS_INDICATIONS_FILE_CAPSULE_DELIVERY_SUPPORTED bit to be set in the OsIndications UEFI variable when the system is booted.  Capsules may also be processed using the CapsuleApp.efi UEFI application.

## High-Level Theory of Operation
Capsule updates use the Firmware Management Protocol (FMP) to access firmware.  The edk2/FmpDevicePkg/FmpDxe module uses a vendor-implemented FmpDeviceLib Library to produce the FMP.  The FmpDeviceLib uses platform-specific drivers to access the system firmware.  The FMP reports system firmware and capsule update information in the EFI System Resource Table (ESRT) as described in the UEFI specification.


## Firmware Volumes
UEFI_NS
UEFI_MM

## Modules
### edk2/FmpDevicePkg/FmpDxe
Description:
DXE module that produces the Firmware Management Protocol.
### edk2-nvidia/Silicon/NVIDIA/Library/FmpBlobLib
Description:
NVIDIA's implementation of the FmpDeviceLib Library that is used by the edk2/FmpDevicePkg/FmpDxe DXE module on platforms using blob FW updates.
### edk2-nvidia/Silicon/NVIDIA/Library/FmpParamLib
Description:
Library to access FMP parameters from PCDs and/or DTB.

## Parameters and Options
### FMP Lowest Supported Version
The FMP Lowest Supported Version is a 32-bit unsigned integer that is set to either the value in the PCD PcdFmpDeviceBuildTimeLowestSupportedVersion or the value of the DTB /firmware/uefi node's fmp-lowest-supported-version property, whichever numeric value is larger.
### FMP Capsule Image Type ID GUID
The FMP Capsule Image Type ID GUID is used to uniquely identify the system FW and is reported in the ESRT.  This GUID is set by the PCD PcdSystemFmpCapsuleImageTypeIdGuid or that value can be overriden by setting the DTB /firmware/uefi node's fmp-image-type-id-guid property to a GUID string with format xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx.

**For platforms using blob FW updates, the default FMP Capsule Image Type ID GUID should always be replaced with a platform-specific GUID by updating the PCD or overriding it with the DTB property.**
