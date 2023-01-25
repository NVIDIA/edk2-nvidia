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

#endif
