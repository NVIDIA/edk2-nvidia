#  SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
#  SPDX-License-Identifier: BSD-2-Clause-Patent

########
OVERVIEW
########
This directory contains templates used by the Kconfig for different SOC families and BUILD types.
New files dropped into this directory will automatically show up in the Kconfig menu.


################
KconfigSoc*.conf
################
These are the SOC type choices, with one file per SOC family. Each file should have the following
format:

config SOC_<FAMILY>
  bool "<FAMILY>"
  help
    Support for <FAMILY> chips
  # Hardware support present
  imply SUPPORTS_*
  ...

The option should only imply the SUPPORTS_* hardware features that can be supported by that family.
These files are NOT for actually enabling features.


######################
KconfigCommonSoc*.conf
######################
These are helper files that group together a number of hardware features that are common to multiple
families. Each file should have the following format:

config SOC_<COMMON_FAMILY_NAME>
  bool
  imply SUPPORTS_<COMMON_FEATURE>


##################
KconfigBuild*.conf
##################
These are the default BUILD type choice options, with one file per option. Each file should have the
following format:

config BUILD_<BUILDNAME>
  bool "BUILDNAME"
  help
    "<desription of the soul of the BUILDNAME build>"
  #Default features
  imply <FEATURE>
  ...

The option should only "imply" features to enable by default for that BUILD type. It should NOT
"select" features, so that the option can still be disabled in a BUILD either by the user or due to
missing dependencies.
