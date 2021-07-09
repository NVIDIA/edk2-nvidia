/** @file

  Copyright (c) 2021, NVIDIA Corporation. All rights reserved.
  Copyright (c) 2017-2018, Arm Limited. All rights reserved.

  This program and the accompanying materials
  are licensed and made available under the terms and conditions of the BSD License
  which accompanies this distribution.  The full text of the license may be found at
  http://opensource.org/licenses/bsd-license.php

  THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,
  WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.

  Portions provided under the following terms:
  Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2021 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

  System Control and Management Interface V1.0
    http://infocenter.arm.com/help/topic/com.arm.doc.den0056a/
    DEN0056A_System_Control_and_Management_Interface.pdf
**/
#ifndef SCMI_DXE_H_
#define SCMI_DXE_H_

#define MAX_VENDOR_LEN       SCMI_MAX_STR_LEN

/** Pointer to protocol initialization function.

  @param[in]  Handle  A pointer to the EFI_HANDLE on which the protocol
                      interface is to be installed.

  @retval EFI_SUCCESS  Protocol interface installed successfully.
**/
typedef
EFI_STATUS
(EFIAPI *SCMI_PROTOCOL_INIT_FXN)(
  IN  EFI_HANDLE  *Handle
  );

#endif /* SCMI_DXE_H_ */
