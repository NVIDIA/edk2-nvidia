/** @file

  Copyright (c) 2023, NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef _PHY_MGBE_H__
#define _PHY_MGBE_H__

#define PHY_AQR113C_OUI   0x31C31C12
#define PHY_AQR113_OUI    0x31C31C42

/*
 * @brief Configure MGBE PHY
 *
 * @param PhyDriver PHY object
 *
 * @return EFI_SUCCESS if success, specific error if fails
 */
EFI_STATUS
EFIAPI
PhyMGBEConfig (
  IN  PHY_DRIVER  *PhyDriver
  );

/*
 * @brief Start auto-negotiation from MGBE PHY
 *
 * @param PhyDriver PHY object
 *
 * @return EFI_SUCCESS if success, specific error if fails
 */
EFI_STATUS
EFIAPI
PhyMGBEStartAutoNeg (
  IN  PHY_DRIVER  *PhyDriver
  );

/*
 * @brief Check auto-negotiation completion status from MGBE PHY
 *
 * @param PhyDriver PHY object
 *
 * @return EFI_SUCCESS if success, specific error if fails
 */
EFI_STATUS
EFIAPI
PhyMGBECheckAutoNeg (
  IN  PHY_DRIVER  *PhyDriver
  );

/*
 * @brief Detect link between MGBE PHY and MAC
 *
 * @param phy PHY object
 */
VOID
EFIAPI
PhyMGBEDetectLink (
  IN  PHY_DRIVER  *PhyDriver
  );

#endif /* _PHY_MGBE_H__ */
