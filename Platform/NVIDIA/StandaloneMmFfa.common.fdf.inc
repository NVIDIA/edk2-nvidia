#
#  SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#

  # MM QSPI Driver
  #
  INF Silicon/NVIDIA/Drivers/QspiControllerDxe/QspiControllerStandaloneMm.inf

  #
  # MM NorFlash Driver
  #
  INF Silicon/NVIDIA/Drivers/NorFlashDxe/NorFlashStandaloneMm.inf

  #
  # MM NOR FvB driver
  #
  INF Silicon/NVIDIA/Drivers/FvbNorFlashDxe/FvbNorFlashStandaloneMm.inf

  #
  # MM FW Partition Driver
  #
  INF Silicon/NVIDIA/Drivers/FwPartitionMmDxe/FwPartitionStandaloneMm.inf

  #
  # FTW Driver
  #
  INF MdeModulePkg/Universal/FaultTolerantWriteDxe/FaultTolerantWriteStandaloneMm.inf

  #
  # GPIO Driver
  #
  INF Silicon/NVIDIA/Drivers/TegraGpio/TegraGpioStandaloneMm.inf

  #
  # MM Core
  #
  INF Silicon/StandaloneMmPkgLegacy/Core/StandaloneMmCore.inf

  #
  # MM CPU Driver
  #
  INF Silicon/NVIDIA/StandaloneMm/Drivers/NvMmStandaloneMmCpu/StandaloneMmCpu.inf

  #
  # MM Variable Driver
  #
  INF MdeModulePkg/Universal/Variable/RuntimeDxe/VariableStandaloneMm.inf

  #
  # MM User Authentication Driver
  #
  INF Silicon/NVIDIA/Drivers/UserAuthenticationDxeMm/UserAuthenticationMm.inf
