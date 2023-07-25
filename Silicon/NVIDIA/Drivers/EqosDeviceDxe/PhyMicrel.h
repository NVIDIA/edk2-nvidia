/** @file

  Copyright (c) 2011 - 2019, Intel Corporaton. All rights reserved.
  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.
  Copyright (c) 2011 - 2014, ARM Limited. All rights reserved.
  Copyright (c) 2020, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2023, Connect Tech Inc. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _PHY_MICREL_H__
#define _PHY_MICREL_H__

#define PHY_MICREL_OUI  0x000885

/*
 * @brief Configure Micrel PHY
 *
 * @param PhyDriver PHY object
 *
 * @return EFI_SUCCESS if success, specific error if fails
 */
EFI_STATUS
EFIAPI
PhyMicrelConfig (
  IN  PHY_DRIVER  *PhyDriver
  );

/*
 * @brief Start auto-negotiation from Micrel PHY
 *
 * @param PhyDriver PHY object
 *
 * @return EFI_SUCCESS if success, specific error if fails
 */
EFI_STATUS
EFIAPI
PhyMicrelStartAutoNeg (
  IN  PHY_DRIVER  *PhyDriver
  );

/*
 * @brief Check auto-negotiation completion status from Micrel PHY
 *
 * @param PhyDriver PHY object
 *
 * @return EFI_SUCCESS if success, specific error if fails
 */
EFI_STATUS
EFIAPI
PhyMicrelCheckAutoNeg (
  IN  PHY_DRIVER  *PhyDriver
  );

/*
 * @brief Detect link between Micrel PHY and MAC
 *
 * @param phy PHY object
 */
VOID
EFIAPI
PhyMicrelDetectLink (
  IN  PHY_DRIVER  *PhyDriver
  );

#endif /* _PHY_MICREL_H__ */
