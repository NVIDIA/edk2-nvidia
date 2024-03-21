/** @file
  Server Power Control Protocol

  SPDX-FileCopyrightText: Copyright (c) 2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.

  SPDX-License-Identifier: BSD-2-Clause-Patent

**/

#ifndef __SERVER_POWER_CONTROL_PROTOCOL_H__
#define __SERVER_POWER_CONTROL_PROTOCOL_H__

#include <Uefi/UefiSpec.h>

typedef struct _NVIDIA_SERVER_POWER_CONTROL_PROTOCOL NVIDIA_SERVER_POWER_CONTROL_PROTOCOL;

typedef enum {
  ServerPowerControlInputPowerCapping50ms,
  ServerPowerControlInputPowerCapping1s,
  ServerPowerControlInputPowerCapping5s,
  ServerPowerControlInputPowerCappingMax
} NVIDIA_SERVER_POWER_CONTROL_SETTING;

/**
  This function configures the power control as per user setting

  @param[in]  This                 The instance of NVIDIA_SERVER_POWER_CONTROL_PROTOCOL.
  @param[in]  PowerControlSetting  Server power control setting.

  @return EFI_SUCCESS              Server power control setting configured.
  @return Others                   Server power control setting not configured.
**/
typedef
EFI_STATUS
(EFIAPI *CONFIGURE_POWER_CONTROL)(
  IN     NVIDIA_SERVER_POWER_CONTROL_PROTOCOL  *This,
  IN     NVIDIA_SERVER_POWER_CONTROL_SETTING   PowerControlSetting
  );

struct _NVIDIA_SERVER_POWER_CONTROL_PROTOCOL {
  CONFIGURE_POWER_CONTROL    ConfigurePowerControl;
};

extern EFI_GUID  gServerPowerControlProtocolGuid;

#endif
