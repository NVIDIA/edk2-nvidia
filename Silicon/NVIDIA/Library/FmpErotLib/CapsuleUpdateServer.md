# Capsule Update documentation for NVIDIA Server platforms
Feature Name: Capsule Update

PI Phase(s) Supported: DXE

SMM Required? Yes

## Purpose
The Capsule Update feature provides system firmware update capabilities in accordance with the UEFI specification.  Delivery of capsules via file on mass storage device is supported.  This requires the capsule to be present in the EFI System Partition (ESP) in the \EFI\UpdateCapsule directory and the EFI_OS_INDICATIONS_FILE_CAPSULE_DELIVERY_SUPPORTED bit to be set in the OsIndications UEFI variable when the system is booted.  Capsules may also be processed using the CapsuleApp.efi UEFI application.

## High-Level Theory of Operation
Capsule updates use the Firmware Management Protocol (FMP) to access firmware.  The edk2/FmpDevicePkg/FmpDxe module uses a vendor-implemented FmpDeviceLib Library to produce the FMP.  The FmpDeviceLib uses platform-specific drivers to access the system firmware.  The FMP reports system firmware and capsule update information in the EFI System Resource Table (ESRT) as described in the UEFI specification.

System firmware on NVIDIA Server platforms is managed by an enhanced Root of Trust (eRoT) that implements the Platform Level Data Model (PLDM) for Firmware Update Specification (DSP0267).  The eRoT maintains a backup copy of the system firmware for recovery.

The Capsule Update feature uses the FmpDxe driver to interact with the eRoT in Management Mode (MM) using PLDM Firmware Update commands.  The PLDM commands are sent and received using the Management Component Transport Protocol (MCTP) with an NVIDIA-specific transport layer over a Quad Serial Peripheral Interface (QSPI).

## Firmware Volumes
UEFI_NS
UEFI_MM

## Modules
### edk2-nvidia/Silicon/NVIDIA/Library/ErotLib
Description:
Library of eRoT support functions.
### edk2-nvidia/Silicon/NVIDIA/Drivers/ErotQspiDxe/ErotQspiStmm.inf
Description:
Standalone MM Module that produces the MCTP protocol over QSPI for eRoT devices.
### edk2/FmpDevicePkg/FmpDxe
Description:
DXE module that produces the Firmware Management Protocol.
### edk2-nvidia/Silicon/NVIDIA/Library/FmpErotLib
Description:
NVIDIA's implementation of the FmpDeviceLib Library that is used by the edk2/FmpDevicePkg/FmpDxe DXE module on Server platforms.
### edk2-nvidia/Silicon/NVIDIA/Drivers/MctpMmDxe
Description:
DXE module that produces the MCTP Protocol bridge to MM.  This driver uses the MM Communication Protocol to access MM MCTP Protocol instances.
### edk2-nvidia/Silicon/NVIDIA/Library/PldmFwUpdateLib
Description:
Library of PLDM Firmware Update definitions and helper functions.
### edk2-nvidia/Silicon/NVIDIA/Library/PldmFwUpdatePkgLib
Description:
Library of PLDM Firmware Update Package definitions and helper functions.
### edk2-nvidia/Silicon/NVIDIA/Library/PldmFwUpdateTaskLib
Description:
Library that implements the PLDM Firmware Update Task sequence.
### edk2-nvidia/Silicon/NVIDIA/Library/FmpParamLib
Description:
Library to access FMP parameters from PCDs and/or DTB.

## Parameters and Options
### FMP Lowest Supported Version
The FMP Lowest Supported Version is a 32-bit unsigned integer that is set to either the value in the PCD PcdFmpDeviceBuildTimeLowestSupportedVersion or the value of the DTB /firmware/uefi node's fmp-lowest-supported-version property, whichever numeric value is larger.
### FMP Capsule Image Type ID GUID
The FMP Capsule Image Type ID GUID is used to uniquely identify the system FW and is reported in the ESRT.  This GUID is set by the Kconfig Firmware Management Options menu "Platform ESRT System FW GUID" setting which sets the CONFIG_FMP_SYSTEM_IMAGE_TYPE_ID value used to set the PCD PcdSystemFmpCapsuleImageTypeIdGuid.  This build-time value can be overriden by setting the DTB /firmware/uefi node's fmp-image-type-id-guid property to a GUID string with format xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx.

**For Server platforms, the default FMP Capsule Image Type ID GUID should always be replaced with a platform-specific GUID by updating CONFIG_FMP_SYSTEM_IMAGE_TYPE_ID or overriding it with the DTB property.**

### FMP PKCS7 Certificates
FMP capsules are authenticated using PKCS7 certificates.  UEFI supports a list of certificates to aid in key management and revocation.  UEFI will attempt to validate an incoming capsule with each certificate in the list. Capsule authentication fails if no certificate in the list can authenticate the signature.

When FIRMWARE_CAPSULE_SUPPORTED is configured, the Kconfig Firmware Options menu "FMP certificates to authenticate capsule payload" setting configures the source of the certificates.  Choices include using the EDK2 test certificate file (**DO NOT USE FOR PRODUCTION**), providing a production certificates file path, or configuring UEFI to retrieve certificates from the DTB /firmware/uefi node's fmp-pkcs7-cert-buffer-xdr property set to the raw bytes of the certificate list.

For details on generating certificates, see https://github.com/tianocore/tianocore.github.io/wiki/Capsule-Based-System-Firmware-Update-Generate-Keys.

The EDK2 capsule generation and signing tool is here: https://github.com/tianocore/edk2/blob/master/BaseTools/Source/Python/Capsule/GenerateCapsule.py.
