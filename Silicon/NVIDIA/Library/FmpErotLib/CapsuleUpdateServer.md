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
