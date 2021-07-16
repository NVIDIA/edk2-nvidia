/** @file

  Copyright (c) 2011 - 2019, Intel Corporaton. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  The original software modules are licensed as follows:

  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.
  Copyright (c) 2011 - 2014, ARM Limited. All rights reserved.
  Copyright (c) 2020, NVIDIA Corporation. All rights reserved.


  SPDX-License-Identifier: BSD-2-Clause-Patent

  Portions provided under the following terms:
  Copyright (c) 2020 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  NVIDIA CORPORATION, its affiliates and licensors retain all intellectual
  property and proprietary rights in and to this material, related
  documentation and any modifications thereto. Any use, reproduction,
  disclosure or distribution of this material and related documentation
  without an express license agreement from NVIDIA CORPORATION or
  its affiliates is strictly prohibited.

  SPDX-FileCopyrightText: Copyright (c) 2020 NVIDIA CORPORATION & AFFILIATES
  SPDX-License-Identifier: LicenseRef-NvidiaProprietary

**/


#ifndef _PHY_REALTEK_H__
#define _PHY_REALTEK_H__

#define PHY_REALTEK_OUI         0x000732

/*
 * @brief Configure Realtek PHY
 *
 * @param PhyDriver PHY object
 * @param MacBaseAddress Base address of MAC
 *
 * @return EFI_SUCCESS if success, specific error if fails
 */
EFI_STATUS
EFIAPI
PhyRealtekConfig (
  IN  PHY_DRIVER   *PhyDriver,
  IN  UINTN        MacBaseAddress
  );

/*
 * @brief Start auto-negotiation from Realtek PHY
 *
 * @param PhyDriver PHY object
 * @param MacBaseAddress Base address of MAC
 *
 * @return EFI_SUCCESS if success, specific error if fails
 */
EFI_STATUS
EFIAPI
PhyRealtekStartAutoNeg (
  IN  PHY_DRIVER   *PhyDriver,
  IN  UINTN        MacBaseAddress
  );

/*
 * @brief Check auto-negotiation completion status from Realtek PHY
 *
 * @param PhyDriver PHY object
 * @param MacBaseAddress Base address of MAC
 *
 * @return EFI_SUCCESS if success, specific error if fails
 */
EFI_STATUS
EFIAPI
PhyRealtekCheckAutoNeg (
  IN  PHY_DRIVER   *PhyDriver,
  IN  UINTN        MacBaseAddress
  );

/*
 * @brief Detect link between Realtek PHY and MAC
 *
 * @param phy PHY object
 * @param MacBaseAddress Base address of MAC
 */
VOID
EFIAPI
PhyRealtekDetectLink (
  IN  PHY_DRIVER   *PhyDriver,
  IN  UINTN        MacBaseAddress
  );

#endif /* _PHY_REALTEK_H__ */
