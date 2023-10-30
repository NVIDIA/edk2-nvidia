#  This file contains defines that are global to all NVIDIA platforms.
#
#  SPDX-FileCopyrightText: Copyright (c) 2023 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent


[Defines]

  # Skip edk2's debug macro plugin.  It assumes a packaging structure that
  # doesn't match edk2-nvidia.  Specifically, it requires a dec file in
  # edk2-nvidia/Platform.
  DISABLE_DEBUG_MACRO_CHECK=TRUE

