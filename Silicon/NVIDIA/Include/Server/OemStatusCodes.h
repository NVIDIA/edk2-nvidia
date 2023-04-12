/** @file
*
*  Copyright (c) 2023, NVIDIA CORPORATION. All rights reserved.
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
///@}

#endif
