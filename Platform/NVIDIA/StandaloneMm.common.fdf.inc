#
#  Copyright (c) 2022, NVIDIA CORPORATION. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#


  #
  # MM Core
  #
  INF StandaloneMmPkg/Core/StandaloneMmCore.inf

  #
  # MM CPU Driver
  #
  INF StandaloneMmPkg/Drivers/StandaloneMmCpu/StandaloneMmCpu.inf

  #
  # MM Variable Driver
  #
  INF MdeModulePkg/Universal/Variable/RuntimeDxe/VariableStandaloneMm.inf
