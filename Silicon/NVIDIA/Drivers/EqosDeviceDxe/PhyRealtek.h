/** @file

  Copyright (c) 2011 - 2019, Intel Corporaton. All rights reserved.
  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.
  Copyright (c) 2011 - 2014, ARM Limited. All rights reserved.
  Copyright (c) 2020, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#ifndef _PHY_REALTEK_H__
#define _PHY_REALTEK_H__

#define PHY_REALTEK_OUI         0x000732

/*
 * @brief Configure Realtek PHY
 *
 * @param PhyDriver PHY object
 *
 * @return EFI_SUCCESS if success, specific error if fails
 */
EFI_STATUS
EFIAPI
PhyRealtekConfig (
  IN  PHY_DRIVER   *PhyDriver
  );

/*
 * @brief Start auto-negotiation from Realtek PHY
 *
 * @param PhyDriver PHY object
 *
 * @return EFI_SUCCESS if success, specific error if fails
 */
EFI_STATUS
EFIAPI
PhyRealtekStartAutoNeg (
  IN  PHY_DRIVER   *PhyDriver
  );

/*
 * @brief Check auto-negotiation completion status from Realtek PHY
 *
 * @param PhyDriver PHY object
 *
 * @return EFI_SUCCESS if success, specific error if fails
 */
EFI_STATUS
EFIAPI
PhyRealtekCheckAutoNeg (
  IN  PHY_DRIVER   *PhyDriver
  );

/*
 * @brief Detect link between Realtek PHY and MAC
 *
 * @param phy PHY object
 */
VOID
EFIAPI
PhyRealtekDetectLink (
  IN  PHY_DRIVER   *PhyDriver
  );

#endif /* _PHY_REALTEK_H__ */
