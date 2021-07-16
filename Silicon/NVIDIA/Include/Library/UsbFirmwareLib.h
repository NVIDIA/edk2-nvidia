/** @file
*
*  Copyright (c) 2019, NVIDIA CORPORATION. All rights reserved.
*
*  This program and the accompanying materials
*  are licensed and made available under the terms and conditions of the BSD License
*  which accompanies this distribution.  The full text of the license may be found at
*  http://opensource.org/licenses/bsd-license.php
*
*  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
*  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.
*
*  Portions provided under the following terms:
*  Copyright (c) 2019 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
*
*  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
*  property and proprietary rights in and to this material, related
*  documentation and any modifications thereto. Any use, reproduction,
*  disclosure or distribution of this material and related documentation
*  without an express license agreement from NVIDIA CORPORATION or
*  its affiliates is strictly prohibited.
*
*  SPDX-FileCopyrightText: Copyright (c) 2019 NVIDIA CORPORATION & AFFILIATES
*  SPDX-License-Identifier: LicenseRef-NvidiaProprietary
*
**/

#ifndef __USB_FIRMWARE_LIB_H__
#define __USB_FIRMWARE_LIB_H__

#include <Uefi/UefiBaseType.h>

extern unsigned char xusb_sil_prod_fw[];
extern unsigned char xusb_sil_rel_fw[];
extern unsigned int  xusb_sil_prod_fw_len;
extern unsigned int  xusb_sil_rel_fw_len;

#endif //__USB_FIRMWARE_LIB_H__
