/** @file

  Copyright (c) 2011 - 2019, Intel Corporaton. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

  The original software modules are licensed as follows:

  Copyright (c) 2008 - 2009, Apple Inc. All rights reserved.
  Copyright (c) 2011 - 2014, ARM Limited. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/


#ifndef _PHY_DXE_H__
#define _PHY_DXE_H__

#include <Protocol/EmbeddedGpio.h>

typedef struct _PHY_DRIVER  PHY_DRIVER;

typedef
VOID
(EFIAPI *NVIDIA_EQOS_PHY_CONFIG)(
    IN  PHY_DRIVER   *PhyDriver,
    IN  UINTN        MacBaseAddress
    );

typedef
EFI_STATUS
(EFIAPI *NVIDIA_EQOS_PHY_AUTO_NEG)(
    IN  PHY_DRIVER   *PhyDriver,
    IN  UINTN        MacBaseAddress
    );

typedef
VOID
(EFIAPI *NVIDIA_EQOS_PHY_DETECT_LINK)(
    IN  PHY_DRIVER   *PhyDriver,
    IN  UINTN        MacBaseAddress
    );

struct _PHY_DRIVER{
  UINT32                      PhyPage;
  UINT32                      PhyPageSelRegister;
  UINT32                      PhyCurrentLink;
  UINT32                      PhyOldLink;
  UINT32                      Speed;
  UINT32                      Duplex;
  EFI_HANDLE                  ControllerHandle;
  EMBEDDED_GPIO_PIN           ResetPin;
  EMBEDDED_GPIO_MODE          ResetMode0;
  EMBEDDED_GPIO_MODE          ResetMode1;
  NVIDIA_EQOS_PHY_CONFIG      Config;
  NVIDIA_EQOS_PHY_AUTO_NEG    AutoNeg;
  NVIDIA_EQOS_PHY_DETECT_LINK DetectLink;
};


#define PAGE_PHY                                      0

#define REG_PHY_CONTROL                               0
#define REG_PHY_CONTROL_RESET                         BIT15
#define REG_PHY_CONTROL_AUTO_NEGOTIATION_ENABLE       BIT12
#define REG_PHY_CONTROL_RESTART_AUTO_NEGOTIATION      BIT9

#define REG_PHY_STATUS                                1
#define REG_PHY_STATUS_AUTO_NEGOTIATION_COMPLETED    BIT12

#define REG_PHY_IDENTIFIER_1                          2

#define REG_PHY_IDENTIFIER_2                          3
#define REG_PHY_IDENTIFIER_2_WIDTH                    ((15 - 10) + 1)
#define REG_PHY_IDENTIFIER_2_SHIFT                    10

#define REG_PHY_AUTONEG_ADVERTISE                     4
#define REG_PHY_AUTONEG_ADVERTISE_100_BASE_T4         BIT9
#define REG_PHY_AUTONEG_ADVERTISE_100_BASE_TX_FULL    BIT8
#define REG_PHY_AUTONEG_ADVERTISE_100_BASE_TX_HALF    BIT7
#define REG_PHY_AUTONEG_ADVERTISE_10_BASE_T_FULL      BIT6
#define REG_PHY_AUTONEG_ADVERTISE_10_BASE_T_HALF      BIT5

#define REG_PHY_GB_CONTROL                            9
#define REG_PHY_GB_CONTROL_ADVERTISE_1000_BASE_T_FULL BIT9

/************************************************************************************************************/
#define NON_EXISTENT_ON_PRESIL                        0xDEADBEEF

#define SPEED_1000                            1000
#define SPEED_100                             100
#define SPEED_10                              10

#define DUPLEX_FULL                           1
#define DUPLEX_HALF                           0

#define LINK_UP                               1
#define LINK_DOWN                             0
#define PHY_TIMEOUT                           200000

EFI_STATUS
EFIAPI
PhyRead (
  IN PHY_DRIVER   *PhyDriver,
  IN  UINT32   Page,
  IN  UINT32   Reg,
  OUT UINT32   *Data,
  IN  UINTN    MacBaseAddress
  );


// Function to write to the MII register (PHY Access)
EFI_STATUS
EFIAPI
PhyWrite (
  IN PHY_DRIVER   *PhyDriver,
  IN UINT32   Page,
  IN UINT32   Reg,
  IN UINT32   Data,
  IN UINTN    MacBaseAddress
  );

EFI_STATUS
EFIAPI
PhyDxeInitialization (
  IN  PHY_DRIVER     *PhyDriver,
  IN  UINTN          MacBaseAddress
  );

EFI_STATUS
EFIAPI
PhySoftReset (
  IN  PHY_DRIVER    *PhyDriver,
  IN  UINTN         MacBaseAddress
  );

EFI_STATUS
EFIAPI
PhyLinkAdjustEmacConfig (
  IN  PHY_DRIVER    *PhyDriver,
  IN  UINTN         MacBaseAddress
  );

#endif /* _PHY_DXE_H__ */
