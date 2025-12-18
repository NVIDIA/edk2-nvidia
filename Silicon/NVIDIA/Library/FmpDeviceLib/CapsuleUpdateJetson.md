# Capsule Update documentation for NVIDIA Jetson platforms
Feature Name: Capsule Update

PI Phase(s) Supported: DXE

SMM Required? Yes, except on platforms without QSPI flash.

## Purpose
The Capsule Update feature provides system firmware update capabilities in accordance with the UEFI specification.  Delivery of capsules via file on mass storage device is supported.  This requires the capsule to be present in the EFI System Partition (ESP) in the \EFI\UpdateCapsule directory and the EFI_OS_INDICATIONS_FILE_CAPSULE_DELIVERY_SUPPORTED bit to be set in the OsIndications UEFI variable when the system is booted.  Capsules may also be processed using the CapsuleApp.efi UEFI application.

## High-Level Theory of Operation
Capsule updates use the Firmware Management Protocol (FMP) to access firmware.  The edk2/FmpDevicePkg/FmpDxe module uses a vendor-implemented FmpDeviceLib Library to produce the FMP.  The FmpDeviceLib uses platform-specific drivers to access the system firmware.  The FMP reports system firmware and capsule update information in the EFI System Resource Table (ESRT) as described in the UEFI specification.

NVIDIA system firmware supports multiple boot chains of firmware images.  Typically, two boot chains are implemented for redundancy and if the current boot chain fails, the other boot chain is automatically used.  The Capsule Update feature always updates the inactive boot chain's firmware images.  After all images are successfully written, the Boot Control Table is updated to boot the new firmware as the active boot chain.

## Firmware Volumes
UEFI_NS
UEFI_MM

## Modules
### edk2/FmpDevicePkg/FmpDxe
Description:
DXE module that produces the Firmware Management Protocol.
### edk2-nvidia/Silicon/NVIDIA/Library/FmpDeviceLib
Description:
NVIDIA's implementation of the FmpDeviceLib Library that is used by the edk2/FmpDevicePkg/FmpDxe DXE module on Jetson platforms.
### edk2-nvidia/Silicon/NVIDIA/Drivers/FwImageDxe
Description:
DXE module that produces the Firmware Image Protocol used to access system firmware partitions.
### edk2-nvidia/Silicon/NVIDIA/Library/FwImageLib
Description:
Library to simplify use of Firmware Image Protocol instances.
### edk2-nvidia/Silicon/NVIDIA/Drivers/FwPartitionBlockIoDxe
Description:
DXE module that produces the Firmware Partition Protocol for NS Block IO partitions.
### edk2-nvidia/Silicon/NVIDIA/Drivers/FwPartitionMmDxe
Description:
DXE module that produces the Firmware Partition Protocol for MM partitions.  This driver uses the MM Communication Protocol to access MM NOR Flash partitions.
### edk2-nvidia/Silicon/NVIDIA/Drivers/FwPartitionNorFlashDxe
Description:
DXE module that produces the Firmware Partition Protocol for NS NOR Flash partitions.
### edk2-nvidia/Silicon/NVIDIA/Library/FwPackageLib
Description:
Library to access system firmware images within a capsule's FMP data area.
### edk2-nvidia/Silicon/NVIDIA/Library/FmpParamLib
Description:
Library to access FMP parameters from PCDs and/or DTB.

## Parameters and Options
### FMP Lowest Supported Version
The FMP Lowest Supported Version is a 32-bit unsigned integer that is set to either the value in the PCD PcdFmpDeviceBuildTimeLowestSupportedVersion or the value of the DTB /firmware/uefi node's fmp-lowest-supported-version property, whichever numeric value is larger.
### FMP Capsule Image Type ID GUID
The FMP Capsule Image Type ID GUID is used to uniquely identify the system FW and is reported in the ESRT.  This GUID is set by the Kconfig Firmware Management Options menu "Platform ESRT System FW GUID" setting which sets the CONFIG_FMP_SYSTEM_IMAGE_TYPE_ID value used to set the PCD PcdSystemFmpCapsuleImageTypeIdGuid.  This build-time value can be overriden by setting the DTB /firmware/uefi node's fmp-image-type-id-guid property to a GUID string with format xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx.

**For Jetson platforms, the default FMP Capsule Image Type ID GUID should always be replaced with a platform-specific GUID by updating CONFIG_FMP_SYSTEM_IMAGE_TYPE_ID or overriding it with the DTB property.**

### FMP PKCS7 Certificates
FMP capsules are authenticated using PKCS7 certificates.  UEFI supports a list of certificates to aid in key management and revocation.  UEFI will attempt to validate an incoming capsule with each certificate in the list. Capsule authentication fails if no certificate in the list can authenticate the signature.

When FIRMWARE_CAPSULE_SUPPORTED is configured, the Kconfig Firmware Options menu "FMP certificates to authenticate capsule payload" setting configures the source of the certificates.  Choices include using the EDK2 test certificate file (**DO NOT USE FOR PRODUCTION**), providing a production certificates file path, or configuring UEFI to retrieve certificates from the DTB /firmware/uefi node's fmp-pkcs7-cert-buffer-xdr property set to the raw bytes of the certificate list.

For details on generating certificates, see https://github.com/tianocore/tianocore.github.io/wiki/Capsule-Based-System-Firmware-Update-Generate-Keys.

The EDK2 capsule generation and signing tool is here: https://github.com/tianocore/edk2/blob/master/BaseTools/Source/Python/Capsule/GenerateCapsule.py.

## Special Features
### Single Image Capsule Update
The Capsule Update feature updates all system firmware images.  For development purposes, a capsule with a single firmware image may also be used to update that image in one boot chain.

The enable this support, the PCD gNVIDIATokenSpaceGuid.PcdFmpSingleImageUpdate must be set to TRUE.  Before a single image capsule is processed, the FmpCapsuleSinglePartitionChain UEFI variable (GUID=781e084c-a330-417c-b678-38e696380cb9) must be set to the desired boot chain to be updated (0=A, 1=B).  An active boot chain firmware image can only be updated if the PCD gNVIDIATokenSpaceGuid.PcdOverwriteActiveFwPartition is set to TRUE.

