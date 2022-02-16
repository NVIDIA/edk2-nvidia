/** @file

  Copyright (c) 2011 - 2019, Intel Corporaton. All rights reserved.
  Copyright (c) 2020, NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.
  Copyright (c) 2011 - 2014, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#ifndef _PHY_MARVELL_H__
#define _PHY_MARVELL_H__

#define PHY_MARVELL_OUI         0x005043

/*
 * @brief Configure Marvell PHY
 *
 * @param PhyDriver PHY object
 *
 * @return EFI_SUCCESS if success, specific error if fails
 */
EFI_STATUS
EFIAPI
PhyMarvellConfig (
  IN  PHY_DRIVER   *PhyDriver
  );

/*
 * @brief Start auto-negotiation from Marvell PHY
 *
 * @param PhyDriver PHY object
 *
 * @return EFI_SUCCESS if success, specific error if fails
 */
EFI_STATUS
EFIAPI
PhyMarvellStartAutoNeg (
  IN  PHY_DRIVER   *PhyDriver
  );

/*
 * @brief Check auto-negotiation completion status from Marvell PHY
 *
 * @param PhyDriver PHY object
 *
 * @return EFI_SUCCESS if success, specific error if fails
 */
EFI_STATUS
EFIAPI
PhyMarvellCheckAutoNeg (
  IN  PHY_DRIVER   *PhyDriver
  );

/*
 * @brief Detect link between Marvell PHY and MAC
 *
 * @param phy PHY object
 */
VOID
EFIAPI
PhyMarvellDetectLink (
  IN  PHY_DRIVER   *PhyDriver
  );

#endif /* _PHY_MARVELL_H__ */
