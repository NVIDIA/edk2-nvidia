#
#  SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#
#  SPDX-License-Identifier: BSD-2-Clause-Patent
#

  !include Platform/NVIDIA/StandaloneMmFfa.common.fdf.inc

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
