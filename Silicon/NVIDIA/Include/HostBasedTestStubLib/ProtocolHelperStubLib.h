/** @file
  Test hooks for ProtocolHelperStubLib (host-based unit tests).

  SPDX-FileCopyrightText: Copyright (c) 2026 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef PROTOCOL_HELPER_STUB_LIB_H_
#define PROTOCOL_HELPER_STUB_LIB_H_

#include <Uefi.h>

/**
  Reset stub state to defaults suitable for unit tests:
  ProtocolHelperGetParentProtocolFromHandle returns EFI_SUCCESS for the
  NVIDIA PCI root bridge configuration protocol, with C2C init marked done.
**/
VOID
EFIAPI
ProtocolHelperStubReset (
  VOID
  );

/**
  Force ProtocolHelperGetParentProtocolFromHandle to return Status (for any
  matching protocol query implemented by the stub).
**/
VOID
EFIAPI
ProtocolHelperStubSetGetParentProtocolFromHandleStatus (
  IN EFI_STATUS  Status
  );

/**
  When the stub returns a root bridge configuration protocol instance, set
  its C2cInitDone field to this value.
**/
VOID
EFIAPI
ProtocolHelperStubSetRootBridgeC2cInitDone (
  IN BOOLEAN  C2cInitDone
  );

#endif // PROTOCOL_HELPER_STUB_LIB_H_
