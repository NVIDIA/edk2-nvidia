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
**/

#ifndef __USB_FIRMWARE_LIB_H__
#define __USB_FIRMWARE_LIB_H__

#include <Uefi/UefiBaseType.h>

extern unsigned char xusb_sil_prod_fw[];
extern unsigned char xusb_sil_rel_fw[];
extern unsigned int  xusb_sil_prod_fw_len;
extern unsigned int  xusb_sil_rel_fw_len;

#endif //__USB_FIRMWARE_LIB_H__
