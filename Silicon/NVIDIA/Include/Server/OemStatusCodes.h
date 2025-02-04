/** @file
*
*  SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef _OEM_STATUS_CODES_H_
#define _OEM_STATUS_CODES_H_

///
/// OEM Definitions of severities
///
///@{
#define EFI_OEM_PROGRESS_MINOR  0x00000000
#define EFI_OEM_PROGRESS_MAJOR  0x80000000
///@}

///
/// OEM Progress Code Descriptions
///
///@{
#define OEM_PC_DESC_RESET_NS_VARIABLES \
        "Reset non-authenticated UEFI settings"
///@}

///
/// OEM Error Code Descriptions
///
///@{
#define OEM_EC_DESC_INVALID_PASSWORD \
        "Invalid password"

#define OEM_EC_DESC_INVALID_PASSWORD_MAX \
        "Resetting system - Password retry limit reached"

#define OEM_EC_DESC_NO_SMBIOS_TABLE \
        "No SMBIOS table installed"

#define OEM_EC_DESC_SMBIOS_TRANSFER_FAILED \
        "Failed to send SMBIOS tables to BMC"

#define OEM_EC_M2_NOT_DETECTED \
        "Failed to find any drive in slot"

#define OEM_M2_NO_EFI_PARTITION \
        "Drive has no EFI partition"

#define OEM_M2_PARTITION_NOT_FAT \
        "EFI partition is not FAT"

#define OEM_M2_NOT_NVME \
        "M.2 drive is not NVMe"

#define OEM_EC_DESC_HOST_INTERFACE_INVALID_MAC_ADDRESS \
        "Host interface : Invalid MAC address"

#define OEM_EC_DESC_HOST_INTERFACE_INVALID_IP_ADDRESS \
        "Host interface : Invalid IP address"

#define OEM_EC_DESC_HOST_INTERFACE_INVALID_SUBNET_MASK_ADDRESS \
        "Host interface : Invalid subnet mask address"

#define OEM_EC_DESC_REDFISH_SERVICE_INVALID_IP_ADDRESS \
        "Redfish service : Invalid IP address"

#define OEM_EC_DESC_REDFISH_SERVICE_INVALID_SUBNET_MASK_ADDRESS \
        "Redfish service : Invalid subnet mask address"

#define OEM_EC_DESC_REDFISH_BOOTSTRAP_CREDENTIAL \
        "Fail to get Redfish credential from BMC. Status = %r"

#define OEM_EC_DESC_REDFISH_CONFIG_CHANGED_AND_REBOOT \
        "System configuration is changed via Redfish. Reboot system"

#define OEM_EC_DESC_TPM_INACCESSIBLE \
        "TPM inaccessible"

#define OEM_EC_DESC_TPM_NOT_INITIALIZED \
        "TPM not initialized"

#define OEM_EC_DESC_TPM_PCR_BANK_NOT_SUPPORTED \
        "TPM PCR bank not supported - 0x%02X"

#define OEM_EC_DESC_TPM_SELF_TEST_FAILED \
        "TPM self test failed"

#define OEM_EC_DESC_TPM_PPI_EXECUTE \
        "Execute TPM PPI command %2d"

#define OEM_EC_DESC_TPM_CLEAR_FAILED \
        "TPM Clear failed"

#define OEM_EC_DESC_SECURE_BOOT_ENABLED \
        "Secure boot is enabled"

#define OEM_EC_DESC_SECURE_BOOT_DISABLED \
        "Secure boot is disabled"

#define OEM_EC_DESC_SECURE_BOOT_FAILURE \
        "Secure boot failure "

#define OEM_EC_DESC_C2C_INIT_FAILULE \
        "S%uC%u C2C init failed - 0x%02X"

///@}

#endif
