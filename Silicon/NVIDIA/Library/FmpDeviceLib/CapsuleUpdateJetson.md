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

## Special Features
### Single Image Capsule Update
The Capsule Update feature updates all system firmware images.  For development purposes, a capsule with a single firmware image may also be used to update that image in one boot chain.

The enable this support, the PCD gNVIDIATokenSpaceGuid.PcdFmpSingleImageUpdate must be set to TRUE.  Before a single image capsule is processed, the FmpCapsuleSinglePartitionChain UEFI variable (GUID=781e084c-a330-417c-b678-38e696380cb9) must be set to the desired boot chain to be updated (0=A, 1=B).  An active boot chain firmware image can only be updated if the PCD gNVIDIATokenSpaceGuid.PcdOverwriteActiveFwPartition is set to TRUE.

