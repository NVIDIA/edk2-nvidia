#
#  SPDX-FileCopyrightText: Copyright (c) 2022-2024 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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
  INF Silicon/NVIDIA/Drivers/NorFlashDxe/NorFlashStandaloneMmBlob.inf

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
  # MM MCTP/ERoT Drivers
  #
  INF Silicon/NVIDIA/Drivers/ErotQspiDxe/ErotQspiStmm.inf
  INF Silicon/NVIDIA/Drivers/MctpMmDxe/MctpStandaloneMm.inf

  #
  # MM Core
  #
  INF StandaloneMmPkg/Core/StandaloneMmCore.inf

  #
  # MM CPU Driver
  #
  INF Silicon/NVIDIA/StandaloneMm/Drivers/NvMmStandaloneMmCpu/StandaloneMmCpu.inf

  #
  # MM Variable Driver
  #
  INF MdeModulePkg/Universal/Variable/RuntimeDxe/VariableStandaloneMm.inf

  #
  # Sequential Record Flash Driver
  #
  INF Silicon/NVIDIA/Drivers/SequentialRecordStMm/SequentialRecordStorage.inf

  #
  # CMET Record Storage Driver
  #
  INF Silicon/NVIDIA/Drivers/SequentialRecordStMm/CmetRecordStorage.inf

  #
  # Sequential Record client driver
  #
  INF Silicon/NVIDIA/Drivers/SequentialRecordStMm/SequentialRecordComm.inf


  #
  # MM Error Serialization Driver
  #
  INF Silicon/NVIDIA/Server/TH500/Drivers/ErrorSerializationMmDxe/ErrorSerializationMm.inf

  #
  # MM User Authentication Driver
  #
  INF Silicon/NVIDIA/Drivers/UserAuthenticationDxeMm/UserAuthenticationMm.inf

################################################################################
#
# Rules are use with the [FV] section's module INF type to define
# how an FFS file is created for a given INF file. The following Rule are the default
# rules for the different module type. User can add the customized rules to define the
# content of the FFS file.
#
################################################################################


############################################################################
# Example of a DXE_DRIVER FFS file with a Checksum encapsulation section   #
############################################################################
#
#[Rule.Common.DXE_DRIVER]
#  FILE DRIVER = $(NAMED_GUID) {
#    DXE_DEPEX    DXE_DEPEX               Optional $(INF_OUTPUT)/$(MODULE_NAME).depex
#    COMPRESS PI_STD {
#      GUIDED {
#        PE32     PE32                    $(INF_OUTPUT)/$(MODULE_NAME).efi
#        UI       STRING="$(MODULE_NAME)" Optional
#        VERSION  STRING="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
#      }
#    }
#  }
#
############################################################################

[Rule.Common.MM_CORE_STANDALONE]
  FILE SEC = $(NAMED_GUID) FIXED {
    PE32  PE32 Align = Auto             $(INF_OUTPUT)/$(MODULE_NAME).efi
  }

[Rule.Common.MM_STANDALONE]
  FILE MM_STANDALONE = $(NAMED_GUID) {
    SMM_DEPEX SMM_DEPEX Optional       $(INF_OUTPUT)/$(MODULE_NAME).depex
    PE32      PE32                     $(INF_OUTPUT)/$(MODULE_NAME).efi
    UI        STRING="$(MODULE_NAME)" Optional
    VERSION   STRING="$(INF_VERSION)" Optional BUILD_NUM=$(BUILD_NUMBER)
  }
